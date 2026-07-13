#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#if !__has_include("module/encoder/encoder_math.hpp")
#error "Missing production API: module/encoder/encoder_math.hpp"
#endif

#if !__has_include("module/NRF24L01/nrf24l01_state.hpp")
#error "Missing production API: module/NRF24L01/nrf24l01_state.hpp"
#endif

#if !__has_include("src/control_topic_logic.hpp")
#error "Missing production API: src/control_topic_logic.hpp"
#endif

#if __has_include("module/encoder/encoder_math.hpp") && \
    __has_include("module/NRF24L01/nrf24l01_state.hpp") && \
    __has_include("src/control_topic_logic.hpp")
#include "module/encoder/encoder_math.hpp"
#include "module/NRF24L01/nrf24l01_state.hpp"
#include "src/control_topic_logic.hpp"

namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr double kPiDouble = 3.14159265358979323846;

void ExpectNear(float actual, float expected, float tolerance,
                const std::string& message)
{
  if (std::fabs(actual - expected) <= tolerance)
  {
    return;
  }

  std::ostringstream error;
  error << message << ": expected " << expected << ", got " << actual;
  throw std::runtime_error(error.str());
}

void ExpectNearDouble(double actual, double expected, double tolerance,
                      const std::string& message)
{
  if (std::fabs(actual - expected) <= tolerance)
  {
    return;
  }

  std::ostringstream error;
  error << message << ": expected " << expected << ", got " << actual;
  throw std::runtime_error(error.str());
}

template <typename T>
void ExpectEqual(const T& actual, const T& expected, const std::string& message)
{
  if (actual != expected)
  {
    throw std::runtime_error(message);
  }
}

void TestTopicFreshnessUsesWrapSafeElapsedTime()
{
  constexpr std::uint32_t last =
      std::numeric_limits<std::uint32_t>::max() - 5U;
  constexpr std::uint32_t now = 3U;

  ExpectEqual(ControlTopicLogic::IsFresh(true, last, now, 10U), true,
              "wrapped nine millisecond sample must remain fresh");
  ExpectEqual(ControlTopicLogic::IsFresh(true, last, now, 9U), false,
              "sample must become stale at the timeout boundary");
  ExpectEqual(ControlTopicLogic::IsFresh(false, last, now, 10U), false,
              "missing sample must never be fresh");
}

void TestStaleEncoderAlwaysGatesTargetToZero()
{
  ExpectNear(ControlTopicLogic::GateTarget(2.5f, false, false, false), 0.0f,
             0.0f, "stale Encoder must gate every target to zero");
}

void TestStaleTrackingGatesLineFollowingTargetToZero()
{
  ExpectNear(ControlTopicLogic::GateTarget(2.5f, true, true, false), 0.0f,
             0.0f, "stale Tracking must gate line-following target to zero");
}

void TestStaleTrackingDoesNotGateSpinTarget()
{
  ExpectNear(ControlTopicLogic::GateTarget(-2.5f, true, false, false), -2.5f,
             0.0f, "spin target must not depend on Tracking freshness");
}

void TestCacheInvalidationClearsSampleAndPresenceFlag()
{
  struct Sample
  {
    int count;
    float speed;
  };

  Sample sample{42, 3.5f};
  bool has_sample = true;
  ControlTopicLogic::InvalidateCache(sample, has_sample);

  ExpectEqual(sample.count, 0, "cache invalidation must clear sample data");
  ExpectNear(sample.speed, 0.0f, 0.0f,
             "cache invalidation must value-initialize the sample");
  ExpectEqual(has_sample, false,
              "cache invalidation must clear the presence flag");
}

void TestSpeedControllerRequiresActiveModeAndFreshEncoder()
{
  ExpectEqual(ControlTopicLogic::ShouldRunSpeedController(true, true), true,
              "active mode with fresh Encoder must run the controller");
  ExpectEqual(ControlTopicLogic::ShouldRunSpeedController(false, true), false,
              "idle mode must not run the controller");
  ExpectEqual(ControlTopicLogic::ShouldRunSpeedController(true, false), false,
              "stale Encoder must not run the controller");
}

void TestEncoderFirstSampleKeepsAnglesAndHasZeroSpeed()
{
  const Module::EncoderMath::WheelCounts current_counts = {
      256, -256, 512, -512};

  const auto sample = Module::EncoderMath::BuildSample(
      current_counts, std::nullopt, 0.010f, 1024.0f);

  const std::array<float, 4> expected_angles = {
      kPi / 2.0f, -kPi / 2.0f, kPi, -kPi};
  for (std::size_t wheel = 0; wheel < expected_angles.size(); ++wheel)
  {
    ExpectNear(sample.angle_rad[wheel], expected_angles[wheel], 1e-5f,
               "first sample angle must be derived from current count");
    ExpectNear(sample.speed_rad_s[wheel], 0.0f, 1e-6f,
               "first sample speed must be zero without a previous sample");
  }
}

void TestEncoderFixedCountDeltasProduceExpectedSpeeds()
{
  const Module::EncoderMath::WheelCounts previous_counts = {
      100, 200, -100, -200};
  const Module::EncoderMath::WheelCounts current_counts = {
      110, 220, -130, -160};
  constexpr float dt_seconds = 0.020f;
  constexpr float counts_per_rev = 1000.0f;

  const auto sample = Module::EncoderMath::BuildSample(
      current_counts, previous_counts, dt_seconds, counts_per_rev);

  const std::array<std::int32_t, 4> count_deltas = {10, 20, -30, 40};
  for (std::size_t wheel = 0; wheel < count_deltas.size(); ++wheel)
  {
    const float expected_speed =
        static_cast<float>(count_deltas[wheel]) * 2.0f * kPi /
        counts_per_rev / dt_seconds;
    ExpectNear(sample.speed_rad_s[wheel], expected_speed, 1e-5f,
               "fixed count delta must convert to radians per second");
  }
}

void TestEncoderAllWheelsUseTheSameFrameDt()
{
  const Module::EncoderMath::WheelCounts previous_counts = {0, 0, 0, 0};
  const Module::EncoderMath::WheelCounts current_counts = {8, 16, 24, 32};
  constexpr float dt_seconds = 0.025f;
  constexpr float counts_per_rev = 800.0f;

  const auto sample = Module::EncoderMath::BuildSample(
      current_counts, previous_counts, dt_seconds, counts_per_rev);

  const float speed_per_count = 2.0f * kPi / counts_per_rev / dt_seconds;
  for (std::size_t wheel = 0; wheel < current_counts.size(); ++wheel)
  {
    ExpectNear(sample.speed_rad_s[wheel],
               static_cast<float>(current_counts[wheel]) * speed_per_count,
               1e-5f, "all four wheels must share the supplied frame dt");
  }
}

void TestEncoderSampleAfterCountResetIsZero()
{
  const Module::EncoderMath::WheelCounts reset_counts = {0, 0, 0, 0};

  const auto sample = Module::EncoderMath::BuildSample(
      reset_counts, reset_counts, 0.010f, 1024.0f);

  for (std::size_t wheel = 0; wheel < reset_counts.size(); ++wheel)
  {
    ExpectNear(sample.angle_rad[wheel], 0.0f, 1e-6f,
               "reset count must produce zero angle");
    ExpectNear(sample.speed_rad_s[wheel], 0.0f, 1e-6f,
               "reset current and previous counts must produce zero speed");
  }
}

void TestEncoderAnglesPreserveAdjacentLargeCounts()
{
  constexpr std::int32_t lower_count = 16777216;
  constexpr std::int32_t upper_count = 16777217;
  constexpr float counts_per_rev = 1024.0f;

  const auto lower_sample = Module::EncoderMath::BuildSample(
      {lower_count, 0, 0, 0}, std::nullopt, 0.010f, counts_per_rev);
  const auto upper_sample = Module::EncoderMath::BuildSample(
      {upper_count, 0, 0, 0}, std::nullopt, 0.010f, counts_per_rev);

  const double expected_step = 2.0 * kPiDouble / counts_per_rev;
  ExpectNearDouble(lower_sample.angle_rad[0],
                   static_cast<double>(lower_count) * expected_step, 1e-9,
                   "large count angle must retain double precision");
  ExpectNearDouble(upper_sample.angle_rad[0],
                   static_cast<double>(upper_count) * expected_step, 1e-9,
                   "adjacent large count angle must retain double precision");
  if (!(upper_sample.angle_rad[0] > lower_sample.angle_rad[0]))
  {
    throw std::runtime_error(
        "adjacent large counts must produce ordered distinct angles");
  }
}

void TestEncoderCountWrapProducesSinglePulseSpeed()
{
  const Module::EncoderMath::WheelCounts previous_counts = {
      std::numeric_limits<std::int32_t>::max(),
      std::numeric_limits<std::int32_t>::min(), 0, 0};
  const Module::EncoderMath::WheelCounts current_counts = {
      std::numeric_limits<std::int32_t>::min(),
      std::numeric_limits<std::int32_t>::max(), 0, 0};
  constexpr float dt_seconds = 0.010f;
  constexpr float counts_per_rev = 1024.0f;

  const auto sample = Module::EncoderMath::BuildSample(
      current_counts, previous_counts, dt_seconds, counts_per_rev);
  const float one_pulse_speed =
      2.0f * kPi / counts_per_rev / dt_seconds;

  ExpectNear(sample.speed_rad_s[0], one_pulse_speed, 1e-5f,
             "forward count wrap must be treated as one positive pulse");
  ExpectNear(sample.speed_rad_s[1], -one_pulse_speed, 1e-5f,
             "reverse count wrap must be treated as one negative pulse");
}

void TestEncoderNonFiniteInputsReturnSafeZeroSample()
{
  const Module::EncoderMath::WheelCounts previous_counts = {0, 0, 0, 0};
  const Module::EncoderMath::WheelCounts current_counts = {1, 2, 3, 4};
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  const std::array<Module::EncoderMath::Sample, 4> samples = {
      Module::EncoderMath::BuildSample(current_counts, previous_counts, nan,
                                       1024.0f),
      Module::EncoderMath::BuildSample(current_counts, previous_counts,
                                       infinity, 1024.0f),
      Module::EncoderMath::BuildSample(current_counts, previous_counts, 0.010f,
                                       nan),
      Module::EncoderMath::BuildSample(current_counts, previous_counts, 0.010f,
                                       infinity)};

  for (const auto& sample : samples)
  {
    for (std::size_t wheel = 0; wheel < current_counts.size(); ++wheel)
    {
      ExpectNearDouble(sample.angle_rad[wheel], 0.0, 0.0,
                       "non-finite input must produce zero angle");
      ExpectNear(sample.speed_rad_s[wheel], 0.0f, 0.0f,
                 "non-finite input must produce zero speed");
    }
  }
}

void TestNrfTxDsMeansSuccess()
{
  const auto decision = Module::Nrf24l01State::EvaluateTx(
      Module::Nrf24l01State::kTxDsMask, 20U, 100U);

  ExpectEqual(decision.outcome, Module::Nrf24l01State::TxOutcome::kSuccess,
              "TX_DS must report TX success");
  ExpectEqual(decision.recovery, Module::Nrf24l01State::RecoveryAction::kResumeRx,
              "successful TX must restore RX mode without reinitializing");
}

void TestNrfMaxRtMeansMaximumRetriesReached()
{
  const auto decision = Module::Nrf24l01State::EvaluateTx(
      Module::Nrf24l01State::kMaxRtMask, 20U, 100U);

  ExpectEqual(decision.outcome,
              Module::Nrf24l01State::TxOutcome::kMaximumRetries,
              "MAX_RT must report maximum retries reached");
  ExpectEqual(
      decision.recovery,
      Module::Nrf24l01State::RecoveryAction::kReinitializeAndResumeRx,
      "MAX_RT must request device reinitialization before resuming RX");
}

void TestNrfTxDsAndMaxRtTogetherAreInvalid()
{
  const std::uint8_t invalid_status = static_cast<std::uint8_t>(
      Module::Nrf24l01State::kTxDsMask |
      Module::Nrf24l01State::kMaxRtMask);
  const auto decision =
      Module::Nrf24l01State::EvaluateTx(invalid_status, 20U, 100U);

  ExpectEqual(decision.outcome,
              Module::Nrf24l01State::TxOutcome::kInvalidStatus,
              "TX_DS and MAX_RT set together must be rejected");
  ExpectEqual(
      decision.recovery,
      Module::Nrf24l01State::RecoveryAction::kReinitializeAndResumeRx,
      "invalid TX status must request reinitialization before resuming RX");
}

void TestNrfTimesOutAtOneHundredMilliseconds()
{
  const auto before_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 99U, 100U);
  const auto at_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 100U, 100U);

  ExpectEqual(before_timeout.outcome,
              Module::Nrf24l01State::TxOutcome::kPending,
              "TX must remain pending before 100 ms");
  ExpectEqual(at_timeout.outcome,
              Module::Nrf24l01State::TxOutcome::kTimeout,
              "TX must time out at 100 ms");
  ExpectEqual(
      at_timeout.recovery,
      Module::Nrf24l01State::RecoveryAction::kReinitializeAndResumeRx,
      "TX timeout must request reinitialization before resuming RX");
}

void TestNrfUsesCustomTimeoutBoundary()
{
  const auto before_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 24U, 25U);
  const auto at_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 25U, 25U);

  ExpectEqual(before_timeout.outcome,
              Module::Nrf24l01State::TxOutcome::kPending,
              "TX must remain pending before its configured timeout");
  ExpectEqual(at_timeout.outcome,
              Module::Nrf24l01State::TxOutcome::kTimeout,
              "TX must time out at its configured timeout");
}

}  // namespace

int main()
{
  try
  {
    TestEncoderFirstSampleKeepsAnglesAndHasZeroSpeed();
    TestEncoderFixedCountDeltasProduceExpectedSpeeds();
    TestEncoderAllWheelsUseTheSameFrameDt();
    TestEncoderSampleAfterCountResetIsZero();
    TestEncoderAnglesPreserveAdjacentLargeCounts();
    TestEncoderCountWrapProducesSinglePulseSpeed();
    TestEncoderNonFiniteInputsReturnSafeZeroSample();
    TestTopicFreshnessUsesWrapSafeElapsedTime();
    TestStaleEncoderAlwaysGatesTargetToZero();
    TestStaleTrackingGatesLineFollowingTargetToZero();
    TestStaleTrackingDoesNotGateSpinTarget();
    TestCacheInvalidationClearsSampleAndPresenceFlag();
    TestSpeedControllerRequiresActiveModeAndFreshEncoder();
    TestNrfTxDsMeansSuccess();
    TestNrfMaxRtMeansMaximumRetriesReached();
    TestNrfTxDsAndMaxRtTogetherAreInvalid();
    TestNrfTimesOutAtOneHundredMilliseconds();
    TestNrfUsesCustomTimeoutBoundary();
  }
  catch (const std::exception& error)
  {
    std::cerr << "topic module logic test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "topic module logic tests passed.\n";
  return EXIT_SUCCESS;
}
#endif
