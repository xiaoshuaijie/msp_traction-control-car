#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mspm0_gpio.hpp"
#include "ti_msp_dl_config.h"

namespace Module
{

/**
 * @brief 8 路循迹传感器读取器。
 *
 * LineTracker 封装 SysConfig 生成的 Line_OUT1 到 Line_OUT8 这 8 个 GPIO
 * 输入。该类采用纯头文件形式，使用时只需要包含这个 C++ 头文件，不需要
 * 再把单独的 line.cpp 加入构建系统。
 *
 * 所有掩码统一使用以下位映射：
 * - 第 0 位：Line_OUT1
 * - 第 1 位：Line_OUT2
 * - ...
 * - 第 7 位：Line_OUT8
 *
 * 传感器按低电平有效处理：某一路 GPIO 读到低电平时，表示该通道检测到黑线。
 */
class LineTracker
{
 public:
  /**
   * @brief 一次轮询得到的全部循迹通道状态快照。
   *
   * raw_mask 直接保存 GPIO 的电平状态。black_mask 则保存经过低电平有效转换后的
   * 逻辑检测结果，也就是“当前通道是否压在黑线上”。
   */
  struct State
  {
    /**
     * @brief 原始 GPIO 输入电平掩码。
     *
     * 某一位为 1 表示对应 GPIO 输入为高电平，为 0 表示低电平。这个掩码适合用于
     * 调试接线或观察低电平有效转换之前的真实电气信号。
     */
    uint8_t raw_mask = 0;

    /**
     * @brief 逻辑黑线检测掩码。
     *
     * 某一位为 1 表示对应传感器通道当前检测到黑线。由于传感器是低电平有效，
     * 当原始 GPIO 输入为低电平时，这里的对应位会被置 1。
     */
    uint8_t black_mask = 0;

    /**
     * @brief 判断是否有任意一路传感器检测到黑线。
     *
     * @return black_mask 中至少有一位被置 1 时返回 true。
     */
    bool AnyOnBlack() const { return black_mask != 0U; }

    /**
     * @brief 查询某一路通道的逻辑黑线检测状态。
     *
     * @param index 从 0 开始的通道下标。0 对应 Line_OUT1，7 对应 Line_OUT8。
     * @return 通道下标有效且该通道当前检测到黑线时返回 true。
     */
    bool IsOnBlack(size_t index) const
    {
      return index < kSensorCount ? ((black_mask & (1U << index)) != 0U) : false;
    }
  };

  /**
   * @brief 循迹模块使用的 GPIO 通道数量。
   */
  static constexpr size_t kSensorCount = 8;

  /**
   * @brief 构造对象并配置全部 8 路传感器 GPIO 输入。
   *
   * 构造函数使用 ti_msp_dl_config.h 中由 SysConfig 生成的 Line_OUTx_PORT、
   * Line_OUTx_PIN 和 Line_OUTx_IOMUX 符号，然后把每个 GPIO 配置为上拉输入。
   */
  LineTracker();

  /**
   * @brief 读取全部传感器通道一次。
   *
   * @return State 快照，包含原始电平掩码和低电平有效转换后的黑线检测掩码。
   */
  State ReadState();

  /**
   * @brief 只读取逻辑黑线检测掩码。
   *
   * @return 8 位低电平有效转换后的掩码，位为 1 表示“在黑线上”。
   */
  uint8_t ReadBlackMask() { return ReadState().black_mask; }

  /**
   * @brief 读取单个通道并返回它的逻辑黑线检测状态。
   *
   * @param index 从 0 开始的通道下标。越界下标会返回 false。
   * @return 选中的通道当前检测到黑线时返回 true。
   */
  bool IsOnBlack(size_t index) { return ReadState().IsOnBlack(index); }

 private:
  /**
   * @brief 所有循迹通道共用的 GPIO 配置。
   *
   * 上拉输入模式与低电平有效的传感器逻辑匹配：空闲或高电平状态表示未检测到黑线，
   * 被拉低时表示检测到黑线。
   */
  static constexpr LibXR::GPIO::Configuration kInputConfig = {
      LibXR::GPIO::Direction::INPUT, LibXR::GPIO::Pull::UP};

  /**
   * @brief Line_OUT1 到 Line_OUT8 对应的 GPIO 对象。
   *
   * 数组顺序刻意与 raw_mask 和 black_mask 的位顺序保持一致，因此 sensors_[0]
   * 对应第 0 位和 Line_OUT1。
   */
  std::array<LibXR::MSPM0GPIO, kSensorCount> sensors_;
};

inline LineTracker::LineTracker()
    : sensors_{{
          // 保持初始化顺序与上面的位映射一致：OUT1 -> 第 0 位，OUT8 -> 第 7 位。
          LibXR::MSPM0GPIO(Line_OUT1_PORT, Line_OUT1_PIN, Line_OUT1_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT2_PORT, Line_OUT2_PIN, Line_OUT2_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT3_PORT, Line_OUT3_PIN, Line_OUT3_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT4_PORT, Line_OUT4_PIN, Line_OUT4_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT5_PORT, Line_OUT5_PIN, Line_OUT5_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT6_PORT, Line_OUT6_PIN, Line_OUT6_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT7_PORT, Line_OUT7_PIN, Line_OUT7_IOMUX),
          LibXR::MSPM0GPIO(Line_OUT8_PORT, Line_OUT8_PIN, Line_OUT8_IOMUX),
      }}
{
  for (auto& sensor : sensors_)
  {
    sensor.SetConfig(kInputConfig);
  }
}

inline LineTracker::State LineTracker::ReadState()
{
  State state = {};

  for (size_t index = 0; index < sensors_.size(); ++index)
  {
    const bool raw_high = sensors_[index].Read();
    if (raw_high)
    {
      // raw_mask 保留电气输入电平：1 表示 GPIO 高电平。
      state.raw_mask |= static_cast<uint8_t>(1U << index);
    }
    else
    {
      // 低电平有效转换：GPIO 低电平表示该通道检测到黑线。
      state.black_mask |= static_cast<uint8_t>(1U << index);
    }
  }

  return state;
}

}  // namespace Module
