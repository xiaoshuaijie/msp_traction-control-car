#pragma once

#include <cstdint>

namespace Module::HC_SR04Logic
{

enum class Status : std::uint8_t
{
  NO_ECHO = 0,
  VALID,
  BELOW_MIN,
  ABOVE_MAX
};

struct Sample
{
  std::uint32_t pulse_width_us = 0;
  float raw_distance_mm = 0.0F;
  float filtered_distance_mm = 0.0F;
  Status status = Status::NO_ECHO;
  std::uint32_t sequence = 0;
};

struct FilterState
{
  float filtered_distance_mm = 0.0F;
  bool has_valid_sample = false;
};

inline std::uint32_t TicksToMicroseconds(std::uint32_t ticks,
                                         std::uint32_t capture_clock_hz)
{
  const std::uint64_t scaled =
      static_cast<std::uint64_t>(ticks) * 1000000ULL;
  return static_cast<std::uint32_t>(
      (scaled + capture_clock_hz / 2U) / capture_clock_hz);
}

inline Status ClassifyDistance(float distance_mm, float min_distance_mm,
                               float max_distance_mm)
{
  if (distance_mm < min_distance_mm)
  {
    return Status::BELOW_MIN;
  }
  if (distance_mm > max_distance_mm)
  {
    return Status::ABOVE_MAX;
  }
  return Status::VALID;
}

inline Sample ProcessCapture(std::uint32_t ticks,
                             std::uint32_t capture_clock_hz,
                             float min_distance_mm, float max_distance_mm,
                             float filter_old_weight, FilterState& state)
{
  Sample sample;
  sample.pulse_width_us = TicksToMicroseconds(ticks, capture_clock_hz);
  sample.raw_distance_mm =
      static_cast<float>(sample.pulse_width_us) * 0.17F;
  sample.status = ClassifyDistance(sample.raw_distance_mm, min_distance_mm,
                                   max_distance_mm);

  if (sample.status == Status::VALID)
  {
    state.filtered_distance_mm =
        state.has_valid_sample
            ? state.filtered_distance_mm * filter_old_weight +
                  sample.raw_distance_mm * (1.0F - filter_old_weight)
            : sample.raw_distance_mm;
    state.has_valid_sample = true;
  }

  sample.filtered_distance_mm = state.filtered_distance_mm;
  return sample;
}

inline Sample BuildNoEcho(const FilterState& state)
{
  Sample sample;
  sample.status = Status::NO_ECHO;
  sample.filtered_distance_mm = state.filtered_distance_mm;
  return sample;
}

}  // namespace Module::HC_SR04Logic
