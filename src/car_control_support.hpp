#pragma once

#include <array>
#include <cstdint>

#include "BitsButtonXR.hpp"
#include "control_topic_logic.hpp"
#include "pid.hpp"
#include "tb6612.hpp"
#include "xrobot_main.hpp"

extern "C" {

struct SpeedFeedForwardParam
{
  float static_output;
  float velocity_gain;
};

extern LibXR::PID<float>::Param
    speed_pid_config[Module::MotorGroup::kMotorCount];
extern SpeedFeedForwardParam
    speed_feedforward_config[Module::MotorGroup::kMotorCount];
extern LibXR::PID<float> speed_pid[Module::MotorGroup::kMotorCount];

}  // extern "C"

namespace CarControlSupport
{

constexpr std::array<const char*, 4> kButtonAliases = {"btn1", "btn2", "btn3",
                                                       "btn4"};
constexpr LibXR::GPIO::Configuration kGreySensorInputConfig = {
    LibXR::GPIO::Direction::INPUT, LibXR::GPIO::Pull::UP};
constexpr bool kGreySensorActiveLow = false;
constexpr uint32_t kControlPeriodMs = 5;
constexpr uint32_t kTopicFreshnessTimeoutMs = 50U;
constexpr float kWheelRadiusM = 0.032f;
constexpr float kForwardDistanceTargetM = 50.0f;
constexpr float kForwardSpeedRadS = 8.0f;
constexpr float kSpinSpeedRadS = 3.0f;
constexpr float kSpeedTargetEpsilon = 1e-3f;
constexpr BitsButtonXR::ButtonConstraints kButtonConstraints = {
    .short_press_time_ms = 50,
    .long_press_start_time_ms = 1000,
    .long_press_period_triger_ms = 500,
    .time_window_time_ms = 300,
};

enum class DriveMode : uint8_t
{
  kIdle = 0,
  kForwardDistance,
  kLineFollowing,
  kSpinInPlace,
};

struct CarControlSample
{
  uint32_t control_time_ms = 0;
  uint32_t elapsed_ms = 0;
  float dt_s = 0.0f;
  DriveMode drive_mode = DriveMode::kIdle;
  bool encoder_topic_ready = false;
  bool tracking_topic_ready = false;
  bool grey_sensor_active_low = false;
  bool line_lost = false;
  float forward_distance_m = 0.0f;
  std::array<float, Module::MotorGroup::kMotorCount> target_speed{};
  std::array<float, Module::MotorGroup::kMotorCount> measured_speed{};
  std::array<float, Module::MotorGroup::kMotorCount> feedforward_output{};
  std::array<float, Module::MotorGroup::kMotorCount> pid_output{};
  std::array<float, Module::MotorGroup::kMotorCount> motor_output{};
};

constexpr int8_t kEncoderDirection[Module::MotorGroup::kMotorCount] = {
    1, -1, 1, -1};
constexpr int8_t kMotorOutputDirection[Module::MotorGroup::kMotorCount] = {
    1, 1, 1, -1};

void ResetSpeedPids();
float ResolveTargetSpeed(DriveMode drive_mode, uint8_t motor_index,
                         const Tracking::Output& tracking_output,
                         bool encoder_topic_fresh,
                         bool tracking_topic_fresh);
float AbsFloat(float value);
float ClampMotorOutput(float output);
float CalculateSpeedFeedForward(uint8_t motor_index,
                                float target_speed_rad_s);
uint8_t ResolveButtonIndex(const char* alias);
float CalculateForwardDistanceM(
    const Module::Encoder::Sample& encoder_sample);
void ResetTrackingState(
    Tracking& tracking,
    LibXR::Topic::ASyncSubscriber<Tracking::Output>& tracking_subscriber,
    Tracking::Output& tracking_output, bool& has_tracking_sample);
void ResetEncoderState(
    Module::Encoder& encoder,
    LibXR::Topic::ASyncSubscriber<Module::Encoder::Sample>& encoder_subscriber,
    Module::Encoder::Sample& encoder_sample, bool& has_encoder_sample);

}  // namespace CarControlSupport
