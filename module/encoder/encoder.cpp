#include "encoder.hpp"

#include <bit>
#include <cstdint>

namespace Module
{
namespace
{

/**
 * @brief 作用域级全局中断保护器，析构时恢复进入作用域前的 PRIMASK。
 *
 * 构造时先保存当前 PRIMASK，再关闭中断；析构时写回原值，而不是无条件开中断。
 * 因此它可安全嵌套：如果调用方本来就在临界区内，内层保护器退出后仍保持关中断。
 */
class InterruptGuard
{
 public:
  /** @brief 保存调用方中断状态并进入临界区。 */
  InterruptGuard() : primask_(__get_PRIMASK()) { __disable_irq(); }

  // 保护器必须严格绑定当前作用域，禁止复制或移动导致重复恢复 PRIMASK。
  InterruptGuard(const InterruptGuard&) = delete;
  InterruptGuard(InterruptGuard&&) = delete;
  InterruptGuard& operator=(const InterruptGuard&) = delete;
  InterruptGuard& operator=(InterruptGuard&&) = delete;

  /** @brief 恢复构造前保存的中断状态。 */
  ~InterruptGuard() { __set_PRIMASK(primask_); }

 private:
  uint32_t primask_;  ///< 进入临界区前的 Cortex-M PRIMASK 快照。
};

}  // namespace（本翻译单元私有辅助类型）

QuadratureDecoder::QuadratureDecoder(GPIO_Regs* a_port, uint32_t a_pin,
                                     uint32_t a_iomux, GPIO_Regs* b_port,
                                     uint32_t b_pin, uint32_t b_iomux)
    : a_(a_port, a_pin, a_iomux), b_(b_port, b_pin, b_iomux)
{
}

void QuadratureDecoder::Init()
{
  // 上拉使悬空输入保持确定高电平；双边沿中断使 A/B 每次有效跳变都参与四倍频计数。
  const LibXR::GPIO::Configuration config = {
      LibXR::GPIO::Direction::FALL_RISING_INTERRUPT, LibXR::GPIO::Pull::UP};

  a_.SetConfig(config);
  b_.SetConfig(config);
  // 两相共用同一状态机回调；任意一相变化后都重新采集完整 A/B 状态。
  a_.RegisterCallback(
      LibXR::GPIO::Callback::Create(&QuadratureDecoder::OnEdge, this));
  b_.RegisterCallback(
      LibXR::GPIO::Callback::Create(&QuadratureDecoder::OnEdge, this));

  // 必须先记录稳定的初始相位，再开放中断，避免第一次回调从默认 00 产生虚假计数。
  last_state_ = ReadPhase();
  a_.EnableInterrupt();
  b_.EnableInterrupt();
}

int32_t QuadratureDecoder::GetCount() const
{
  // bit_cast 只改变位模式的解释方式，不执行可能依赖实现的无符号到有符号数值转换。
  const uint32_t modular_count = count_;
  return std::bit_cast<int32_t>(modular_count);
}

void QuadratureDecoder::ResetCount() { count_ = 0U; }

void QuadratureDecoder::Shutdown()
{
  a_.DisableInterrupt();
  b_.DisableInterrupt();
}

uint8_t QuadratureDecoder::ReadPhase()
{
  const uint8_t a = a_.Read() ? 1U : 0U;
  const uint8_t b = b_.Read() ? 1U : 0U;
  // 状态布局与 kTransitionTable 一致：A 位于 bit1，B 位于 bit0。
  return static_cast<uint8_t>((a << 1U) | b);
}

void QuadratureDecoder::OnEdge(bool in_isr, QuadratureDecoder* self)
{
  // 回调始终执行同一套无阻塞逻辑，当前实现不需要根据上下文标志分支。
  (void)in_isr;
  const uint8_t new_state = self->ReadPhase();
  // 两个 2 bit 状态拼成 4 bit 查表索引：[旧 A/B][新 A/B]。
  const uint8_t index =
      static_cast<uint8_t>((self->last_state_ << 2U) | new_state);
  const int8_t transition = kTransitionTable[index];

  // 分支写法避免把 -1 直接混入无符号表达式；加减均按模 2^32 自然回绕。
  // transition == 0 表示电平未变或两相同时变化，不更新计数但仍同步状态机基准。
  if (transition > 0)
  {
    self->count_ = self->count_ + 1U;
  }
  else if (transition < 0)
  {
    self->count_ = self->count_ - 1U;
  }
  self->last_state_ = new_state;
}

Encoder::Encoder(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app)
    : Encoder(hw, app, Config{})
{
  // 委托给显式配置构造函数，保证默认与自定义构造路径执行完全相同的注册流程。
}

Encoder::Encoder(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
                 const Config& config)
    : decoders_{{
          // 数组顺序必须与 MotorId 保持一致：前左、前右、后左、后右。
          {encoder_jie_PORT, encoder_jie_FLA_PIN, encoder_jie_FLA_IOMUX,
           encoder_jie_PORT, encoder_jie_FLB_PIN, encoder_jie_FLB_IOMUX},
          {encoder_jie_PORT, encoder_jie_FRA_PIN, encoder_jie_FRA_IOMUX,
           encoder_jie_PORT, encoder_jie_FRB_PIN, encoder_jie_FRB_IOMUX},
          {encoder_jie_PORT, encoder_jie_BLA_PIN, encoder_jie_BLA_IOMUX,
           encoder_jie_PORT, encoder_jie_BLB_PIN, encoder_jie_BLB_IOMUX},
          {encoder_jie_PORT, encoder_jie_BRA_PIN, encoder_jie_BRA_IOMUX,
           encoder_jie_PORT, encoder_jie_BRB_PIN, encoder_jie_BRB_IOMUX},
      }},
      config_(config),
      sample_topic_(LibXR::Topic::CreateTopic<Sample>(config.topic_name))
{
  // 对象与 Topic 已构造完成后再使能 GPIO 中断，确保回调访问的是完整实例。
  for (auto& decoder : decoders_)
  {
    decoder.Init();
  }

  // 注册后 ApplicationManager 可周期调用 OnMonitor()，HardwareContainer 可按名称查找模块。
  app.Register(*this);
  hw.Register(LibXR::Entry<Encoder>{*this, {"encoder"}});
}

Encoder::~Encoder()
{
  // 在关闭八个 GPIO 中断的整个过程中保持临界区，避免析构与边沿回调交错。
  InterruptGuard guard;
  for (auto& decoder : decoders_)
  {
    decoder.Shutdown();
  }
}

LibXR::Topic& Encoder::SampleTopic() { return sample_topic_; }

const Encoder::Sample& Encoder::LatestSample() const { return latest_sample_; }

void Encoder::ResetAll()
{
  // 四路计数和与其关联的软件采样状态必须作为一个整体复位。
  InterruptGuard guard;
  for (auto& decoder : decoders_)
  {
    decoder.ResetCount();
  }

  // 序号用于消费者识别发布先后，因此复位测量值时保留序号的单调递增关系。
  const uint32_t latest_sequence = latest_sample_.sequence;
  latest_sample_ = {};
  latest_sample_.sequence = latest_sequence;
  // 清除历史后，下一帧没有有效差分基准，BuildSample() 将输出零速度。
  previous_counts_.reset();
  last_sample_time_ = {};
  has_published_ = false;
}

EncoderMath::WheelCounts Encoder::ReadCounts() const
{
  EncoderMath::WheelCounts counts{};
  // 在同一临界区复制四路 volatile 计数，避免采集过程中某一路被中断更新而造成帧间错位。
  InterruptGuard guard;
  for (size_t wheel = 0; wheel < decoders_.size(); ++wheel)
  {
    counts[wheel] = decoders_[wheel].GetCount();
  }
  return counts;
}

void Encoder::OnMonitor()
{
  const LibXR::MicrosecondTimestamp now_us = LibXR::Timebase::GetMicroseconds();
  // 首帧立即发布；周期配置为 0 时关闭节流，其余情况等待完整的毫秒周期。
  if (has_published_ && config_.publish_period_ms != 0U &&
      (now_us - last_sample_time_).ToMicrosecond() <
          static_cast<uint64_t>(config_.publish_period_ms) * 1000ULL)
  {
    return;
  }

  // 计数快照和时间戳只采集一次，四个车轮共享同一 dt，保证帧内运动学时间一致。
  const EncoderMath::WheelCounts counts = ReadCounts();
  const float dt_seconds = has_published_
                               ? (now_us - last_sample_time_).ToSecondf()
                               : 0.0F;
  // 首帧 dt 为 0 且 previous_counts_ 为空，因此只生成角度，速度保持为 0。
  const EncoderMath::Sample kinematics = EncoderMath::BuildSample(
      counts, previous_counts_, dt_seconds, config_.counts_per_rev);

  // 先在栈上组装完整消息，避免订阅者观察到只更新了一部分字段的缓存对象。
  Sample sample{};
  sample.count = counts;
  sample.angle_rad = kinematics.angle_rad;
  sample.speed_rad_s = kinematics.speed_rad_s;
  // Topic 协议使用毫秒时间戳；这里从微秒时间基准截断，并允许 uint32_t 自然回绕。
  sample.sample_time_ms = static_cast<uint32_t>(
      static_cast<uint64_t>(now_us) / 1000ULL);
  sample.sequence = sequence_++;

  // 发布前先提交全部内部状态，使同步回调读取 LatestSample() 时看到的就是当前帧。
  previous_counts_ = counts;
  last_sample_time_ = now_us;
  latest_sample_ = sample;
  has_published_ = true;
  sample_topic_.Publish(latest_sample_);
}

}  // namespace Module（编码器业务模块）
