#include "sr04.h"

#include "libxr_def.hpp"
#include "thread.hpp"

#include <cstdint>
#include <limits>

namespace
{

/**
 * @brief Validates a Topic name before CreateTopic() receives it.
 * @param topic_name Non-null, non-empty Topic name.
 * @return The validated Topic name.
 */
const char* ValidateTopicName(const char* topic_name)
{
  ASSERT(topic_name != nullptr && topic_name[0] != '\0');
  return topic_name;
}

/**
 * @brief Maps a capture channel to its pulse-width-up completion event.
 * @param channel DriverLib capture/compare channel index.
 * @return The channel's unique CC-up event mask, or zero if unsupported.
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
  ASSERT(resources_.capture_timer != nullptr);
  ASSERT(resources_.capture_clock_hz > 0U);
  const std::uint32_t expected_capture_event_mask =
      PulseWidthUpEventMask(resources_.capture_channel);
  ASSERT(expected_capture_event_mask != 0U);
  ASSERT(resources_.capture_event_mask == expected_capture_event_mask);
  ASSERT(config_.trigger_frequency_hz > 0U);
  ASSERT(config_.trigger_pulse_width_us > 0U);
  ASSERT(config_.min_distance_mm <= config_.max_distance_mm);
  ASSERT(config_.filter_old_weight >= 0.0F &&
         config_.filter_old_weight <= 1.0F);
  ASSERT(config_.no_echo_timeout_ms > 0U);

  const std::uint64_t high_us_per_second =
      static_cast<std::uint64_t>(config_.trigger_frequency_hz) *
      config_.trigger_pulse_width_us;
  ASSERT(high_us_per_second <= 1000000ULL);

  trigger_period_ms_ = static_cast<std::uint32_t>(
      (1000ULL + config_.trigger_frequency_hz - 1ULL) /
      config_.trigger_frequency_hz);

  const std::uint64_t max_capture_ticks =
      DL_Timer_getLoadValue(resources_.capture_timer);
  const std::uint64_t max_capture_microseconds =
      (max_capture_ticks * 1000000ULL +
       resources_.capture_clock_hz / 2U) /
      resources_.capture_clock_hz;
  ASSERT(max_capture_microseconds <=
         std::numeric_limits<std::uint32_t>::max());

  ASSERT(trigger_pwm_.Disable() == LibXR::ErrorCode::OK);
  ASSERT(trigger_pwm_.SetConfig({config_.trigger_frequency_hz}) ==
         LibXR::ErrorCode::OK);
  const float duty_cycle =
      static_cast<float>(high_us_per_second) / 1000000.0F;
  ASSERT(trigger_pwm_.SetDutyCycle(duty_cycle) == LibXR::ErrorCode::OK);

  DL_Timer_clearInterruptStatus(resources_.capture_timer,
                                resources_.capture_event_mask);
  DL_Timer_startCounter(resources_.capture_timer);
  ASSERT(trigger_pwm_.Enable() == LibXR::ErrorCode::OK);

  const std::uint32_t now_ms = LibXR::Thread::GetTime();
  last_capture_ms_ = now_ms;
  last_publish_ms_ = now_ms;
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
  const std::uint32_t capture_event = DL_Timer_getRawInterruptStatus(
      resources_.capture_timer, resources_.capture_event_mask);

  if (capture_event != 0U)
  {
    const std::uint32_t ticks = DL_Timer_getCaptureCompareValue(
        resources_.capture_timer, resources_.capture_channel);
    DL_Timer_clearInterruptStatus(resources_.capture_timer,
                                  resources_.capture_event_mask);
    last_capture_ms_ = now_ms;

    Publish(Module::HC_SR04Logic::ProcessCapture(
                ticks, resources_.capture_clock_hz, config_.min_distance_mm,
                config_.max_distance_mm, config_.filter_old_weight,
                filter_state_),
            now_ms);
    return;
  }

  if (static_cast<std::uint32_t>(now_ms - last_capture_ms_) <
          config_.no_echo_timeout_ms ||
      static_cast<std::uint32_t>(now_ms - last_publish_ms_) <
          trigger_period_ms_)
  {
    return;
  }

  Publish(Module::HC_SR04Logic::BuildNoEcho(filter_state_), now_ms);
}

void HC_SR04::Publish(Sample sample, std::uint32_t now_ms)
{
  sample.sequence = sequence_++;
  latest_sample_ = sample;
  last_publish_ms_ = now_ms;
  sample_topic_.Publish(latest_sample_);
}
