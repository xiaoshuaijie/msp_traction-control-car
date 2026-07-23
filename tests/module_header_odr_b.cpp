#include <cstddef>

#include "MPU6050.hpp"
#include "NRF24L01.hpp"

std::size_t ModuleHeaderOdrB() {
  return sizeof(MPU6050::Sample) + sizeof(NRF24L01::Status);
}
