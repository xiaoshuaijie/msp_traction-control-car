#pragma once

#include <cstdint>

namespace ControlTopicLogic
{

/**
 * Returns whether a timestamped sample is still usable.
 *
 * Args:
 *   has_sample: Whether any sample has been received.
 *   last_sample_time_ms: Timestamp of the newest sample.
 *   now_ms: Current timestamp.
 *   timeout_ms: Exclusive freshness timeout.
 */
constexpr bool IsFresh(bool has_sample, std::uint32_t last_sample_time_ms,
                       std::uint32_t now_ms, std::uint32_t timeout_ms)
{
  return has_sample &&
         static_cast<std::uint32_t>(now_ms - last_sample_time_ms) < timeout_ms;
}

/**
 * Gates a motor target using the sensor data required by its drive mode.
 *
 * Args:
 *   desired: Ungated motor speed target.
 *   encoder_fresh: Whether closed-loop feedback is fresh.
 *   tracking_required: Whether this target depends on Tracking output.
 *   tracking_fresh: Whether Tracking output is fresh.
 */
constexpr float GateTarget(float desired, bool encoder_fresh,
                           bool tracking_required, bool tracking_fresh)
{
  return (!encoder_fresh || (tracking_required && !tracking_fresh)) ? 0.0f
                                                                    : desired;
}

/** Returns whether the speed controller has an active, observable plant. */
constexpr bool ShouldRunSpeedController(bool active_mode, bool encoder_fresh)
{
  return active_mode && encoder_fresh;
}

/**
 * Invalidates a cached topic sample.
 *
 * Args:
 *   sample: Cached sample to value-initialize.
 *   has_sample: Presence flag to clear.
 */
template <typename Sample>
constexpr void InvalidateCache(Sample& sample, bool& has_sample)
{
  sample = {};
  has_sample = false;
}

}  // namespace ControlTopicLogic
