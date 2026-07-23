#include "MPU6050.hpp"
#include "NRF24L01.hpp"

void VerifyGeneratedModuleConstructors(LibXR::HardwareContainer& hw,
                                       LibXR::ApplicationManager& app) {
  MPU6050 mpu6050(
      hw, app, "mpu6050", 10, 100, 200, MPU6050::Filter::BAND_5HZ,
      MPU6050::GyroRange::DPS_250, MPU6050::AccelRange::G_2, 200, 1.2,
      20.0, 0.05, 30.0, 0.1, 0.002, 20, -2.3, 5.0, 0.0, 3e-05, 2048,
      LibXR::Thread::Priority::HIGH);
  NRF24L01 nrf24l01(
      hw, app, {17, 34, 51, 68, 85}, {17, 34, 51, 68, 85}, 2,
      NRF24L01::DataRate::MBPS_2, NRF24L01::OutputPower::ZERO_DBM, 250, 3,
      NRF24L01::PayloadMode::FIXED_32, "nrf24l01_tx", "nrf24l01_rx",
      "nrf24l01_status", 10, 10, 100, 1000, 1000, 1, 1536,
      LibXR::Thread::Priority::MEDIUM);
}
