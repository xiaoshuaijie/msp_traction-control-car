#include <cstddef>

#include "MPU6050.hpp"
#include "NRF24L01.hpp"

std::size_t ModuleHeaderOdrA() {
  return sizeof(MPU6050) + sizeof(NRF24L01);
}
