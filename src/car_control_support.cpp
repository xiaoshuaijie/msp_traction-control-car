#include "car_control_support.hpp"

#include <cstring>

#include "control_topic_logic.hpp"

extern "C" {

LibXR::PID<float>::Param speed_pid_config[Module::MotorGroup::kMotorCount] = {
    {.k = 1.0f,
     .p = 0.36f,
     .i = 0.20f,
     .d = 0.0f,
     .i_limit = 0.5f,
     .out_limit = 0.45f,
     .cycle = false},
    {.k = 1.0f,
     .p = 0.36f,
     .i = 0.20f,
     .d = 0.0f,
     .i_limit = 0.6f,
     .out_limit = 0.65f,
     .cycle = false},
    {.k = 1.0f,
     .p = 0.36f,
     .i = 0.20f,
     .d = 0.0f,
     .i_limit = 0.5f,
     .out_limit = 0.45f,
     .cycle = false},
    {.k = 1.0f,
     .p = 0.36f,
     .i = 0.20f,
     .d = 0.0f,
     .i_limit = 0.2f,
     .out_limit = 0.25f,
     .cycle = false}};

SpeedFeedForwardParam speed_feedforward_config[Module::MotorGroup::kMotorCount] = {
    {.static_output = 0.18f, .velocity_gain = 0.075f},
    {.static_output = 0.28f, .velocity_gain = 0.085f},
    {.static_output = 0.18f, .velocity_gain = 0.075f},
    {.static_output = 0.08f, .velocity_gain = 0.075f}};

LibXR::PID<float> speed_pid[Module::MotorGroup::kMotorCount] = {
    LibXR::PID<float>(speed_pid_config[0]),
    LibXR::PID<float>(speed_pid_config[1]),
    LibXR::PID<float>(speed_pid_config[2]),
    LibXR::PID<float>(speed_pid_config[3])};

}  // extern "C"

namespace CarControlSupport
{

void ResetSpeedPids()
{
  for (auto& pid : speed_pid)
  {
    pid.Reset();
  }
}

bool IsLeftMotor(uint8_t index)
{
  return index == Module::MotorGroup::kFrontLeft ||
         index == Module::MotorGroup::kBackLeft;
}

float ResolveTargetSpeed(DriveMode drive_mode, uint8_t motor_index,
                         const Tracking::Output& tracking_output,
                         bool encoder_topic_fresh,
                         bool tracking_topic_fresh)
{
  float desired = 0.0f;
  if (drive_mode == DriveMode::kForwardDistance)
  {
    desired = kForwardSpeedRadS;
  }
  else if (drive_mode == DriveMode::kLineFollowing)
  {
    desired = tracking_output.wheel_speed_rad_s[motor_index];
  }
  else if (drive_mode == DriveMode::kSpinInPlace)
  {
    desired = IsLeftMotor(motor_index) ? kSpinSpeedRadS : -kSpinSpeedRadS;
  }

  return ControlTopicLogic::GateTarget(
      desired, encoder_topic_fresh,
      drive_mode == DriveMode::kLineFollowing, tracking_topic_fresh);
}

float AbsFloat(float value) { return value >= 0.0f ? value : -value; }

float ClampMotorOutput(float output)
{
  if (output > 1.0f)
  {
    return 1.0f;
  }
  if (output < -1.0f)
  {
    return -1.0f;
  }
  return output;
}

float CalculateSpeedFeedForward(uint8_t motor_index, float target_speed_rad_s)
{
  const float abs_target = AbsFloat(target_speed_rad_s);
  if (abs_target < kSpeedTargetEpsilon ||
      motor_index >= Module::MotorGroup::kMotorCount)
  {
    return 0.0f;
  }

  const auto& config = speed_feedforward_config[motor_index];
  const float direction = target_speed_rad_s > 0.0f ? 1.0f : -1.0f;
  const float output = config.static_output + config.velocity_gain * abs_target;
  return direction * ClampMotorOutput(output);
}

uint8_t ResolveButtonIndex(const char* alias)
{
  for (uint8_t i = 0; i < kButtonAliases.size(); ++i)
  {
    if (std::strcmp(alias, kButtonAliases[i]) == 0)
    {
      return i;
    }
  }
  return 0xFF;
}

float CalculateForwardDistanceM(const Module::Encoder::Sample& encoder_sample)
{
  float distance_sum = 0.0f;
  for (uint8_t i = 0; i < Module::Encoder::kMotorCount; ++i)
  {
    const float encoder_direction = static_cast<float>(kEncoderDirection[i]);
    distance_sum += static_cast<float>(encoder_sample.angle_rad[i]) *
                    encoder_direction * kWheelRadiusM;
  }

  const float distance =
      distance_sum / static_cast<float>(Module::Encoder::kMotorCount);
  return distance > 0.0f ? distance : 0.0f;
}

void ResetTrackingState(
    Tracking& tracking,
    LibXR::Topic::ASyncSubscriber<Tracking::Output>& tracking_subscriber,
    Tracking::Output& tracking_output, bool& has_tracking_sample)
{
  tracking.Reset();
  ControlTopicLogic::InvalidateCache(tracking_output, has_tracking_sample);
  if (tracking_subscriber.Available())
  {
    (void)tracking_subscriber.GetData();
  }
  tracking_subscriber.StartWaiting();
}

void ResetEncoderState(
    Module::Encoder& encoder,
    LibXR::Topic::ASyncSubscriber<Module::Encoder::Sample>& encoder_subscriber,
    Module::Encoder::Sample& encoder_sample, bool& has_encoder_sample)
{
  encoder.ResetAll();
  ControlTopicLogic::InvalidateCache(encoder_sample, has_encoder_sample);
  if (encoder_subscriber.Available())
  {
    (void)encoder_subscriber.GetData();
  }
  encoder_subscriber.StartWaiting();
}

}  // namespace CarControlSupport
