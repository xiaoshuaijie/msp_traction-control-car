#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mspm0_gpio.hpp"
#include "libxr_cb.hpp"
#include "timebase.hpp"
#include "ti_msp_dl_config.h"
#include "mspm0_timebase.hpp"
namespace Module
{

/**
 * @brief 单个电机的正交编码器解码器（4 倍频）。
 *
 * 每个解码器封装一路编码器的 A、B 两相 GPIO。两相都配置为上拉输入 + 双沿中断
 * （FALL_RISING_INTERRUPT），任意一相产生边沿时都会触发回调。回调里重新读取
 * A、B 当前电平组成 2 bit 新状态，与上一次状态一起查正交状态表，得到 +1 / -1 /
 * 0 的增量并累加到 count_，从而实现 4 倍频解码（A、B 每次有效跳变都计数）。
 *
 * 计数变量 count_ 在中断里写、主循环里读，因此声明为 volatile。
 *
 * 旋转方向的正负由 A、B 接线顺序决定。如果实测方向与期望相反，最简单的做法是
 * 在上层把目标量取反，或在构造时交换 A/B 两相参数。
 */
class QuadratureDecoder
{
 public:
  /**
   * @brief 绑定一路编码器的 A、B 相 GPIO。
   *
   * @param a_port A 相端口基地址（如 encoder_jie_FLA_PORT）。
   * @param a_pin  A 相引脚掩码（如 encoder_jie_FLA_PIN）。
   * @param a_iomux A 相 IOMUX PINCM 下标（如 encoder_jie_FLA_IOMUX）。
   * @param b_port B 相端口基地址。
   * @param b_pin  B 相引脚掩码。
   * @param b_iomux B 相 IOMUX PINCM 下标。
   */
  QuadratureDecoder(GPIO_Regs* a_port, uint32_t a_pin, uint32_t a_iomux,
                    GPIO_Regs* b_port, uint32_t b_pin, uint32_t b_iomux)
      : a_(a_port, a_pin, a_iomux), b_(b_port, b_pin, b_iomux)
  {
  }

  /**
   * @brief 配置 A、B 两相 GPIO 并注册中断回调，开始计数。
   *
   * 必须在对象构造完成后调用一次。两相都设为上拉输入 + 双沿中断，并把回调绑定到
   * 本对象。SetConfig() 会在运行时覆盖 SysConfig 里“仅下降沿”的极性设置，从而把
   * 中断改成双沿触发，无需修改 SysConfig。
   */
  void Init()
  {
    const LibXR::GPIO::Configuration cfg = {
        LibXR::GPIO::Direction::FALL_RISING_INTERRUPT, LibXR::GPIO::Pull::UP};

    a_.SetConfig(cfg);
    b_.SetConfig(cfg);

    // 同一个回调同时挂到 A、B 两相：任意一相边沿都会重新读两相并更新状态机。
    a_.RegisterCallback(LibXR::GPIO::Callback::Create(&QuadratureDecoder::OnEdge, this));
    b_.RegisterCallback(LibXR::GPIO::Callback::Create(&QuadratureDecoder::OnEdge, this));

    // 记录初始相位，避免第一次边沿时把一个虚假大跳变算进去。
    last_state_ = ReadPhase();

    a_.EnableInterrupt();
    b_.EnableInterrupt();
  }

  /**
   * @brief 读取累计计数值（4 倍频后的脉冲数，带方向符号）。
   */
  int32_t GetCount() const { return count_; }

  /**
   * @brief 把累计计数清零（速度采样基准也会一并复位）。
   */
  void ResetCount()
  {
    count_ = 0;
    last_sample_count_ = 0;
    last_sample_time_ = LibXR::Timebase::GetMicroseconds();
  }

  /**
   * @brief 计算累计转轴角度（弧度）。
   *
   * @param counts_per_rev 输出轴转一圈对应的计数值（编码器线数 × 4 倍频 × 减速比）。
   * @return 角度（弧度），方向与 count_ 符号一致。
   */
  float GetAngleRad(float counts_per_rev) const
  {
    if (counts_per_rev <= 0.0f)
    {
      return 0.0f;
    }
    return static_cast<float>(count_) * (2.0f * 3.14159265358979f / counts_per_rev);
  }

  /**
   * @brief 计算自上次调用以来的平均转速（弧度/秒）。
   *
   * 用两次调用之间的计数差除以时间差得到。建议在固定周期（如每 5ms）调用一次，
   * 调用周期越稳定速度越平滑。第一次调用返回 0。
   *
   * @param counts_per_rev 输出轴转一圈对应的计数值。
   * @return 平均角速度（弧度/秒）。
   */
  float GetSpeedRadPerSec(float counts_per_rev)
  {
    auto now = LibXR::MSPM0Timebase::GetMicroseconds();
    float dt = (now - last_sample_time_).ToSecondf();
    last_sample_time_ = now;

    int32_t current = count_;
    int32_t delta = current - last_sample_count_;
    last_sample_count_ = current;

    if (dt < 1e-6f || counts_per_rev <= 0.0f)
    {
      return 0.0f;
    }

    float revs = static_cast<float>(delta) / counts_per_rev;
    return revs * (2.0f * 3.14159265358979f) / dt;
  }

 private:
  /**
   * @brief 读取当前 A、B 两相组成的 2 bit 相位状态。
   *
   * 第 1 位为 A 相电平，第 0 位为 B 相电平，取值范围 0..3。
   */
  uint8_t ReadPhase()
  {
    uint8_t a = a_.Read() ? 1U : 0U;
    uint8_t b = b_.Read() ? 1U : 0U;
    return static_cast<uint8_t>((a << 1) | b);
  }

  /**
   * @brief GPIO 中断回调：读新相位、查正交表、累加计数。
   *
   * @param in_isr 是否在中断上下文（这里始终为 true）。
   * @param self   构造回调时绑定的解码器对象指针。
   */
  static void OnEdge(bool in_isr, QuadratureDecoder* self)
  {
    (void)in_isr;
    uint8_t new_state = self->ReadPhase();
    // 用 4 位索引 (旧状态<<2 | 新状态) 查表，得到 +1 / -1 / 0。
    uint8_t index = static_cast<uint8_t>((self->last_state_ << 2) | new_state);
    self->count_ += kTransitionTable[index];//!检测到有效跳变才更新计数，抖动或非法跳变不计数。
    self->last_state_ = new_state;
  }

  /**
   * @brief 正交解码状态转移表（4 倍频）。
   *
   * 索引为 (上一状态 << 2) | 新状态，每个状态是 (A<<1)|B 的 2 bit 值。合法的单步
   * 跳变给出 +1（一个方向）或 -1（另一个方向）；非法跳变（A、B 同时变化，通常是
   * 抖动或丢步）记 0，保持计数不变。
   */

   // 状态转移图（箭头方向对应 +1 还是 -1）：
  static constexpr int8_t kTransitionTable[16] = {
      0, +1, -1, 0,   // 旧 00 -> 00/01/10/11
      -1, 0, 0, +1,   // 旧 01 -> 00/01/10/11
      +1, 0, 0, -1,   // 旧 10 -> 00/01/10/11
      0, -1, +1, 0};  // 旧 11 -> 00/01/10/11

  LibXR::MSPM0GPIO a_;
  LibXR::MSPM0GPIO b_;

  volatile int32_t count_ = 0;
  uint8_t last_state_ = 0;

  int32_t last_sample_count_ = 0;
  LibXR::MicrosecondTimestamp last_sample_time_{};
};

/**
 * @brief 四路 520 编码器电机的聚合读取器。
 *
 * 把四个 QuadratureDecoder 按 前左(FL)/前右(FR)/后左(BL)/后右(BR) 聚合，构造函数
 * 直接使用 SysConfig 生成的 encoder_jie_FLA/FLB/... 等 8 个引脚宏，无需外部传参，
 * 与 module/line/line.hpp 的风格保持一致。
 *
 * 用法：
 *   Module::Encoder encoder;
 *   encoder.Init();                                  // 配置 GPIO + 使能中断
 *   float w = encoder.GetSpeed(Module::Encoder::kFrontLeft);
 *
 * 注意：编码器中断真正生效还需要项目里实现 GROUP1_IRQHandler（见 encoder_irq.cpp），
 * 否则 GPIOB 的中断不会被分发到这些回调。
 */
class Encoder
{
 public:
  /**
   * @brief 电机/车轮编号，作为下面各接口的下标。
   */
  enum MotorId : size_t
  {
    kFrontLeft = 0,  ///< 前左 FL
    kFrontRight,     ///< 前右 FR
    kBackLeft,       ///< 后左 BL
    kBackRight,      ///< 后右 BR
    kMotorCount      ///< 电机数量（4）
  };

  /**
   * @brief 520 减速电机一圈对应的计数默认值。
   *
   * 这里给的是常见 13 线霍尔编码器 × 4 倍频 × 30 减速比 = 1560 的占位值。请按实际
   * 电机规格（编码器 PPR 和减速比）修改，速度/角度换算才会准确。
   */
  static constexpr float kDefaultCountsPerRev = 1560.0f;

  Encoder()
      : decoders_{{
            {encoder_jie_PORT, encoder_jie_FLA_PIN, encoder_jie_FLA_IOMUX,
             encoder_jie_PORT, encoder_jie_FLB_PIN, encoder_jie_FLB_IOMUX},
            {encoder_jie_PORT, encoder_jie_FRA_PIN, encoder_jie_FRA_IOMUX,
             encoder_jie_PORT, encoder_jie_FRB_PIN, encoder_jie_FRB_IOMUX},
            {encoder_jie_PORT, encoder_jie_BLA_PIN, encoder_jie_BLA_IOMUX,
             encoder_jie_PORT, encoder_jie_BLB_PIN, encoder_jie_BLB_IOMUX},
            {encoder_jie_PORT, encoder_jie_BRA_PIN, encoder_jie_BRA_IOMUX,
             encoder_jie_PORT, encoder_jie_BRB_PIN, encoder_jie_BRB_IOMUX},
        }}
  {
  }

  /**
   * @brief 配置全部四路编码器并使能中断。构造后调用一次。
   */
  void Init()
  {
    for (auto& d : decoders_)
    {
      d.Init();
    }
  }

  /**
   * @brief 读取某个电机的累计计数。
   */
  int32_t GetCount(MotorId id) const
  {
    return id < kMotorCount ? decoders_[id].GetCount() : 0;
  }

  /**
   * @brief 读取某个电机的累计角度（弧度）。
   */
  float GetAngle(MotorId id, float counts_per_rev = kDefaultCountsPerRev) const
  {
    return id < kMotorCount ? decoders_[id].GetAngleRad(counts_per_rev) : 0.0f;
  }

  /**
   * @brief 读取某个电机自上次调用以来的平均转速（弧度/秒）。
   */
  float GetSpeed(MotorId id, float counts_per_rev = kDefaultCountsPerRev)
  {
    return id < kMotorCount ? decoders_[id].GetSpeedRadPerSec(counts_per_rev) : 0.0f;
  }

  /**
   * @brief 清零所有电机的计数。
   */
  void ResetAll()
  {
    for (auto& d : decoders_)
    {
      d.ResetCount();
    }
  }

 private:
  std::array<QuadratureDecoder, kMotorCount> decoders_;
};

}  // namespace Module
