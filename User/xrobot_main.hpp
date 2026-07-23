#include "app_framework.hpp"
#include "libxr.hpp"

// Module headers
#include "MPU6050.hpp"
#include "NRF24L01.hpp"
#include "BitsButtonXR.hpp"
#include "GreySensor.hpp"
#include "EventBinder.hpp"

static void XRobotMain(LibXR::HardwareContainer &hw) {
  using namespace LibXR;
  ApplicationManager appmgr;

  // Auto-generated module instantiations
  static MPU6050 MPU6050_0(
      hw,
      appmgr,
      "mpu6050",
      10,
      100,
      200,
      MPU6050::Filter::BAND_5HZ,
      MPU6050::GyroRange::DPS_250,
      MPU6050::AccelRange::G_2,
      200,
      1.2,
      20.0,
      0.05,
      30.0,
      0.1,
      0.002,
      20,
      -2.3,
      5.0,
      0.0,
      3e-05,
      2048,
      LibXR::Thread::Priority::HIGH
  );
  static NRF24L01 NRF24L01_0(
      hw,
      appmgr,
      {17, 34, 51, 68, 85},
      {17, 34, 51, 68, 85},
      2,
      NRF24L01::DataRate::MBPS_2,
      NRF24L01::OutputPower::ZERO_DBM,
      250,
      3,
      NRF24L01::PayloadMode::FIXED_32,
      "nrf24l01_tx",
      "nrf24l01_rx",
      "nrf24l01_status",
      10,
      10,
      100,
      1000,
      1000,
      1,
      1536,
      LibXR::Thread::Priority::MEDIUM
  );
  static BitsButtonXR BitsButtonXR_0(
      hw,
      appmgr,
      {{"btn1", false, {50, 1000, 500, 300}}, {"btn2", false, {50, 1000, 500, 300}}, {"btn3", false, {50, 1000, 500, 300}}},
      {{"btn1_btn2", true, {"btn1", "btn2"}, {50, 1000, 500, 300}}}
  );
  static GreySensor GreySensor_0(
      hw,
      appmgr,
      {"grey_sensor_gpio_0", "grey_sensor_gpio_1", "grey_sensor_gpio_2", "grey_sensor_gpio_3", "grey_sensor_gpio_4", "grey_sensor_gpio_5", "grey_sensor_gpio_6", "grey_sensor_gpio_7"},
      false,
      "grey_sensor",
      10
  );
  static EventBinder EventBinder_0(
      hw,
      appmgr,
      {{"dr16", dr16}, {"cmd", cmd}},
      {{{{"dr16", DR16::SwitchPos::DR16_SW_R_POS_MID, "cmd", CMD::Mode::CMD_OP_CTRL}, {"dr16", DR16::SwitchPos::DR16_SW_R_POS_BOT, "cmd", CMD::Mode::CMD_OP_CTRL}}}}
  );

  while (true) {
    appmgr.MonitorAll();
    Thread::Sleep(1000);
  }
}