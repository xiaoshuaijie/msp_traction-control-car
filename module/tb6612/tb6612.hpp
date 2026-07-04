#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "mspm0_gpio.hpp"
#include "mspm0_pwm.hpp"
#include "ti_msp_dl_config.h"

namespace Module
{

/**
 * @brief 单个 TB6612 通道驱动的直流电机。
 *
 * 每个电机由一路 PWM（调速）+ 两个方向 GPIO（IN1/IN2）组成：
 * - IN1=1, IN2=0 -> 正转
 * - IN1=0, IN2=1 -> 反转
 * - IN1=IN2=1    -> 刹车（短接制动）
 * - IN1=IN2=0    -> 滑行（高阻）
 *
 * SetOutput() 接受 [-1, 1] 的归一化指令：符号决定方向，绝对值决定 PWM 占空比。
 * 正好与 Jie::IncrementalPID 的限幅输出（max_output 设为 1.0）对接。
 */
class Motor
{
 public:
  /**
   * @brief 构造一个电机驱动。
   *
   * @param pwm_res PWM 通道资源（用 MSPM0PWM::Resources 直接给出 timer/channel/clock）。
   * @param in1_port IN1 方向脚端口。
   * @param in1_pin  IN1 方向脚引脚掩码。
   * @param in1_iomux IN1 方向脚 IOMUX 下标。
   * @param in2_port IN2 方向脚端口。
   * @param in2_pin  IN2 方向脚引脚掩码。
   * @param in2_iomux IN2 方向脚 IOMUX 下标。
   */
  Motor(LibXR::MSPM0PWM::Resources pwm_res, GPIO_Regs* in1_port, uint32_t in1_pin,
        uint32_t in1_iomux, GPIO_Regs* in2_port, uint32_t in2_pin, uint32_t in2_iomux)
      : pwm_(pwm_res),
        in1_(in1_port, in1_pin, in1_iomux),
        in2_(in2_port, in2_pin, in2_iomux)
  {
  }

  /**
   * @brief 配置方向脚为推挽输出、设置 PWM 频率并使能输出。构造后调用一次。
   *
   * @param pwm_freq_hz PWM 频率（Hz），TB6612 一般用 10~20kHz 避免可闻噪声。
   */
  void Init(uint32_t pwm_freq_hz = 10000)//10KHZ
  {
    const LibXR::GPIO::Configuration out_cfg = {
        LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE};
    in1_.SetConfig(out_cfg);
    in2_.SetConfig(out_cfg);

    pwm_.SetConfig({pwm_freq_hz});
    pwm_.SetDutyCycle(0.0f);
    pwm_.Enable();

    Coast();
  }

  /**
   * @brief 设置归一化输出。
   *
   * @param value 取值 [-1, 1]。正值正转，负值反转，0 滑行。超出范围会被截断。
   */
  void SetOutput(float value)
  {
    if (value > 1.0f)
    {
      value = 1.0f;
    }
    else if (value < -1.0f)
    {
      value = -1.0f;
    }

    if (value > 0.0f)
    {
      in1_.Write(true);
      in2_.Write(false);
      pwm_.SetDutyCycle(value);
    }
    else if (value < 0.0f)
    {
      in1_.Write(false);
      in2_.Write(true);
      pwm_.SetDutyCycle(-value);
    }
    else
    {
      Coast();
    }
  }

  /**
   * @brief 短接制动（IN1=IN2=1，占空比清零）。
   */
  void Brake()
  {
    in1_.Write(true);
    in2_.Write(true);
    pwm_.SetDutyCycle(0.0f);
  }

  /**
   * @brief 滑行/自由停止（IN1=IN2=0，占空比清零）。
   */
  void Coast()
  {
    in1_.Write(false);
    in2_.Write(false);
    pwm_.SetDutyCycle(0.0f);
  }

 private:
  LibXR::MSPM0PWM pwm_;
  LibXR::MSPM0GPIO in1_;
  LibXR::MSPM0GPIO in2_;
};

/**
 * @brief 四路 TB6612 电机的聚合驱动。
 *
 * 把四个 Motor 按 前左(FL)/前右(FR)/后左(BL)/后右(BR) 聚合，构造函数直接使用
 * SysConfig 生成的 PWM_0/PWM_1 与 tb6612_* 方向脚宏，无需外部传参，风格与
 * module/line/line.hpp、module/encoder/encoder.hpp 一致。
 *
 * 引脚映射（与 sysconfig/untitled.syscfg 对应）：
 *   FL: PWM_0 通道0(PA17), 方向 AIN1_F(PA14)/AIN2_F(PA18)
 *   FR: PWM_0 通道1(PA16), 方向 BIN1_F(PA12)/BIN2_F(PA13)
 *   BL: PWM_1 通道0(PB6),  方向 AIN1_B(PB8)/AIN2_B(PB9)
 *   BR: PWM_1 通道1(PB7),  方向 BIN1_B(PB23)/BIN2_B(PB26)
 *
 * MotorId 与 Module::Encoder::MotorId 的编号顺序保持一致，便于在上层一一配对。
 */
class MotorGroup
{
 public:
  /**
   * @brief 电机/车轮编号，作为各接口下标。
   */
  enum MotorId : uint8_t
  {
    kFrontLeft = 0,  ///< 前左 FL
    kFrontRight,     ///< 前右 FR
    kBackLeft,       ///< 后左 BL
    kBackRight,      ///< 后右 BR
    kMotorCount      ///< 电机数量（4）
  };

  MotorGroup()
      : motors_{{
            // FL: PWM_0 / CC0
            {{PWM_0_INST, GPIO_PWM_0_C0_IDX,
              static_cast<uint32_t>(PWM_0_INST_CLK_FREQ)},
             tb6612_AIN1_F_PORT, tb6612_AIN1_F_PIN, tb6612_AIN1_F_IOMUX,
             tb6612_AIN2_F_PORT, tb6612_AIN2_F_PIN, tb6612_AIN2_F_IOMUX},
            // FR: PWM_0 / CC1
            {{PWM_0_INST, GPIO_PWM_0_C1_IDX,
              static_cast<uint32_t>(PWM_0_INST_CLK_FREQ)},
             tb6612_BIN1_F_PORT, tb6612_BIN1_F_PIN, tb6612_BIN1_F_IOMUX,
             tb6612_BIN2_F_PORT, tb6612_BIN2_F_PIN, tb6612_BIN2_F_IOMUX},
            // BL: PWM_1 / CC0
            {{PWM_1_INST, GPIO_PWM_1_C0_IDX,
              static_cast<uint32_t>(PWM_1_INST_CLK_FREQ)},
             tb6612_AIN1_B_PORT, tb6612_AIN1_B_PIN, tb6612_AIN1_B_IOMUX,
             tb6612_AIN2_B_PORT, tb6612_AIN2_B_PIN, tb6612_AIN2_B_IOMUX},
            // BR: PWM_1 / CC1
            {{PWM_1_INST, GPIO_PWM_1_C1_IDX,
              static_cast<uint32_t>(PWM_1_INST_CLK_FREQ)},
             tb6612_BIN1_B_PORT, tb6612_BIN1_B_PIN, tb6612_BIN1_B_IOMUX,
             tb6612_BIN2_B_PORT, tb6612_BIN2_B_PIN, tb6612_BIN2_B_IOMUX},
        }}
  {
  }

  /**
   * @brief 初始化全部四个电机。构造后调用一次。
   *
   * @param pwm_freq_hz 所有通道统一的 PWM 频率（Hz）。
   */
  void Init(uint32_t pwm_freq_hz = 10000)
  {
    for (auto& m : motors_)
    {
      m.Init(pwm_freq_hz);
    }
  }

  /**
   * @brief 设置某个电机的归一化输出 [-1, 1]。
   */
  void SetOutput(MotorId id, float value)
  {
    if (id < kMotorCount)
    {
      motors_[id].SetOutput(value);
    }
  }

  /**
   * @brief 所有电机刹车。
   */
  void BrakeAll()
  {
    for (auto& m : motors_)
    {
      m.Brake();
    }
  }

  /**
   * @brief 所有电机滑行停止。
   */
  void CoastAll()
  {
    for (auto& m : motors_)
    {
      m.Coast();
    }
  }

 private:
  std::array<Motor, kMotorCount> motors_;
};

}  // namespace Module
