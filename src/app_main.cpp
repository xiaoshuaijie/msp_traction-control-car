#include "app_main.h"

#include <array>
#include <cstdint>

#include "BitsButtonXR.hpp"
#include "car_control_support.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_i2c.hpp"
#include "mspm0_pwm.hpp"
#include "mspm0_timebase.hpp"
#include "pid.hpp"
#include "tb6612.hpp"
#include "thread.hpp"
#include "ti_msp_dl_config.h"
#include "xrobot_main.hpp"

using namespace CarControlSupport;

/// 最近一次 GreySensor Topic 完整采样，供 Ozone 调试观察。
GreySensor::Sample grey_sensor_sample{};
/// 最近一次 Tracking Topic 完整输出，供 Ozone 调试观察。
Tracking::Output tracking_output{};
/// 最近一次 Encoder Topic 四轮采样，供 Ozone 调试观察。
Module::Encoder::Sample encoder_sample{};
/// 最近一次 HC-SR04 Topic 完整采样，供 Ozone 调试观察。
HC_SR04::Sample hc_sr04_sample{};
/// 最近一次 MPU6050 Topic 完整采样，供 Ozone 调试观察。
MPU6050::Sample mpu6050_sample{};
/// 最近一次 NRF24L01 接收 Topic 完整数据包，供 Ozone 调试观察。
NRF24L01::RxPacket nrf24l01_rx_packet{};
/// 最近一次 NRF24L01 状态 Topic 完整输出，供 Ozone 调试观察。
NRF24L01::Status nrf24l01_status{};
/// 最近一次被主循环消费的按键事件，供 Ozone 调试观察。
BitsButtonXR::ButtonEventResult button_event{};
/// 当前控制周期的聚合状态与四轮控制量，供 Ozone 调试观察。
CarControlSample car_control_sample{};

extern "C" void app_main()
{
  // 主流程 1. 时间与 I2C 基础：先建立系统时间基准，再准备 MPU6050 总线。
  //           PID 计时和 Thread::Sleep 都依赖 timebase，必须最先构造。
  LibXR::MSPM0Timebase timebase;
  UNUSED(timebase);

  // MSPM0I2C 使用调用方提供的暂存区，缓冲区必须与总线对象保持相同生命周期。
  std::array<uint8_t, MPU6050::BUFFER_SIZE> mpu6050_i2c_stage_buffer{};
  LibXR::MSPM0I2C mpu6050_i2c(MSPM0_I2C_INIT(
      I2C_0, mpu6050_i2c_stage_buffer.data(),
      mpu6050_i2c_stage_buffer.size(), 8));

  // 主流程 2. LED 心跳 GPIO：配置为推挽输出，方便观察主循环是否在跑。
  LibXR::MSPM0GPIO led_gpio(LED_PORT, LED_LED0_PIN, LED_LED0_IOMUX);
  led_gpio.SetConfig(
      {LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE});

  LibXR::MSPM0GPIO buzzer(BEEp_PORT , BEEp_PIN_0_PIN , BEEp_PIN_0_IOMUX );
  buzzer.SetConfig(
      {LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE});

  // 主流程 3. K1-K4 上拉按键：BitsButtonXR 用 GPIO 双沿中断唤醒事件状态机。
  LibXR::MSPM0GPIO key_gpio_1(KEY_PORT, KEY_k1_PIN, KEY_k1_IOMUX);
  LibXR::MSPM0GPIO key_gpio_2(KEY_PORT, KEY_k2_PIN, KEY_k2_IOMUX);
  LibXR::MSPM0GPIO key_gpio_3(KEY_PORT, KEY_k3_PIN, KEY_k3_IOMUX);
  LibXR::MSPM0GPIO key_gpio_4(KEY_PORT, KEY_k4_PIN, KEY_k4_IOMUX);

  // 主流程 4. 8 路灰度/循迹传感器：Line_OUT1 在车头右侧并映射到 bit0，
  //           Line_OUT8 在车头左侧并映射到 bit7。
  LibXR::MSPM0GPIO grey_sensor_gpio_0(Line_OUT1_PORT, Line_OUT1_PIN,
                                      Line_OUT1_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_1(Line_OUT2_PORT, Line_OUT2_PIN,
                                      Line_OUT2_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_2(Line_OUT3_PORT, Line_OUT3_PIN,
                                      Line_OUT3_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_3(Line_OUT4_PORT, Line_OUT4_PIN,
                                      Line_OUT4_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_4(Line_OUT5_PORT, Line_OUT5_PIN,
                                      Line_OUT5_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_5(Line_OUT6_PORT, Line_OUT6_PIN,
                                      Line_OUT6_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_6(Line_OUT7_PORT, Line_OUT7_PIN,
                                      Line_OUT7_IOMUX);
  LibXR::MSPM0GPIO grey_sensor_gpio_7(Line_OUT8_PORT, Line_OUT8_PIN,
                                      Line_OUT8_IOMUX);

  // 主流程 5. NRF24L01 软件 SPI：CE/CSN 控制收发状态，SCK/MOSI/MISO 传输数据。
  LibXR::MSPM0GPIO nrf24l01_ce_gpio(
      NRF24L01_PORT, NRF24L01_CE_PIN, NRF24L01_CE_IOMUX);
  LibXR::MSPM0GPIO nrf24l01_csn_gpio(
      NRF24L01_PORT, NRF24L01_CSN_PIN, NRF24L01_CSN_IOMUX);
  LibXR::MSPM0GPIO nrf24l01_sck_gpio(
      NRF24L01_PORT, NRF24L01_SCK_PIN, NRF24L01_SCK_IOMUX);
  LibXR::MSPM0GPIO nrf24l01_mosi_gpio(
      NRF24L01_PORT, NRF24L01_MOSI_PIN, NRF24L01_MOSI_IOMUX);
  LibXR::MSPM0GPIO nrf24l01_miso_gpio(
      NRF24L01_PORT, NRF24L01_MISO_PIN, NRF24L01_MISO_IOMUX);

  // 主流程 6. HC-SR04：TRIG 使用 SysConfig PWM，ECHO 使用 1 MHz 脉宽捕获。
  const HC_SR04::Resources ultrasonic_resources{
      MSPM0_PWM_INIT(PWM_ULTRASONIC, GPIO_PWM_ULTRASONIC_C0),
      CAPTURE_ULTRASONIC_INST,
      DL_TIMER_CC_0_INDEX,
      DL_TIMER_INTERRUPT_CC0_UP_EVENT,
      1000000U};

  // 主流程 7. 硬件与应用模块装配：先用稳定别名注册底层资源，
  //           再让按键和 XRobotModules 按名称取得依赖，避免直接耦合板级对象。
  LibXR::HardwareContainer hardware(
      LibXR::Entry<LibXR::I2C>{mpu6050_i2c,
                               {XRobotModules::kImuI2cAliases[0],
                                XRobotModules::kImuI2cAliases[1],
                                XRobotModules::kImuI2cAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_1, {kButtonAliases[0]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_2, {kButtonAliases[1]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_3, {kButtonAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_4, {kButtonAliases[3]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_0, {XRobotModules::kGreySensorAliases[0]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_1, {XRobotModules::kGreySensorAliases[1]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_2, {XRobotModules::kGreySensorAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_3, {XRobotModules::kGreySensorAliases[3]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_4, {XRobotModules::kGreySensorAliases[4]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_5, {XRobotModules::kGreySensorAliases[5]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_6, {XRobotModules::kGreySensorAliases[6]}},
      LibXR::Entry<LibXR::GPIO>{
          grey_sensor_gpio_7, {XRobotModules::kGreySensorAliases[7]}},
      LibXR::Entry<LibXR::GPIO>{
          nrf24l01_ce_gpio, {XRobotModules::kNrfGpioAliases[0]}},
      LibXR::Entry<LibXR::GPIO>{
          nrf24l01_csn_gpio, {XRobotModules::kNrfGpioAliases[1]}},
      LibXR::Entry<LibXR::GPIO>{
          nrf24l01_sck_gpio, {XRobotModules::kNrfGpioAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{
          nrf24l01_mosi_gpio, {XRobotModules::kNrfGpioAliases[3]}},
      LibXR::Entry<LibXR::GPIO>{
          nrf24l01_miso_gpio, {XRobotModules::kNrfGpioAliases[4]}});

  // ApplicationManager 统一轮询按键和各传感器模块的非阻塞状态机。
  LibXR::ApplicationManager app_manager;
  BitsButtonXR buttons(
      hardware, app_manager,
      {{kButtonAliases[0], false, kButtonConstraints},
       {kButtonAliases[1], false, kButtonConstraints},
       {kButtonAliases[2], false, kButtonConstraints},
       {kButtonAliases[3], false, kButtonConstraints}},
      {});

  // XRobotModules 集中构造所有 Topic/Application 模块；app_main 只保留控制入口。
  XRobotModules::Config config;
  config.grey_active_low = kGreySensorActiveLow;
  XRobotModules modules(hardware, app_manager, ultrasonic_resources, config);
  Tracking& tracking = modules.TrackingModule();
  Module::Encoder& encoder = modules.EncoderModule();
  NRF24L01& radio = modules.RadioModule();
  HC_SR04& ultrasonic = modules.UltrasonicModule();

  // 异步订阅器保存最近一次完整快照，时间戳用于阻止陈旧数据继续驱动电机。
  LibXR::Topic::ASyncSubscriber<GreySensor::Sample> grey_sensor_subscriber(
      config.grey_topic_name);
  LibXR::Topic::ASyncSubscriber<Tracking::Output> tracking_subscriber(
      config.tracking_topic_name);
  LibXR::Topic::ASyncSubscriber<Module::Encoder::Sample> encoder_subscriber(
      config.encoder.topic_name);
  LibXR::Topic::ASyncSubscriber<HC_SR04::Sample> hc_sr04_subscriber(
      config.ultrasonic.topic_name);
  LibXR::Topic::ASyncSubscriber<MPU6050::Sample> mpu6050_subscriber(
      config.imu.topic_name);
  LibXR::Topic::ASyncSubscriber<NRF24L01::RxPacket> nrf24l01_rx_subscriber(
      config.radio.rx_topic_name);
  LibXR::Topic::ASyncSubscriber<NRF24L01::Status> nrf24l01_status_subscriber(
      config.radio.status_topic_name);
  grey_sensor_subscriber.StartWaiting();
  tracking_subscriber.StartWaiting();
  encoder_subscriber.StartWaiting();
  hc_sr04_subscriber.StartWaiting();
  mpu6050_subscriber.StartWaiting();
  nrf24l01_rx_subscriber.StartWaiting();
  nrf24l01_status_subscriber.StartWaiting();
  tracking_output = tracking.GetLatestOutput();
  encoder_sample = encoder.LatestSample();
  hc_sr04_sample = ultrasonic.GetLatestSample();
  nrf24l01_status = radio.LatestStatus();
  uint32_t last_tracking_sample_time_ms = 0U;
  uint32_t last_encoder_sample_time_ms = 0U;
  bool has_tracking_sample = false;
  bool has_encoder_sample = false;
  car_control_sample.grey_sensor_active_low = kGreySensorActiveLow;

  // GreySensor 构造函数内部默认使用 Pull::NONE。这里在接入层显式设置上拉输入，
  // 有效电平极性由 kGreySensorActiveLow 控制，不修改 GreySensor 模块内部逻辑。
  std::array<LibXR::GPIO*, GreySensor::MAX_CHANNEL_COUNT> grey_sensor_gpios = {
      &grey_sensor_gpio_0, &grey_sensor_gpio_1, &grey_sensor_gpio_2,
      &grey_sensor_gpio_3, &grey_sensor_gpio_4, &grey_sensor_gpio_5,
      &grey_sensor_gpio_6, &grey_sensor_gpio_7};
  for (auto* gpio : grey_sensor_gpios)
  {
    gpio->SetConfig(kGreySensorInputConfig);
  }

  // 主流程 7. 四路 TB6612 电机驱动：初始化 10kHz PWM。
  Module::MotorGroup motors;
  motors.Init(10000);

  // 主流程 8. 速度环 PID：每个轮子一个 PID，使用全局 speed_pid_config 初始化。
  for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
  {
    speed_pid[i] = LibXR::PID<float>(speed_pid_config[i]);
  }

  // 主流程 9. 主循环状态变量：记录控制周期和 LED 心跳。
  uint32_t last_control_time = LibXR::Thread::GetTime();
  uint32_t last_blink_time = last_control_time;
  bool led_on = false;
  DriveMode drive_mode = DriveMode::kIdle;

  while (true)
  {
    // 主循环 1. 驱动应用模块，并接收各 Topic 的最新完整快照。
    app_manager.MonitorAll();
    const uint32_t now = LibXR::Thread::GetTime();
    if (grey_sensor_subscriber.Available())
    {
      grey_sensor_sample = grey_sensor_subscriber.GetData();
      grey_sensor_subscriber.StartWaiting();
    }
    if (encoder_subscriber.Available())
    {
      encoder_sample = encoder_subscriber.GetData();
      last_encoder_sample_time_ms = now;
      has_encoder_sample = true;
      encoder_subscriber.StartWaiting();
    }
    if (tracking_subscriber.Available())
    {
      tracking_output = tracking_subscriber.GetData();
      last_tracking_sample_time_ms = now;
      has_tracking_sample = true;
      tracking_subscriber.StartWaiting();
    }
    if (hc_sr04_subscriber.Available())
    {
      hc_sr04_sample = hc_sr04_subscriber.GetData();
      hc_sr04_subscriber.StartWaiting();
    }
    if (mpu6050_subscriber.Available())
    {
      mpu6050_sample = mpu6050_subscriber.GetData();
      mpu6050_subscriber.StartWaiting();
    }
    if (nrf24l01_rx_subscriber.Available())
    {
      nrf24l01_rx_packet = nrf24l01_rx_subscriber.GetData();
      nrf24l01_rx_subscriber.StartWaiting();
    }
    if (nrf24l01_status_subscriber.Available())
    {
      nrf24l01_status = nrf24l01_status_subscriber.GetData();
      nrf24l01_status_subscriber.StartWaiting();
    }
    const uint32_t elapsed_ms = now - last_control_time;
    car_control_sample.elapsed_ms = elapsed_ms;

    // 主循环 2. 按键事件控制：只消费 PRESSED 事件，避免长按期间反复触发动作。
    while (buttons.GetEventResult(button_event))
    {
      if (button_event.event_type != BitsButtonXR::ButtonEvent::PRESSED)
      {
        continue;
      }

      const uint8_t button_index = ResolveButtonIndex(button_event.key_alias);

      switch (button_index)
      {
        case 0:  // K1：编码器闭环前进 50m。
          drive_mode = DriveMode::kForwardDistance;
          car_control_sample.forward_distance_m = 0.0f;
          ResetEncoderState(encoder, encoder_subscriber, encoder_sample,
                            has_encoder_sample);
          ResetTrackingState(tracking, tracking_subscriber, tracking_output,
                             has_tracking_sample);
          ResetSpeedPids();
          last_control_time = now;
          break;

        case 1:  // K2：进入循迹模式。
          drive_mode = DriveMode::kLineFollowing;
          ResetTrackingState(tracking, tracking_subscriber, tracking_output,
                             has_tracking_sample);
          ResetSpeedPids();
          last_control_time = now;
          break;

        case 2:  // K3：退出所有运动模式并滑行停止。
          drive_mode = DriveMode::kIdle;
          ResetTrackingState(tracking, tracking_subscriber, tracking_output,
                             has_tracking_sample);
          ResetSpeedPids();
          motors.CoastAll();
          last_control_time = now;
          break;

        case 3:  // K4：原地旋转，再次按下停止。
          if (drive_mode == DriveMode::kSpinInPlace)
          {
            drive_mode = DriveMode::kIdle;
            motors.CoastAll();
            last_control_time = now;
          }
          else
          {
            drive_mode = DriveMode::kSpinInPlace;
            last_control_time = now;
          }
          ResetTrackingState(tracking, tracking_subscriber, tracking_output,
                             has_tracking_sample);
          ResetSpeedPids();
          break;

        default:
          break;
      }
    }

    // 主循环 3. 驾驶模式 + 速度闭环：固定周期执行，并检查 Topic 数据新鲜度。
    if (elapsed_ms >= kControlPeriodMs)
    {
      last_control_time = now;
      const float dt_s = static_cast<float>(elapsed_ms) * 0.001f;
      car_control_sample.control_time_ms = now;
      car_control_sample.dt_s = dt_s;
      const bool encoder_topic_fresh = ControlTopicLogic::IsFresh(
          has_encoder_sample, last_encoder_sample_time_ms, now,
          kTopicFreshnessTimeoutMs);
      const bool tracking_topic_fresh = ControlTopicLogic::IsFresh(
          has_tracking_sample, last_tracking_sample_time_ms, now,
          kTopicFreshnessTimeoutMs);
      car_control_sample.encoder_topic_ready = encoder_topic_fresh;
      car_control_sample.tracking_topic_ready = tracking_topic_fresh;

      // 主循环 4. 循迹观测：完整 Tracking 输出保存在 tracking_output，
      //           控制快照仅补充 Topic 新鲜度相关的综合丢线状态。
      car_control_sample.line_lost =
          !tracking_topic_fresh || tracking_output.lost_line;

      // 主循环 5. 定距前进：只用新鲜编码器快照累计路程，到达 50m 后滑行停止。
      if (drive_mode == DriveMode::kForwardDistance && encoder_topic_fresh)
      {
        car_control_sample.forward_distance_m =
            CalculateForwardDistanceM(encoder_sample);
        if (car_control_sample.forward_distance_m >=
            kForwardDistanceTargetM)
        {
          drive_mode = DriveMode::kIdle;
          ResetSpeedPids();
          motors.CoastAll();
        }
      }

      // 主循环 6. 四轮速度闭环：读取编码器，计算 PID 输出，再驱动电机。
      for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
      {
        auto mot_id = static_cast<Module::MotorGroup::MotorId>(i);

        // 主循环 7. 方向修正：统一编码器正方向和电机正输出。
        const float encoder_direction = static_cast<float>(kEncoderDirection[i]);
        const float motor_direction = static_cast<float>(kMotorOutputDirection[i]);

        float measured = encoder_topic_fresh
                             ? encoder_sample.speed_rad_s[i] * encoder_direction
                             : 0.0f;
        float target = ResolveTargetSpeed(
            drive_mode, i, tracking_output, encoder_topic_fresh,
            tracking_topic_fresh);
        float feedforward = 0.0f;
        float pid_correction = 0.0f;
        float motor_output = 0.0f;

        // 主循环 8. 根据驾驶模式和 Topic 新鲜度生成四轮目标速度。
        if (ControlTopicLogic::ShouldRunSpeedController(
                drive_mode != DriveMode::kIdle, encoder_topic_fresh))
        {
          if (AbsFloat(target) < kSpeedTargetEpsilon)
          {
            speed_pid[i].Reset();
          }
          else
          {
            feedforward = CalculateSpeedFeedForward(i, target);
            pid_correction = speed_pid[i].Calculate(target, measured, dt_s);
            const float control_output =
                ClampMotorOutput(feedforward + pid_correction);
            motor_output = control_output * motor_direction;
          }
        }
        else if (!encoder_topic_fresh)
        {
          speed_pid[i].Reset();
        }
        motors.SetOutput(mot_id, motor_output);  // motor_output 范围为 [-1, 1]

        // 主循环 9. 同步完整控制快照：方便 Ozone 按车轮查看闭环输入和输出。
        car_control_sample.target_speed[i] = target;
        car_control_sample.measured_speed[i] = measured;
        car_control_sample.feedforward_output[i] = feedforward;
        car_control_sample.pid_output[i] = pid_correction;
        car_control_sample.motor_output[i] = motor_output;
      }

    }
    //  motors.SetOutput(Module::MotorGroup::kFrontRight, 0.4f); 

    // 主循环 10. 驾驶模式由局部状态机持有，只向调试快照单向同步。
    car_control_sample.drive_mode = drive_mode;

    // 主循环 11. LED 心跳：每 200ms 翻转一次，确认主循环仍在运行。
    if (now - last_blink_time >= 200)
    {
      last_blink_time = now;
      led_on = !led_on;
      led_gpio.Write(led_on);
      buzzer.Write(true);
    }

    // 主循环 12. 主循环让出 1ms，避免空转占满 CPU。
    LibXR::Thread::Sleep(1);
  }
}
