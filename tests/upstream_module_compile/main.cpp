#include <cstddef>

#include "MPU6050.hpp"
#include "NRF24L01.hpp"

int main() {
  return static_cast<int>((sizeof(MPU6050) + sizeof(NRF24L01)) == 0U);
}
