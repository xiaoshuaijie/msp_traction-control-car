#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: HC-SR04 ultrasonic distance module with PWM trigger, pulse-width capture, and Topic publishing
constructor_args:
  - resources:
      trigger_pwm: "pwm_ultrasonic"
      capture_timer: "capture_ultrasonic"
      capture_channel: "capture_ultrasonic"
      capture_event_mask: "capture_ultrasonic"
      capture_clock_hz: 1000000
  - config:
      topic_name: "hc_sr04"
      trigger_frequency_hz: 16
      trigger_pulse_width_us: 10
      min_distance_mm: 20.0
      max_distance_mm: 4000.0
      filter_old_weight: 0.6
      no_echo_timeout_ms: 100
template_args: []
required_hardware:
  - pwm_ultrasonic
  - capture_ultrasonic
depends: []
=== END MANIFEST === */
// clang-format on

#include "app_framework.hpp"
#include "message.hpp"
#include "mspm0_pwm.hpp"
#include "sr04_math.hpp"

#include <cstdint>

/**
 * @brief Publishes HC-SR04 distance samples from PWM-triggered pulse capture.
 *
 * @details The instance must outlive ApplicationManager monitoring because the
 * manager retains its registered Application pointer.
 */
class HC_SR04 : public LibXR::Application
{
 public:
  /** Hardware resources required by the trigger and echo paths. */
  struct Resources
  {
    LibXR::MSPM0PWM::Resources trigger_pwm;
    GPTIMER_Regs* capture_timer;
    DL_TIMER_CC_INDEX capture_channel;
    std::uint32_t capture_event_mask;
    std::uint32_t capture_clock_hz;
  };

  /** Runtime settings for triggering, validation, filtering, and publication. */
  struct Config
  {
    const char* topic_name = "hc_sr04";
    std::uint32_t trigger_frequency_hz = 16;
    std::uint32_t trigger_pulse_width_us = 10;
    float min_distance_mm = 20.0F;
    float max_distance_mm = 4000.0F;
    float filter_old_weight = 0.6F;
    std::uint32_t no_echo_timeout_ms = 100;
  };

  using Status = Module::HC_SR04Logic::Status;
  using Sample = Module::HC_SR04Logic::Sample;

  /**
   * @brief Constructs and registers the module with default settings.
   * @param app Application manager that drives OnMonitor().
   * @param resources Valid PWM and capture resources.
   */
  HC_SR04(LibXR::ApplicationManager& app, const Resources& resources);

  /**
   * @brief Constructs, configures, and registers the module.
   * @param app Application manager that drives OnMonitor().
   * @param resources Valid PWM and capture resources.
   * @param config Trigger, distance, filter, timeout, and Topic settings.
   */
  HC_SR04(LibXR::ApplicationManager& app, const Resources& resources,
          const Config& config);

  /** @return Topic used to publish Sample values. */
  LibXR::Topic& SampleTopic();

  /** @return Most recently published sample, or the default sample initially. */
  [[nodiscard]] const Sample& GetLatestSample() const;

  /** @return true after at least one in-range echo initializes the filter. */
  [[nodiscard]] bool HasValidSample() const;

  /** Polls capture state and publishes echo or timeout samples when due. */
  void OnMonitor() override;

 private:
  /**
   * @brief Assigns sequence metadata, caches, and publishes one sample.
   * @param sample Sample payload before sequence assignment.
   * @param now_ms Current unsigned millisecond time.
   */
  void Publish(Sample sample, std::uint32_t now_ms);

  Resources resources_;
  Config config_;
  LibXR::MSPM0PWM trigger_pwm_;
  LibXR::Topic sample_topic_;
  Module::HC_SR04Logic::FilterState filter_state_;
  Sample latest_sample_;
  std::uint32_t last_capture_ms_ = 0;
  std::uint32_t last_publish_ms_ = 0;
  std::uint32_t trigger_period_ms_ = 0;
  std::uint32_t sequence_ = 0;
};
