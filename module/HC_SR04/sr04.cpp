#include "sr04.h"

#include "libxr_def.hpp"
#include "thread.hpp"

#include <cstdint>
#include <limits>

namespace
{

/**
 * @brief 在创建 Topic 前校验名称，避免把空指针或空字符串传入消息系统。
 * @param topic_name 调用方配置的 Topic 名称，必须指向非空字符串。
 * @return 校验通过的原始指针，供 CreateTopic() 直接使用。
 */
const char* ValidateTopicName(const char* topic_name)
{
  ASSERT(topic_name != nullptr && topic_name[0] != '\0');
  return topic_name;
}

/**
 * @brief 将捕获/比较通道映射为对应的向上脉宽捕获完成事件。
 * @param channel DriverLib 的捕获/比较通道索引。
 * @return 对应通道唯一的 CCx_UP 事件掩码；不支持的索引返回 0。
 *
 * @details MSPM0 DriverLib 为每个 CC 通道定义了独立的原始事件位。构造函数用本
 * 函数核对调用方传入的 capture_event_mask，防止“从一个通道读取捕获值，却查询或
 * 清除另一个通道事件”的资源接线错误。
 */
std::uint32_t PulseWidthUpEventMask(DL_TIMER_CC_INDEX channel)
{
  switch (channel)
  {
    case DL_TIMER_CC_0_INDEX:
      return DL_TIMER_INTERRUPT_CC0_UP_EVENT;
    case DL_TIMER_CC_1_INDEX:
      return DL_TIMER_INTERRUPT_CC1_UP_EVENT;
    case DL_TIMER_CC_2_INDEX:
      return DL_TIMER_INTERRUPT_CC2_UP_EVENT;
    case DL_TIMER_CC_3_INDEX:
      return DL_TIMER_INTERRUPT_CC3_UP_EVENT;
    case DL_TIMER_CC_4_INDEX:
      return DL_TIMER_INTERRUPT_CC4_UP_EVENT;
    case DL_TIMER_CC_5_INDEX:
      return DL_TIMER_INTERRUPT_CC5_UP_EVENT;
    default:
      return 0U;
  }
}

}  // namespace

// 默认构造路径统一委托给完整构造函数，确保两种入口执行完全相同的校验和硬件时序。
HC_SR04::HC_SR04(LibXR::ApplicationManager& app, const Resources& resources)
    : HC_SR04(app, resources, Config{})
{
}

HC_SR04::HC_SR04(LibXR::ApplicationManager& app, const Resources& resources,
                 const Config& config)
    : resources_(resources),
      config_(config),
      trigger_pwm_(resources.trigger_pwm),
      sample_topic_(LibXR::Topic::CreateTopic<Sample>(
          ValidateTopicName(config.topic_name)))
{
  // 资源与配置在启动任何定时器前一次性校验；失败时立即断言，避免以部分配置运行。
  ASSERT(resources_.capture_timer != nullptr);
  ASSERT(resources_.capture_clock_hz > 0U);
  const std::uint32_t expected_capture_event_mask =
      PulseWidthUpEventMask(resources_.capture_channel);
  // 事件掩码必须与 CC 通道一一对应，不能使用组合掩码或其他通道的事件。
  ASSERT(expected_capture_event_mask != 0U);
  ASSERT(resources_.capture_event_mask == expected_capture_event_mask);
  ASSERT(config_.trigger_frequency_hz > 0U);
  ASSERT(config_.trigger_pulse_width_us > 0U);
  ASSERT(config_.min_distance_mm <= config_.max_distance_mm);
  ASSERT(config_.filter_old_weight >= 0.0F &&
         config_.filter_old_weight <= 1.0F);
  ASSERT(config_.no_echo_timeout_ms > 0U);

  // 每秒累计高电平时间 = 触发频率 * 单脉冲宽度；该值同时是微秒制下的占空比分子。
  // 使用 64 位中间值，避免较大配置在相乘时发生 32 位无符号溢出。
  const std::uint64_t high_us_per_second =
      static_cast<std::uint64_t>(config_.trigger_frequency_hz) *
      config_.trigger_pulse_width_us;
  // 一秒最多包含 1,000,000 us，高电平累计时间超过它意味着占空比大于 100%。
  ASSERT(high_us_per_second <= 1000000ULL);

  // 向上取整为整数毫秒，保证无回波状态的发布间隔不会短于一个 TRIG 周期。
  trigger_period_ms_ = static_cast<std::uint32_t>(
      (1000ULL + config_.trigger_frequency_hz - 1ULL) /
      config_.trigger_frequency_hz);

  // 捕获寄存器最大计数值决定可表示的最长 ECHO 脉宽。这里以四舍五入方式换算为
  // 微秒并确认后续的 uint32_t 脉宽字段能够完整承载，避免运行时静默截断。
  const std::uint64_t max_capture_ticks =
      DL_Timer_getLoadValue(resources_.capture_timer);
  const std::uint64_t max_capture_microseconds =
      (max_capture_ticks * 1000000ULL +
       resources_.capture_clock_hz / 2U) /
      resources_.capture_clock_hz;
  ASSERT(max_capture_microseconds <=
         std::numeric_limits<std::uint32_t>::max());

  // 先停用 PWM 再修改周期和占空比，防止重配置期间在 TRIG 引脚上产生畸形脉冲。
  ASSERT(trigger_pwm_.Disable() == LibXR::ErrorCode::OK);
  ASSERT(trigger_pwm_.SetConfig({config_.trigger_frequency_hz}) ==
         LibXR::ErrorCode::OK);
  // high_us_per_second / 1,000,000 等价于“高电平时间 / 一秒”，即目标 PWM 占空比。
  const float duty_cycle =
      static_cast<float>(high_us_per_second) / 1000000.0F;
  ASSERT(trigger_pwm_.SetDutyCycle(duty_cycle) == LibXR::ErrorCode::OK);

  // 清除上电或板级初始化遗留的捕获事件，再启动 ECHO 计数器，最后开启 TRIG。
  // 该顺序保证 PWM 发出第一枚有效脉冲前，捕获链路已经处于可接收状态。
  DL_Timer_clearInterruptStatus(resources_.capture_timer,
                                resources_.capture_event_mask);
  DL_Timer_startCounter(resources_.capture_timer);
  ASSERT(trigger_pwm_.Enable() == LibXR::ErrorCode::OK);

  // 用同一时刻初始化捕获和发布基准，避免模块注册后立即误判为无回波超时。
  const std::uint32_t now_ms = LibXR::Thread::GetTime();
  last_capture_ms_ = now_ms;
  last_publish_ms_ = now_ms;
  // 所有成员与硬件均准备完成后再注册，防止管理器观察到尚未初始化完整的对象。
  app.Register(*this);
}

LibXR::Topic& HC_SR04::SampleTopic() { return sample_topic_; }

const HC_SR04::Sample& HC_SR04::GetLatestSample() const
{
  return latest_sample_;
}

bool HC_SR04::HasValidSample() const
{
  return filter_state_.has_valid_sample;
}

void HC_SR04::OnMonitor()
{
  const std::uint32_t now_ms = LibXR::Thread::GetTime();
  // 查询原始事件而不是捕获寄存器本身：只有事件置位才表示出现了一笔新 ECHO，
  // 从而避免同一个锁存值在多次 OnMonitor() 调用中被重复发布。
  const std::uint32_t capture_event = DL_Timer_getRawInterruptStatus(
      resources_.capture_timer, resources_.capture_event_mask);

  if (capture_event != 0U)
  {
    // 在清除事件前读取锁存的脉宽 tick；清除后下一次轮询只会处理新的捕获结果。
    const std::uint32_t ticks = DL_Timer_getCaptureCompareValue(
        resources_.capture_timer, resources_.capture_channel);
    DL_Timer_clearInterruptStatus(resources_.capture_timer,
                                  resources_.capture_event_mask);
    last_capture_ms_ = now_ms;

    // 换算、范围分类和 EMA 更新集中在纯逻辑层；无论 VALID 还是越界状态都发布，
    // 让订阅者能够区分“有回波但超量程”和“完全没有回波”。
    Publish(Module::HC_SR04Logic::ProcessCapture(
                ticks, resources_.capture_clock_hz, config_.min_distance_mm,
                config_.max_distance_mm, config_.filter_old_weight,
                filter_state_),
            now_ms);
    return;
  }

  // 无回波发布需要同时满足两个条件：距离最近捕获已超过超时，且距离最近发布至少
  // 一个触发周期。两个时间差均使用无符号减法，因此毫秒计数器回绕时仍保持正确。
  if (static_cast<std::uint32_t>(now_ms - last_capture_ms_) <
          config_.no_echo_timeout_ms ||
      static_cast<std::uint32_t>(now_ms - last_publish_ms_) <
          trigger_period_ms_)
  {
    return;
  }

  // 超时样本保留最近一次有效 EMA 值，但 status 明确为 NO_ECHO；订阅者必须先看状态。
  Publish(Module::HC_SR04Logic::BuildNoEcho(filter_state_), now_ms);
}

void HC_SR04::Publish(Sample sample, std::uint32_t now_ms)
{
  // 所有状态共用一个发布序列；uint32_t 自然回绕，不影响相邻样本的顺序判断。
  sample.sequence = sequence_++;
  // 先更新本地缓存和节流基准，再发布同一份样本，保证查询接口与 Topic 内容一致。
  latest_sample_ = sample;
  last_publish_ms_ = now_ms;
  sample_topic_.Publish(latest_sample_);
}
