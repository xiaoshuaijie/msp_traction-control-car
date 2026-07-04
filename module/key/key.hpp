#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mspm0_gpio.hpp"
#include "ti_msp_dl_config.h"

namespace Module
{

/**
 * @brief K1-K4 四路上拉按键读取器。
 *
 * Key 封装 SysConfig 生成的 KEY_k1 到 KEY_k4 四个 GPIO 输入。硬件按键采用
 * 上拉输入：未按下时为高电平，按下时被拉低。因此 State::pressed_mask 使用
 * 低电平有效转换后的逻辑状态，位为 1 表示对应按键稳定按下。
 *
 * 位映射固定为：
 * - 第 0 位：K1
 * - 第 1 位：K2
 * - 第 2 位：K3
 * - 第 3 位：K4
 */
class Key
{
 public:
  /**
   * @brief 按键编号，顺序与 State 掩码位一致。
   */
  enum KeyId : size_t
  {
    kK1 = 0,
    kK2,
    kK3,
    kK4,
    kKeyCount
  };

  /**
   * @brief 按键消抖配置。
   */
  struct Config
  {
    /**
     * @brief 原始电平连续保持多少毫秒后，才更新为稳定状态。
     */
    uint32_t debounce_ms = 20;
  };

  /**
   * @brief 一次按键更新得到的状态快照。
   */
  struct State
  {
    /**
     * @brief 当前直接读取到的 GPIO 电平掩码。
     *
     * 位为 1 表示对应按键当前为高电平，位为 0 表示当前为低电平。该字段未经消抖，
     * 适合调试硬件接线和观察抖动。
     */
    uint8_t raw_mask = kAllReleasedMask;

    /**
     * @brief 消抖后的稳定 GPIO 电平掩码。
     *
     * 位含义与 raw_mask 相同，但只有当原始电平连续保持 debounce_ms 后才更新。
     */
    uint8_t stable_raw_mask = kAllReleasedMask;

    /**
     * @brief 消抖后的稳定按下掩码。
     *
     * 低电平有效：位为 1 表示对应按键稳定按下。
     */
    uint8_t pressed_mask = 0;

    /**
     * @brief 判断是否有任意按键稳定按下。
     */
    bool AnyPressed() const { return pressed_mask != 0U; }

    /**
     * @brief 查询某个按键是否稳定按下。
     *
     * @param index 从 0 开始的按键下标。0 对应 K1，3 对应 K4。
     * @return 下标有效且对应按键稳定按下时返回 true。
     */
    bool IsPressed(size_t index) const
    {
      return index < kKeyCount ? ((pressed_mask & (1U << index)) != 0U) : false;
    }
  };

  /**
   * @brief 默认按键掩码，四个按键都处于高电平未按下状态。
   */
  static constexpr uint8_t kAllReleasedMask = 0x0FU;

  Key() : Key(Config{}) {}

  /**
   * @brief 构造对象并配置 K1-K4 为上拉输入。
   *
   * @param config 消抖配置。
   */
  explicit Key(const Config& config)
      : config_(config),
        keys_{{
            LibXR::MSPM0GPIO(KEY_PORT, KEY_k1_PIN, KEY_k1_IOMUX),
            LibXR::MSPM0GPIO(KEY_PORT, KEY_k2_PIN, KEY_k2_IOMUX),
            LibXR::MSPM0GPIO(KEY_PORT, KEY_k3_PIN, KEY_k3_IOMUX),
            LibXR::MSPM0GPIO(KEY_PORT, KEY_k4_PIN, KEY_k4_IOMUX),
        }}
  {
    for (auto& key : keys_)
    {
      key.SetConfig(kInputConfig);
    }
  }

  /**
   * @brief 按当前时间更新消抖状态。
   *
   * 第一次调用会直接以当前 raw 电平作为稳定状态，避免上电后必须等待消抖时间。
   * 后续每个按键独立消抖：原始电平变化后，连续保持 debounce_ms 才写入稳定状态。
   *
   * @param now_ms 当前毫秒时间，推荐传入 LibXR::Thread::GetTime()。
   * @return 最新状态快照。
   */
  State Update(uint32_t now_ms)
  {
    const uint8_t raw = ReadRawMask();
    state_.raw_mask = raw;

    if (!initialized_)
    {
      initialized_ = true;
      candidate_raw_mask_ = raw;
      candidate_since_ms_.fill(now_ms);
      state_.stable_raw_mask = raw;
      state_.pressed_mask = PressedFromRaw(raw);
      return state_;
    }

    for (size_t i = 0; i < kKeyCount; ++i)
    {
      const uint8_t bit = static_cast<uint8_t>(1U << i);
      const bool raw_high = (raw & bit) != 0U;
      const bool candidate_high = (candidate_raw_mask_ & bit) != 0U;
      const bool stable_high = (state_.stable_raw_mask & bit) != 0U;

      if (raw_high != candidate_high)
      {
        SetMaskBit(candidate_raw_mask_, bit, raw_high);
        candidate_since_ms_[i] = now_ms;
      }
      else if (raw_high != stable_high &&
               static_cast<uint32_t>(now_ms - candidate_since_ms_[i]) >=
                   config_.debounce_ms)
      {
        SetMaskBit(state_.stable_raw_mask, bit, raw_high);
      }
    }

    state_.pressed_mask = PressedFromRaw(state_.stable_raw_mask);
    return state_;
  }

  /**
   * @brief 获取最近一次消抖后的状态，不重新读取 GPIO。
   */
  State GetState() const { return state_; }

  /**
   * @brief 直接读取当前 GPIO 原始电平掩码。
   *
   * @return 位为 1 表示高电平，位为 0 表示低电平。
   */
  uint8_t ReadRawMask()
  {
    uint8_t raw = 0;
    for (size_t i = 0; i < keys_.size(); ++i)
    {
      if (keys_[i].Read())
      {
        raw |= static_cast<uint8_t>(1U << i);
      }
    }
    return static_cast<uint8_t>(raw & kAllReleasedMask);
  }

  /**
   * @brief 获取消抖后的稳定按下掩码。
   */
  uint8_t ReadPressedMask() const { return state_.pressed_mask; }

  /**
   * @brief 查询是否有任意按键稳定按下。
   */
  bool AnyPressed() const { return state_.AnyPressed(); }

  /**
   * @brief 查询指定按键是否稳定按下。
   */
  bool IsPressed(KeyId id) const { return state_.IsPressed(id); }

  /**
   * @brief 清空消抖历史。
   *
   * Reset() 不读取 GPIO。下一次 Update(now_ms) 会重新以当时的 raw 电平初始化稳定状态。
   */
  void Reset()
  {
    initialized_ = false;
    candidate_raw_mask_ = kAllReleasedMask;
    candidate_since_ms_.fill(0);
    state_ = {};
  }

 private:
  /**
   * @brief K1-K4 共用的 GPIO 输入配置。
   */
  static constexpr LibXR::GPIO::Configuration kInputConfig = {
      LibXR::GPIO::Direction::INPUT, LibXR::GPIO::Pull::UP};

  static uint8_t PressedFromRaw(uint8_t raw)
  {
    return static_cast<uint8_t>((~raw) & kAllReleasedMask);
  }

  static void SetMaskBit(uint8_t& mask, uint8_t bit, bool high)
  {
    if (high)
    {
      mask |= bit;
    }
    else
    {
      mask &= static_cast<uint8_t>(~bit);
    }
  }

  Config config_{};
  std::array<LibXR::MSPM0GPIO, kKeyCount> keys_;
  State state_{};
  uint8_t candidate_raw_mask_ = kAllReleasedMask;
  std::array<uint32_t, kKeyCount> candidate_since_ms_{};
  bool initialized_ = false;
};

}  // namespace Module
