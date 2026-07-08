#include "app_main.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "BitsButtonXR.hpp"
#include "GreySensor.hpp"
#include "Tracking.hpp"
#include "encoder.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_timebase.hpp"
#include "pid.hpp"
#include "tb6612.hpp"
#include "thread.hpp"
#include "ti_msp_dl_config.h"

extern "C" {
// 全局 1. app_main() 局部对象调试指针：Ozone Watch 可直接添加这些符号。
//         这里只保留局部对象的入口，已经是全局对象的模块不再额外放指针。

// 全局 2. 主循环时间观测变量：用于确认控制周期和 dt 计算是否正常。
volatile uint32_t g_elapsed_ms = 0;
volatile float g_dt_s = 0.0f;
volatile float jie = 0.0f;
// 全局 3. 编码器观测缓存：按 FL/FR/BL/BR 顺序保存方向修正后的数据。
volatile int32_t g_encoder_count[Module::Encoder::kMotorCount] = {};
volatile float g_encoder_angle[Module::Encoder::kMotorCount] = {};
volatile float g_encoder_speed[Module::Encoder::kMotorCount] = {};

struct SpeedFeedForwardParam
{
  float static_output;
  float velocity_gain;
};

// 全局 4. 四路速度 PID 初始配置：按 FL/FR/BL/BR 顺序集中放置，供启动初始化使用。
LibXR::PID<float>::Param speed_pid_config[Module::MotorGroup::kMotorCount] = {
    {
        .k = 1.0f,
        .p = 0.36f,
        .i = 0.20f,
        .d = 0.0f,
        .i_limit = 0.5f,
        .out_limit = 0.45f,
        .cycle = false,
    }, // FL
    {
        .k = 1.0f,
        .p = 0.36f,
        .i = 0.20f,
        .d = 0.0f,
        .i_limit = 0.6f,
        .out_limit = 0.65f,
        .cycle = false,
    }, // FR: 现象为速度偏慢，先提高响应和闭环输出余量。
    {
        .k = 1.0f,
        .p = 0.36f,
        .i = 0.20f,
        .d = 0.0f,
        .i_limit = 0.5f,
        .out_limit = 0.45f,
        .cycle = false,
    }, // BL
    {
        .k = 1.0f,
        .p = 0.36f,
        .i = 0.20f,
        .d = 0.0f,
        .i_limit = 0.2f,
        .out_limit = 0.25f,
        .cycle = false,
    }, // BR: 现象为超调严重，先压低 P/I、积分和输出上限。
};
// 全局 5. 四路速度前馈配置：按 FL/FR/BL/BR 顺序，支持每个轮子独立调节。
SpeedFeedForwardParam speed_feedforward_config[Module::MotorGroup::kMotorCount] = {
    {.static_output = 0.18f, .velocity_gain = 0.075f}, // FL
    {.static_output = 0.28f, .velocity_gain = 0.085f}, // FR: 速度偏慢，增强前馈。
    {.static_output = 0.18f, .velocity_gain = 0.075f}, // BL
    {.static_output = 0.08f, .velocity_gain = 0.075f}, // BR: 超调严重，削弱前馈。
};
LibXR::PID<float> speed_pid[Module::MotorGroup::kMotorCount] = {
    LibXR::PID<float>(speed_pid_config[0]),
    LibXR::PID<float>(speed_pid_config[1]),
    LibXR::PID<float>(speed_pid_config[2]),
    LibXR::PID<float>(speed_pid_config[3]),
};

// 全局 6. 循迹状态观测变量：保存传感器黑线掩码、偏差和丢线状态。
volatile uint8_t g_line_raw_mask = 0;
volatile uint8_t g_line_black_mask = 0;
volatile uint8_t g_line_active_count = 0;
volatile float g_line_error = 0.0f;
volatile bool g_line_lost = false;
volatile bool g_line_following_enabled = false;
volatile uint8_t g_grey_sensor_active_low = 0;
volatile bool g_tracking_topic_ready = false;
volatile uint32_t g_tracking_sequence = 0;
volatile uint32_t g_tracking_source_sequence = 0;
volatile float g_tracking_left_speed = 0.0f;
volatile float g_tracking_right_speed = 0.0f;

// 全局 7. 按键与驾驶模式观测变量：方便在调试器中确认事件和模式切换。
volatile uint8_t g_drive_mode = 0;
volatile uint8_t g_last_button_index = 0xFF;
volatile uint8_t g_last_button_event = 0xFF;
volatile float g_forward_distance_m = 0.0f;

// 全局 8. 速度闭环观测变量：目标速度、实测速度、前馈、PID 修正和最终电机输出。
volatile float g_target_speed[Module::MotorGroup::kMotorCount] = {};
volatile float g_measured_speed[Module::MotorGroup::kMotorCount] = {};
volatile float g_feedforward_output[Module::MotorGroup::kMotorCount] = {};
volatile float g_pid_output[Module::MotorGroup::kMotorCount] = {};
volatile float g_motor_output[Module::MotorGroup::kMotorCount] = {};
}

namespace
{

// 内部 0. GreySensor 硬件别名：顺序必须与 Line_OUT1 -> bit0、Line_OUT8 -> bit7
//         的掩码位映射一致。
constexpr std::array<const char*, GreySensor::MAX_CHANNEL_COUNT> kGreySensorAliases = {
    "grey_sensor_gpio_0", "grey_sensor_gpio_1", "grey_sensor_gpio_2",
    "grey_sensor_gpio_3", "grey_sensor_gpio_4", "grey_sensor_gpio_5",
    "grey_sensor_gpio_6", "grey_sensor_gpio_7"};

constexpr std::array<const char*, 4> kButtonAliases = {"btn1", "btn2", "btn3",
                                                       "btn4"};

constexpr LibXR::GPIO::Configuration kGreySensorInputConfig = {
    LibXR::GPIO::Direction::INPUT, LibXR::GPIO::Pull::UP};

// 灰度传感器有效电平极性：
// false = 高电平表示检测到黑线；true = 低电平表示检测到黑线。
// 若 K2 后 g_line_black_mask 长期为 0xFF 且 g_line_error 接近 0，通常说明这里设反了。
constexpr bool kGreySensorActiveLow = false;

// 内部 1. 速度环控制周期：主循环每隔这个时间执行一次闭环计算。
constexpr uint32_t kControlPeriodMs = 5;

// 内部 1.1. 独立驾驶模式参数：K1 前进 50m、K4 原地旋转。
constexpr float kWheelRadiusM = 0.032f;          // 64mm 轮胎直径
constexpr float kForwardDistanceTargetM = 50.0f;
constexpr float kForwardSpeedRadS = 8.0f;
constexpr float kSpinSpeedRadS = 3.0f;
constexpr float kSpeedTargetEpsilon = 1e-3f;

constexpr BitsButtonXR::ButtonConstraints kButtonConstraints = {
    .short_press_time_ms = 50,
    .long_press_start_time_ms = 1000,
    .long_press_period_triger_ms = 500,
    .time_window_time_ms = 300,
};

enum class DriveMode : uint8_t
{
  kIdle = 0,
  kForwardDistance,
  kLineFollowing,
  kSpinInPlace,
};

// 内部 5. 编码器方向修正表：0=FL, 1=FR, 2=BL, 3=BR，反向就改为 -1。
constexpr int8_t kEncoderDirection[Module::MotorGroup::kMotorCount] = {
    1,   // FL
    -1,  // FR
    1,   // BL
    -1,  // BR
};

// 内部 6. 电机输出方向修正表：让正输出统一表示车轮向前转。
constexpr int8_t kMotorOutputDirection[Module::MotorGroup::kMotorCount] = {
    1,   // FL
    1,  // FR
    1,   // BL
    -1,   // BR
};

void ResetSpeedPids()
{
  for (auto& pid : speed_pid)
  {
    pid.Reset();
  }
}

bool IsLeftMotor(uint8_t index)
{
  return index == Module::MotorGroup::kFrontLeft ||
         index == Module::MotorGroup::kBackLeft;
}

float AbsFloat(float value)
{
  return value >= 0.0f ? value : -value;
}

float ClampMotorOutput(float output)
{
  if (output > 1.0f)
  {
    return 1.0f;
  }
  if (output < -1.0f)
  {
    return -1.0f;
  }
  return output;
}

float CalculateSpeedFeedForward(uint8_t motor_index, float target_speed_rad_s)
{
  const float abs_target = AbsFloat(target_speed_rad_s);
  if (abs_target < kSpeedTargetEpsilon ||
      motor_index >= Module::MotorGroup::kMotorCount)
  {
    return 0.0f;
  }

  const auto& config = speed_feedforward_config[motor_index];
  const float direction = target_speed_rad_s > 0.0f ? 1.0f : -1.0f;
  const float output = config.static_output + config.velocity_gain * abs_target;
  return direction * ClampMotorOutput(output);
}

uint8_t ResolveButtonIndex(const char* alias)
{
  for (uint8_t i = 0; i < kButtonAliases.size(); ++i)
  {
    if (std::strcmp(alias, kButtonAliases[i]) == 0)
    {
      return i;
    }
  }

  return 0xFF;
}

float CalculateForwardDistanceM(const Module::Encoder& encoder)
{
  float distance_sum = 0.0f;
  for (uint8_t i = 0; i < Module::Encoder::kMotorCount; ++i)
  {
    const auto enc_id = static_cast<Module::Encoder::MotorId>(i);
    const float encoder_direction = static_cast<float>(kEncoderDirection[i]);
    distance_sum += encoder.GetAngle(enc_id) * encoder_direction * kWheelRadiusM;
  }

  float distance = distance_sum / static_cast<float>(Module::Encoder::kMotorCount);
  return distance > 0.0f ? distance : 0.0f;
}

void ResetTrackingState(
    Tracking& tracking,
    LibXR::Topic::ASyncSubscriber<Tracking::Output>& tracking_subscriber)
{
  tracking.Reset();
  if (tracking_subscriber.Available())
  {
    (void)tracking_subscriber.GetData();
  }
  tracking_subscriber.StartWaiting();
}

}  // namespace

extern "C" void app_main()
{
  // 主流程 1. 时间基准：PID 计时和 Thread::Sleep 都依赖它，必须最先构造。
  LibXR::MSPM0Timebase timebase;
  UNUSED(timebase);

  // 主流程 2. LED 心跳 GPIO：配置为推挽输出，方便观察主循环是否在跑。
  LibXR::MSPM0GPIO led_gpio(LED_PORT, LED_LED0_PIN, LED_LED0_IOMUX);
  led_gpio.SetConfig(
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

  LibXR::HardwareContainer hardware(
      LibXR::Entry<LibXR::GPIO>{key_gpio_1, {kButtonAliases[0]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_2, {kButtonAliases[1]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_3, {kButtonAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{key_gpio_4, {kButtonAliases[3]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_0, {kGreySensorAliases[0]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_1, {kGreySensorAliases[1]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_2, {kGreySensorAliases[2]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_3, {kGreySensorAliases[3]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_4, {kGreySensorAliases[4]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_5, {kGreySensorAliases[5]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_6, {kGreySensorAliases[6]}},
      LibXR::Entry<LibXR::GPIO>{grey_sensor_gpio_7, {kGreySensorAliases[7]}});
  LibXR::ApplicationManager app_manager;
  BitsButtonXR buttons(
      hardware, app_manager,
      {{kButtonAliases[0], false, kButtonConstraints},
       {kButtonAliases[1], false, kButtonConstraints},
       {kButtonAliases[2], false, kButtonConstraints},
       {kButtonAliases[3], false, kButtonConstraints}},
      {});
  GreySensor grey_sensor(hardware, app_manager,
                         {kGreySensorAliases[0], kGreySensorAliases[1],
                          kGreySensorAliases[2], kGreySensorAliases[3],
                          kGreySensorAliases[4], kGreySensorAliases[5],
                          kGreySensorAliases[6], kGreySensorAliases[7]},
                         kGreySensorActiveLow);
  Tracking tracking(app_manager);
  LibXR::Topic::ASyncSubscriber<Tracking::Output> tracking_subscriber("tracking");
  tracking_subscriber.StartWaiting();
  g_grey_sensor_active_low = kGreySensorActiveLow ? 1U : 0U;

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

  // 主流程 5. 编码器：构造后 Init() 配置 GPIO 双沿中断并开始 4 倍频计数。
  //           MSPM0GPIO 的 GROUP1_IRQHandler 负责把 GPIOB 中断分发到这里。
  Module::Encoder encoder;
  encoder.Init();

  // 主流程 6. 四路 TB6612 电机驱动：初始化 10kHz PWM。
  Module::MotorGroup motors;
  motors.Init(10000);

  // 主流程 7. 速度环 PID：每个轮子一个 PID，使用全局 speed_pid_config 初始化。
  for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
  {
    speed_pid[i] = LibXR::PID<float>(speed_pid_config[i]);
  }

  // 主流程 8. 主循环状态变量：记录控制周期、LED 心跳和循迹使能状态。
  uint32_t last_control_time = LibXR::Thread::GetTime();
  uint32_t last_blink_time = last_control_time;
  bool led_on = false;
  DriveMode drive_mode = DriveMode::kIdle;

  while (true)
  {
    // 主循环 1. 更新时间并驱动应用模块。
    uint32_t now = LibXR::Thread::GetTime();
    app_manager.MonitorAll();
    const uint32_t elapsed_ms = now - last_control_time;
    g_elapsed_ms = elapsed_ms;

    // 主循环 2. 按键事件控制：只消费 PRESSED 事件，避免长按期间反复触发动作。
    BitsButtonXR::ButtonEventResult button_event{};
    while (buttons.GetEventResult(button_event))
    {
      g_last_button_event = static_cast<uint8_t>(button_event.event_type);
      if (button_event.event_type != BitsButtonXR::ButtonEvent::PRESSED)
      {
        continue;
      }

      const uint8_t button_index = ResolveButtonIndex(button_event.key_alias);
      g_last_button_index = button_index;

      switch (button_index)
      {
        case 0:  // K1：编码器闭环前进 50m。
          drive_mode = DriveMode::kForwardDistance;
          g_forward_distance_m = 0.0f;
          encoder.ResetAll();
          ResetTrackingState(tracking, tracking_subscriber);
          ResetSpeedPids();
          last_control_time = now;
          break;

        case 1:  // K2：进入循迹模式。
          drive_mode = DriveMode::kLineFollowing;
          ResetTrackingState(tracking, tracking_subscriber);
          ResetSpeedPids();
          last_control_time = now;
          break;

        case 2:  // K3：退出所有运动模式并滑行停止。
          drive_mode = DriveMode::kIdle;
          ResetTrackingState(tracking, tracking_subscriber);
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
          ResetTrackingState(tracking, tracking_subscriber);
          ResetSpeedPids();
          break;

        default:
          break;
      }
    }

    // 主循环 3. 驾驶模式 + 速度闭环：固定周期执行。
    if (elapsed_ms >= kControlPeriodMs)
    {
      last_control_time = now;
      const float dt_s = static_cast<float>(elapsed_ms) * 0.001f;
      g_dt_s = dt_s;
      Tracking::Output tracking_output = tracking.GetLatestOutput();
      g_tracking_topic_ready = tracking_subscriber.Available();
      if (g_tracking_topic_ready)
      {
        tracking_output = tracking_subscriber.GetData();
        tracking_subscriber.StartWaiting();
      }

      // 主循环 4. 循迹观测：保存黑线掩码、误差和丢线状态。
      g_line_raw_mask = tracking_output.raw_mask;
      g_line_black_mask = tracking_output.black_mask;
      g_line_active_count = tracking_output.active_count;
      g_tracking_sequence = tracking_output.sequence;
      g_tracking_source_sequence = tracking_output.source_sequence;
      g_tracking_left_speed = tracking_output.left_speed_rad_s;
      g_tracking_right_speed = tracking_output.right_speed_rad_s;
      if (drive_mode == DriveMode::kLineFollowing)
      {
        g_line_error = tracking_output.error;
        g_line_lost = tracking_output.lost_line;
      }
      else
      {
        g_line_error = 0.0f;
        g_line_lost = tracking_output.lost_line;
      }

      if (drive_mode == DriveMode::kForwardDistance)
      {
        g_forward_distance_m = CalculateForwardDistanceM(encoder);
        if (g_forward_distance_m >= kForwardDistanceTargetM)
        {
          drive_mode = DriveMode::kIdle;
          ResetSpeedPids();
          motors.CoastAll();
        }
      }

      // 主循环 5. 四轮速度闭环：读取编码器，计算 PID 输出，再驱动电机。
      for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
      {
        auto enc_id = static_cast<Module::Encoder::MotorId>(i);
        auto mot_id = static_cast<Module::MotorGroup::MotorId>(i);

        // 主循环 6. 方向修正：统一编码器正方向和电机正输出。
        const float encoder_direction = static_cast<float>(kEncoderDirection[i]);
        const float motor_direction = static_cast<float>(kMotorOutputDirection[i]);

        float measured = encoder.GetSpeed(enc_id) * encoder_direction;
        float target = 0.0f;
        float feedforward = 0.0f;
        float pid_correction = 0.0f;
        float motor_output = 0.0f;

        // 主循环 7. 根据驾驶模式生成四轮目标速度。
        if (drive_mode == DriveMode::kForwardDistance)
        {
          target = kForwardSpeedRadS;
        }
        else if (drive_mode == DriveMode::kLineFollowing)
        {
          target = tracking_output.wheel_speed_rad_s[i];  // Tracking topic 目标转速
        }
        else if (drive_mode == DriveMode::kSpinInPlace)
        {
          target = IsLeftMotor(i) ? kSpinSpeedRadS : -kSpinSpeedRadS;
        }

        if (drive_mode != DriveMode::kIdle)
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
        motors.SetOutput(mot_id, motor_output);  // motor_output 范围为 [-1, 1]

        // 主循环 8. 同步四路调试变量：方便 Ozone 查看闭环输入和输出。
        g_encoder_count[i] = encoder.GetCount(enc_id) * kEncoderDirection[i];
        g_encoder_angle[i] = encoder.GetAngle(enc_id) * encoder_direction;
        g_encoder_speed[i] = measured;
        g_target_speed[i] = target;
        g_measured_speed[i] = measured;
        g_feedforward_output[i] = feedforward;
        g_pid_output[i] = pid_correction;
        g_motor_output[i] = motor_output;
      }

    }
    //  motors.SetOutput(Module::MotorGroup::kFrontRight, 0.4f); 
    g_drive_mode = static_cast<uint8_t>(drive_mode);
    g_line_following_enabled = drive_mode == DriveMode::kLineFollowing;

    // 主循环 9. LED 心跳：每 200ms 翻转一次，确认主循环仍在运行。
    if (now - last_blink_time >= 200)
    {
      last_blink_time = now;
      led_on = !led_on;
      led_gpio.Write(led_on);
    }

    // 主循环 10. 主循环让出 1ms，避免空转占满 CPU。
    LibXR::Thread::Sleep(1);
  }
}
