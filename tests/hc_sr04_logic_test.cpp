#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#if !__has_include("module/HC_SR04/sr04_math.hpp")
#error "Missing production API: module/HC_SR04/sr04_math.hpp"
#else
#include "module/HC_SR04/sr04_math.hpp"

namespace
{

using Module::HC_SR04Logic::BuildNoEcho;
using Module::HC_SR04Logic::ClassifyDistance;
using Module::HC_SR04Logic::FilterState;
using Module::HC_SR04Logic::ProcessCapture;
using Module::HC_SR04Logic::Status;
using Module::HC_SR04Logic::TicksToMicroseconds;

void Expect(bool condition, const char* message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

void ExpectNear(float actual, float expected, float tolerance,
                const char* message)
{
  Expect(std::fabs(actual - expected) <= tolerance, message);
}

void TestTickScalingUsesCaptureClock()
{
  Expect(TicksToMicroseconds(1000U, 1000000U) == 1000U,
         "1 MHz capture must map one tick to one microsecond");
  Expect(TicksToMicroseconds(500U, 500000U) == 1000U,
         "capture conversion must honor the supplied clock");
}

void TestValidCaptureInitializesAndUpdatesEma()
{
  FilterState state;
  const auto first =
      ProcessCapture(1000U, 1000000U, 20.0F, 4000.0F, 0.6F, state);

  Expect(first.status == Status::VALID, "170 mm must be valid");
  ExpectNear(first.raw_distance_mm, 170.0F, 0.001F,
             "1000 us must convert to 170 mm");
  ExpectNear(first.filtered_distance_mm, 170.0F, 0.001F,
             "first valid capture must initialize the filter");

  const auto second =
      ProcessCapture(2000U, 1000000U, 20.0F, 4000.0F, 0.6F, state);

  Expect(second.status == Status::VALID, "340 mm must be valid");
  ExpectNear(second.raw_distance_mm, 340.0F, 0.001F,
             "2000 us must convert to 340 mm");
  ExpectNear(second.filtered_distance_mm, 238.0F, 0.001F,
             "second capture must use old*0.6 + raw*0.4");
}

void TestInvalidMeasurementsPreserveFilter()
{
  FilterState state;
  (void)ProcessCapture(1000U, 1000000U, 20.0F, 4000.0F, 0.6F, state);

  const auto below =
      ProcessCapture(50U, 1000000U, 20.0F, 4000.0F, 0.6F, state);
  const auto above =
      ProcessCapture(30000U, 1000000U, 20.0F, 4000.0F, 0.6F, state);
  const auto no_echo = BuildNoEcho(state);

  Expect(below.status == Status::BELOW_MIN, "50 us must be BELOW_MIN");
  Expect(above.status == Status::ABOVE_MAX, "30000 us must be ABOVE_MAX");
  Expect(no_echo.status == Status::NO_ECHO, "timeout must be NO_ECHO");
  ExpectNear(below.filtered_distance_mm, 170.0F, 0.001F,
             "BELOW_MIN must preserve the latest valid filter value");
  ExpectNear(above.filtered_distance_mm, 170.0F, 0.001F,
             "ABOVE_MAX must preserve the latest valid filter value");
  ExpectNear(no_echo.filtered_distance_mm, 170.0F, 0.001F,
             "NO_ECHO must preserve the latest valid filter value");
}

void TestDistanceBoundariesAreValid()
{
  Expect(ClassifyDistance(20.0F, 20.0F, 4000.0F) == Status::VALID,
         "minimum distance boundary must be valid");
  Expect(ClassifyDistance(4000.0F, 20.0F, 4000.0F) == Status::VALID,
         "maximum distance boundary must be valid");
}

}  // namespace

int main()
{
  try
  {
    TestTickScalingUsesCaptureClock();
    TestValidCaptureInitializesAndUpdatesEma();
    TestInvalidMeasurementsPreserveFilter();
    TestDistanceBoundariesAreValid();
  }
  catch (const std::exception& error)
  {
    std::cerr << "HC-SR04 logic test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "HC-SR04 logic tests passed.\n";
  return EXIT_SUCCESS;
}
#endif
