#include "app_main.h"

#include <cstdint>

#include "encoder.hpp"
#include "find.hpp"
#include "key.hpp"
#include "line.hpp"
#include "mspm0_gpio.hpp"
#include "mspm0_timebase.hpp"
#include "pid_incremental.hpp"
#include "tb6612.hpp"
#include "thread.hpp"
#include "ti_msp_dl_config.h"

extern "C" {
// Ozone Watch 可直接添加这些符号，避免局部 C++ 对象在调试器里不可见。
Module::Encoder* volatile g_encoder_instance = nullptr;
volatile int32_t g_encoder_count[Module::Encoder::kMotorCount] = {};
volatile float g_encoder_angle[Module::Encoder::kMotorCount] = {};
volatile float g_encoder_speed[Module::Encoder::kMotorCount] = {};
}

namespace
{

/// 速度环控制周期（毫秒）。
constexpr uint32_t kControlPeriodMs = 5;

volatile uint8_t g_line_black_mask = 0;
volatile float g_line_error = 0.0f;
volatile bool g_line_lost = false;
volatile uint8_t g_key_raw_mask = 0;
volatile uint8_t g_key_stable_raw_mask = 0;
volatile uint8_t g_key_pressed_mask = 0;
volatile bool g_line_following_enabled = false;
volatile float g_target_speed[Module::MotorGroup::kMotorCount] = {};
volatile float g_measured_speed[Module::MotorGroup::kMotorCount] = {};
volatile float g_pid_output[Module::MotorGroup::kMotorCount] = {};

constexpr float kSpeedPidKp[Module::MotorGroup::kMotorCount] = {
    0.02f,  // FL
    0.06f,  // FR
    0.02f,  // BL
    0.02f,  // BR
};

constexpr float kSpeedPidKi[Module::MotorGroup::kMotorCount] = {
    0.05f,  // FL
    0.20f,  // FR
    0.05f,  // BL
    0.05f,  // BR
};

constexpr float kSpeedPidKd[Module::MotorGroup::kMotorCount] = {
    0.0f,  // FL
    0.0f,  // FR
    0.0f,  // BL
    0.0f,  // BR
};

/// 0=FL, 1=FR, 2=BL, 3=BR。方向反了就把对应项改为 -1。
constexpr int8_t kEncoderDirection[Module::MotorGroup::kMotorCount] = {
    1,   // FL
    -1,  // FR
    1,   // BL
    1,   // BR
};

constexpr int8_t kMotorOutputDirection[Module::MotorGroup::kMotorCount] = {
    1,   // FL
    -1,  // FR
    1,   // BL
    1,   // BR
};

/**
 * @brief 构造指定电机的速度环增量式 PID 参数。
 *
 * max_output = 1.0f，正好对应 TB6612 满占空比；增量式输出经内部累加并限幅后
 * 直接喂给 MotorGroup::SetOutput()。kp/ki/kd 按电机独立配置，需按实车整定。
 */
Jie::IncrementalPID::Config MakeSpeedPidConfig(Module::MotorGroup::MotorId motor_id)
{
  uint8_t index = static_cast<uint8_t>(motor_id);
  if (index >= Module::MotorGroup::kMotorCount)
  {
    index = 0;
  }

  Jie::IncrementalPID::Config cfg;
  cfg.kp = kSpeedPidKp[index];
  cfg.ki = kSpeedPidKi[index];
  cfg.kd = kSpeedPidKd[index];
  cfg.max_output = 1.0f;
  cfg.integral_limit = 1.0f;
  cfg.deadband = 0.0f;
  cfg.improve = Jie::PIDImprovement::IntegralLimit;
  return cfg;
}

}  // namespace

extern "C" void app_main()
{
  // 1. 时间基准：PID 计时和 Thread::Sleep 都依赖它，必须最先构造。
  LibXR::MSPM0Timebase timebase;
  UNUSED(timebase);

  // 2. LED 心跳，方便观察主循环是否在跑。
  LibXR::MSPM0GPIO led_gpio(LED_PORT, LED_LED0_PIN, LED_LED0_IOMUX);
  led_gpio.SetConfig(
      {LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE});

  // 3. K1-K4 上拉按键，Update() 输出消抖后的稳定按下状态。
  Module::Key key;

  // 4. 8 路循迹传感器，Line_OUT1 在车头右侧，Line_OUT8 在车头左侧。
  Module::LineTracker line_tracker;
  Jie::LineFollower follower;

  // 5. 编码器：构造后 Init() 配置 GPIO 双沿中断并开始 4 倍频计数。
  //    MSPM0GPIO 的 GROUP1_IRQHandler 负责把 GPIOB 中断分发到这里。
  Module::Encoder encoder;
  g_encoder_instance = &encoder;
  encoder.Init();

  // 6. 四路 TB6612 电机驱动，10kHz PWM。
  Module::MotorGroup motors;
  motors.Init(10000);

  // 7. 每个轮子一个速度环 PID。
  Jie::IncrementalPID speed_pid[Module::MotorGroup::kMotorCount];
  for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
  {
    auto mot_id = static_cast<Module::MotorGroup::MotorId>(i);
    speed_pid[i].Init(MakeSpeedPidConfig(mot_id));
  }

  uint32_t last_control_time = LibXR::Thread::GetTime();
  uint32_t last_blink_time = last_control_time;
  bool led_on = false;
  bool line_following_enabled = false;

  while (true)
  {
    uint32_t now = LibXR::Thread::GetTime();
    const uint32_t elapsed_ms = now - last_control_time;
    const auto key_state = key.Update(now);

    g_key_raw_mask = key_state.raw_mask;
    g_key_stable_raw_mask = key_state.stable_raw_mask;
    g_key_pressed_mask = key_state.pressed_mask;

    if (key_state.IsPressed(Module::Key::kK2))
    {
      line_following_enabled = false;
      follower.Reset();
      for (auto& pid : speed_pid)
      {
        pid.Reset();
      }
      motors.CoastAll();
    }
    else if (key_state.IsPressed(Module::Key::kK1) && !line_following_enabled)
    {
      line_following_enabled = true;
      follower.Reset();
      for (auto& pid : speed_pid)
      {
        pid.Reset();
      }
      last_control_time = now;
    }
    g_line_following_enabled = line_following_enabled;

    // —— 循迹 + 速度闭环：固定周期执行 ——
    if (elapsed_ms >= kControlPeriodMs)
    {
      last_control_time = now;
      const float dt_s = static_cast<float>(elapsed_ms) * 0.001f;
      const auto line_state = line_tracker.ReadState();
      const auto follow_output = follower.Update(line_state.black_mask, dt_s);

      g_line_black_mask = line_state.black_mask;
      g_line_error = follow_output.error;
      g_line_lost = follow_output.lost_line;

      for (uint8_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
      {
        auto enc_id = static_cast<Module::Encoder::MotorId>(i);
        auto mot_id = static_cast<Module::MotorGroup::MotorId>(i);

        const float encoder_direction = static_cast<float>(kEncoderDirection[i]);
        const float motor_direction = static_cast<float>(kMotorOutputDirection[i]);

        float measured = encoder.GetSpeed(enc_id) * encoder_direction;
        float target = 0.0f;
        float motor_output = 0.0f;

        if (line_following_enabled)
        {
          target = follow_output.wheel_speed_rad_s[i];  // 循迹生成的目标转速
          const float output = speed_pid[i].Calculate(target, measured);
          motor_output = output * motor_direction;
        }

        // motors.SetOutput(mot_id, motor_output);  // motor_output ∈ [-1, 1]

        g_encoder_count[i] = encoder.GetCount(enc_id) * kEncoderDirection[i];
        g_encoder_angle[i] = encoder.GetAngle(enc_id) * encoder_direction;
        g_encoder_speed[i] = measured;
        g_target_speed[i] = target;
        g_measured_speed[i] = measured;
        g_pid_output[i] = motor_output;
      }
    }

    // —— LED 心跳：每 200ms 翻转 ——
    if (now - last_blink_time >= 200)
    {
      last_blink_time = now;
      led_on = !led_on;
      led_gpio.Write(led_on);
    }

    LibXR::Thread::Sleep(1);
  }
}
