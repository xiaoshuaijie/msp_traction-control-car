#pragma once

#include <array>
#include <cstdint>

#include "GreySensor.hpp"
#include "MPU6050.hpp"
#include "NRF24L01.hpp"
#include "Tracking.hpp"
#include "app_framework.hpp"
#include "encoder.hpp"
#include "sr04.h"

class XRobotModules
{
 public:
  // 模块装配 1. Topic 名称：统一生产者与消费者的连接契约，避免散落字符串。
  static constexpr const char* kGreyTopicName = "grey_sensor";
  static constexpr const char* kTrackingTopicName = "tracking";
  static constexpr const char* kImuTopicName = "mpu6050";
  static constexpr const char* kEncoderTopicName = "encoder";
  static constexpr const char* kNrfTxTopicName = "nrf24l01_tx";
  static constexpr const char* kNrfRxTopicName = "nrf24l01_rx";
  static constexpr const char* kNrfStatusTopicName = "nrf24l01_status";
  static constexpr const char* kUltrasonicTopicName = "hc_sr04";

  // 模块装配 2. 硬件别名：必须与 app_main 注册到 HardwareContainer 的名称一致。
  static constexpr std::array<const char*, GreySensor::MAX_CHANNEL_COUNT>
      kGreySensorAliases = {
          "grey_sensor_gpio_0", "grey_sensor_gpio_1", "grey_sensor_gpio_2",
          "grey_sensor_gpio_3", "grey_sensor_gpio_4", "grey_sensor_gpio_5",
          "grey_sensor_gpio_6", "grey_sensor_gpio_7"};
  static constexpr std::array<const char*, 3> kImuI2cAliases = {
      "i2c_mpu6050", "imu", "i2c2"};
  static constexpr std::array<const char*, 5> kNrfGpioAliases = {
      "nrf24l01_ce", "nrf24l01_csn", "nrf24l01_sck",
      "nrf24l01_mosi", "nrf24l01_miso"};

  // 模块装配 3. 集中配置：保留各子模块原生 Config，并补充模块间共享的 Topic 参数。
  struct Config
  {
    bool grey_active_low = false;
    const char* grey_topic_name = kGreyTopicName;
    uint32_t grey_publish_period_ms = 10U;
    const char* tracking_topic_name = kTrackingTopicName;
    Tracking::Config tracking{};
    MPU6050::Config imu{};
    Module::Encoder::Config encoder{};
    NRF24L01::Config radio{};
    HC_SR04::Config ultrasonic{};

    // 模块配置 1. 注入默认 Topic、编码器采样周期和无线 GPIO 别名。
    //             调用方仍可在构造 XRobotModules 前覆盖任意字段。
    Config()
    {
      imu.topic_name = kImuTopicName;
      encoder.topic_name = kEncoderTopicName;
      encoder.publish_period_ms = 5U;
      encoder.counts_per_rev = 1024.0F;
      radio.tx_topic_name = kNrfTxTopicName;
      radio.rx_topic_name = kNrfRxTopicName;
      radio.status_topic_name = kNrfStatusTopicName;
      radio.ce_gpio_name = kNrfGpioAliases[0];
      radio.csn_gpio_name = kNrfGpioAliases[1];
      radio.sck_gpio_name = kNrfGpioAliases[2];
      radio.mosi_gpio_name = kNrfGpioAliases[3];
      radio.miso_gpio_name = kNrfGpioAliases[4];
      ultrasonic.topic_name = kUltrasonicTopicName;
    }
  };

  // 模块装配 4. 默认入口：未提供配置时委托给完整构造函数使用统一默认值。
  XRobotModules(LibXR::HardwareContainer& hw,
                LibXR::ApplicationManager& app,
                const HC_SR04::Resources& ultrasonic_resources)
      : XRobotModules(hw, app, ultrasonic_resources, Config{})
  {
  }

  // 模块装配 5. 完整入口：按数据流构造 GreySensor -> Tracking，
  //           再装配 IMU、Encoder、NRF24L01 和 HC-SR04，并注册到同一管理器。
  XRobotModules(LibXR::HardwareContainer& hw,
                LibXR::ApplicationManager& app,
                const HC_SR04::Resources& ultrasonic_resources,
                const Config& config)
      : grey_sensor_(hw, app,
                     {kGreySensorAliases[0], kGreySensorAliases[1],
                      kGreySensorAliases[2], kGreySensorAliases[3],
                      kGreySensorAliases[4], kGreySensorAliases[5],
                      kGreySensorAliases[6], kGreySensorAliases[7]},
                     config.grey_active_low, config.grey_topic_name,
                     config.grey_publish_period_ms),
        tracking_(app, config.grey_topic_name, config.tracking_topic_name,
                  config.tracking),
        imu_(hw, app, config.imu),
        encoder_(hw, app, config.encoder),
        radio_(hw, app, config.radio),
        ultrasonic_(app, ultrasonic_resources, config.ultrasonic)
  {
  }

  // 模块装配 6. 模块持有硬件与管理器引用，禁止复制或移动以保证注册地址稳定。
  XRobotModules(const XRobotModules&) = delete;
  XRobotModules(XRobotModules&&) = delete;
  XRobotModules& operator=(const XRobotModules&) = delete;
  XRobotModules& operator=(XRobotModules&&) = delete;

  // 模块装配 7. 受控访问入口：业务层获取模块引用，但生命周期仍由本容器管理。
  GreySensor& GreySensorModule() { return grey_sensor_; }
  Tracking& TrackingModule() { return tracking_; }
  MPU6050& ImuModule() { return imu_; }
  Module::Encoder& EncoderModule() { return encoder_; }
  NRF24L01& RadioModule() { return radio_; }
  HC_SR04& UltrasonicModule() { return ultrasonic_; }

  const GreySensor& GreySensorModule() const { return grey_sensor_; }
  const Tracking& TrackingModule() const { return tracking_; }
  const MPU6050& ImuModule() const { return imu_; }
  const Module::Encoder& EncoderModule() const { return encoder_; }
  const NRF24L01& RadioModule() const { return radio_; }
  const HC_SR04& UltrasonicModule() const { return ultrasonic_; }

 private:
  // 模块装配 8. 成员声明顺序决定实际构造和析构顺序，并与传感到循迹的数据流一致。
  GreySensor grey_sensor_;
  Tracking tracking_;
  MPU6050 imu_;
  Module::Encoder encoder_;
  NRF24L01 radio_;
  HC_SR04 ultrasonic_;
};
