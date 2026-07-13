#pragma once

#include <cstdint>

namespace LibXR
{

class Thread
{
 public:
  /** Sets the deterministic millisecond time returned to the adapter. */
  static void SetTime(std::uint32_t time) { time_ = time; }

  /** Returns the current deterministic millisecond time. */
  static std::uint32_t GetTime() { return time_; }

 private:
  inline static std::uint32_t time_ = 0;
};

}  // namespace LibXR
