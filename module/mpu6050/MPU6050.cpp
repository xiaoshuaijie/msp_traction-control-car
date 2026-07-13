#include "MPU6050.hpp"

#include <cmath>
#include <limits>

#include "thread.hpp"

namespace
{
constexpr float DEG_TO_RAD = static_cast<float>(LibXR::PI / 180.0);  // 角度转弧度。
constexpr float RAD_TO_DEG = static_cast<float>(180.0 / LibXR::PI);  // 弧度转角度。
constexpr uint8_t WHO_AM_I_VALUE = 0x68;  // MPU6050 固定身份值。
}  // 匿名命名空间
// 默认构造路径委托给完整构造函数，保证注册和总线配置逻辑只有一份。
MPU6050::MPU6050(LibXR::HardwareContainer& hw,
                 LibXR::ApplicationManager& app)
    : MPU6050(hw, app, Config{})
{
}

MPU6050::MPU6050(LibXR::HardwareContainer& hw,
                 LibXR::ApplicationManager& app, const Config& config)
    : config_(config),
      sample_topic_(LibXR::Topic::CreateTopic<Sample>(config.topic_name))
{
  i2c_ = hw.FindOrExit<LibXR::I2C>({"i2c_mpu6050", "imu", "i2c2"});
  // 模块统一使用 400 kHz Fast-mode；配置失败表示硬件抽象层状态不可用。
  const LibXR::ErrorCode config_result =
      i2c_->SetConfig({.clock_speed = I2C_CLOCK});
  ASSERT(config_result == LibXR::ErrorCode::OK);
  // 同时注册调度入口和硬件容器别名，供周期调用及其他模块查找。
  app.Register(*this);
  hw.Register(LibXR::Entry<MPU6050>{*this, {"mpu6050", "imu_mpu6050"}});
}

LibXR::ErrorCode MPU6050::Probe(uint8_t* who_am_i)
{
  uint8_t value = 0;
  const LibXR::ErrorCode result = ReadRegister(Register::WHO_AM_I, value);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  if (who_am_i != nullptr)
  {
    *who_am_i = value;
  }
  // 总线读取成功但身份不符时返回 CHECK_ERR，以区别于 I2C 通信错误。
  return value == WHO_AM_I_VALUE ? LibXR::ErrorCode::OK
                                 : LibXR::ErrorCode::CHECK_ERR;
}

LibXR::ErrorCode MPU6050::ReadRawSample(RawSample& sample)
{
  const LibXR::ErrorCode result =
      i2c_->MemRead(I2C_ADDRESS, static_cast<uint8_t>(Register::ACCEL_XOUT_H),
                    {buffer_, 14}, read_op_, LibXR::I2C::MemAddrLength::BYTE_8);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // ACCEL_XOUT_H 起的 14 字节按加速度、温度、陀螺仪顺序连续排列。
  sample.acc_x = DecodeInt16(&buffer_[0]);
  sample.acc_y = DecodeInt16(&buffer_[2]);
  sample.acc_z = DecodeInt16(&buffer_[4]);
  sample.temp = DecodeInt16(&buffer_[6]);
  sample.gyro_x = DecodeInt16(&buffer_[8]);
  sample.gyro_y = DecodeInt16(&buffer_[10]);
  sample.gyro_z = DecodeInt16(&buffer_[12]);
  return LibXR::ErrorCode::OK;
}

LibXR::ErrorCode MPU6050::ReadSample(Sample& sample)
{
  RawSample raw;
  const LibXR::ErrorCode result = ReadRawSample(raw);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 物理量换算与姿态融合共用同一帧原始数据，避免时间不同步。
  ConvertRaw(raw, sample);
  UpdateAttitude(raw, sample);
  sample.calibrated = calibrated_;
  sample.calibration_count = calibration_count_;
  return LibXR::ErrorCode::OK;
}

void MPU6050::OnMonitor()
{
  const uint32_t now_ms = LibXR::Thread::GetTime();
  if (!initialized_)
  {
    if (static_cast<uint32_t>(now_ms - last_recovery_ms_) <
        config_.recovery_period_ms)
    {
      return;
    }
    // 初始化失败后限频重试，避免故障设备持续占用 I2C 总线。
    last_recovery_ms_ = now_ms;
    const LibXR::ErrorCode init_result = InitializeDevice();
    if (init_result != LibXR::ErrorCode::OK)
    {
      RecordFailure(init_result);
      return;
    }
    RecordSuccess();
  }
  // 标定与正常采样共用发布周期；无符号时间差可正确跨越计时器回绕。
  if (has_poll_attempted_ && config_.publish_period_ms != 0U &&
      static_cast<uint32_t>(now_ms - last_poll_ms_) <
          config_.publish_period_ms)
  {
    return;
  }
  // 首次轮询不受 last_poll_ms_ 初值影响，之后才执行周期门控。
  has_poll_attempted_ = true;
  last_poll_ms_ = now_ms;
  // 上电标定期间只累计 Z 轴零漂，不对外发布尚未校准的样本。
  if (!calibrated_)
  {
    const LibXR::ErrorCode result = AccumulateCalibrationSample();
    if (result != LibXR::ErrorCode::OK)
    {
      RecordFailure(result);
      return;
    }
    RecordSuccess();
    return;
  }
  // 仅完整读取和解算成功的样本才获得序号并发布到 Topic。
  Sample sample;
  const LibXR::ErrorCode result = ReadSample(sample);
  if (result != LibXR::ErrorCode::OK)
  {
    RecordFailure(result);
    return;
  }
  // sequence_ 采用自然回绕；has_valid_sample_ 表示至少成功发布过一次。
  sample.sequence = sequence_++;
  has_valid_sample_ = true;
  sample_topic_.Publish(sample);
  RecordSuccess();
}

float MPU6050::AccelRangeG(AccelRange range)
{
  switch (range)
  {
    case AccelRange::G_2:
      return 2.0F;
    case AccelRange::G_4:
      return 4.0F;
    case AccelRange::G_8:
      return 8.0F;
    case AccelRange::G_16:
      return 16.0F;
  }
  return 2.0F;
}

float MPU6050::GyroRangeDPS(GyroRange range)
{
  switch (range)
  {
    case GyroRange::DPS_250:
      return 250.0F;
    case GyroRange::DPS_500:
      return 500.0F;
    case GyroRange::DPS_1000:
      return 1000.0F;
    case GyroRange::DPS_2000:
      return 2000.0F;
  }
  return 250.0F;
}

float MPU6050::Clamp(float value, float min, float max)
{
  if (value < min)
  {
    return min;
  }
  if (value > max)
  {
    return max;
  }
  return value;
}

int16_t MPU6050::DecodeInt16(const uint8_t* bytes)
{
  // 先在无符号域拼接高低字节，再转换为二进制补码有符号值。
  const uint16_t value = static_cast<uint16_t>(bytes[0]) << 8U |
                         static_cast<uint16_t>(bytes[1]);
  return static_cast<int16_t>(value);
}

float MPU6050::ToTemperatureCelsius(int16_t raw)
{
  return static_cast<float>(raw) / 340.0F + 36.53F;
}

LibXR::ErrorCode MPU6050::ReadRegister(Register reg, uint8_t& value)
{
  return i2c_->MemRead(I2C_ADDRESS, static_cast<uint8_t>(reg), {&value, 1},
                       read_op_, LibXR::I2C::MemAddrLength::BYTE_8);
}

LibXR::ErrorCode MPU6050::WriteRegister(Register reg, uint8_t value)
{
  return i2c_->MemWrite(I2C_ADDRESS, static_cast<uint8_t>(reg), {&value, 1},
                        write_op_, LibXR::I2C::MemAddrLength::BYTE_8);
}

LibXR::ErrorCode MPU6050::InitializeDevice()
{
  uint8_t who_am_i = 0;
  LibXR::ErrorCode result = Probe(&who_am_i);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 写入 DEVICE_RESET 位执行软复位，并等待寄存器恢复到可访问状态。
  result = WriteRegister(Register::PWR_MGMT_1, 0x80);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  LibXR::Thread::Sleep(100);
  // 清除睡眠位；最终配置阶段再选择 X 轴陀螺 PLL 作为时钟源。
  result = WriteRegister(Register::PWR_MGMT_1, 0x00);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 内部基准频率为 1 kHz，限制输入范围可防止分频器溢出或下溢。
  uint16_t sample_rate = config_.sample_rate_hz;
  if (sample_rate > 1000U)
  {
    sample_rate = 1000U;
  }
  if (sample_rate < 4U)
  {
    sample_rate = 4U;
  }
  const uint8_t divider = static_cast<uint8_t>((1000U / sample_rate) - 1U);
  // 禁用中断/FIFO，使用轮询读取；量程和 DLPF 直接采用 Config 的寄存器值。
  const uint8_t setup[][2] = {
      {static_cast<uint8_t>(Register::SMPLRT_DIV), divider},
      {static_cast<uint8_t>(Register::INT_ENABLE), 0x00},
      {static_cast<uint8_t>(Register::CONFIG),
       static_cast<uint8_t>(config_.filter)},
      {static_cast<uint8_t>(Register::GYRO_CONFIG),
       static_cast<uint8_t>(config_.gyro_range)},
      {static_cast<uint8_t>(Register::ACCEL_CONFIG),
       static_cast<uint8_t>(config_.accel_range)},
      {static_cast<uint8_t>(Register::FIFO_EN), 0x00},
      {static_cast<uint8_t>(Register::USER_CTRL), 0x00},
      {static_cast<uint8_t>(Register::PWR_MGMT_1), 0x01},
  };
  // 任一寄存器写入失败都立即终止，避免在部分配置状态下产生错误数据。
  for (const auto& item : setup)
  {
    result = WriteRegister(static_cast<Register>(item[0]), item[1]);
    if (result != LibXR::ErrorCode::OK)
    {
      return result;
    }
  }
  // 芯片配置完成后重新开始标定和姿态积分，旧状态不可跨初始化周期复用。
  initialized_ = true;
  ResetRuntimeState();
  return LibXR::ErrorCode::OK;
}

LibXR::ErrorCode MPU6050::AccumulateCalibrationSample()
{
  if (config_.calibration_samples == 0U)
  {
    // 允许显式关闭标定，此时 Z 轴零偏按 0 处理并立即进入采样阶段。
    gyro_zero_z_ = 0.0F;
    calibrated_ = true;
    return LibXR::ErrorCode::OK;
  }

  RawSample raw;
  const LibXR::ErrorCode result = ReadRawSample(raw);
  if (result != LibXR::ErrorCode::OK)
  {
    return result;
  }
  // 假定上电标定期间设备静止，使用原始 Z 轴角速度的算术平均值作为零偏。
  calibration_sum_z_ += raw.gyro_z;
  calibration_count_++;
  if (calibration_count_ >= config_.calibration_samples)
  {
    gyro_zero_z_ =
        static_cast<float>(calibration_sum_z_) /
        static_cast<float>(config_.calibration_samples);
    calibrated_ = true;
  }
  return LibXR::ErrorCode::OK;
}

void MPU6050::ResetRuntimeState()
{
  calibrated_ = false;
  has_poll_attempted_ = false;
  calibration_sum_z_ = 0;
  calibration_count_ = 0;
  gyro_zero_z_ = 0.0F;
  quaternion_ = {};
  integral_fb_x_ = 0.0F;
  integral_fb_y_ = 0.0F;
  integral_fb_z_ = 0.0F;
  last_gz_rad_s_ = 0.0F;
  locked_yaw_deg_ = 0.0F;
  stable_count_ = 0;
  yaw_locked_ = false;
}

void MPU6050::ConvertRaw(const RawSample& raw, Sample& sample) const
{
  const float accel_scale = AccelRangeG(config_.accel_range) / 32768.0F;
  const float gyro_scale = GyroRangeDPS(config_.gyro_range) / 32768.0F;
  // 原始满量程映射到 [-range, range)，加速度再由 g 换算为 m/s^2。
  sample.acceleration = {
      static_cast<float>(raw.acc_x) * accel_scale *
          static_cast<float>(LibXR::STANDARD_GRAVITY),
      static_cast<float>(raw.acc_y) * accel_scale *
          static_cast<float>(LibXR::STANDARD_GRAVITY),
      static_cast<float>(raw.acc_z) * accel_scale *
          static_cast<float>(LibXR::STANDARD_GRAVITY),
  };
  sample.angular_velocity = {
      static_cast<float>(raw.gyro_x) * gyro_scale,
      static_cast<float>(raw.gyro_y) * gyro_scale,
      (static_cast<float>(raw.gyro_z) - gyro_zero_z_) * gyro_scale,
  };
  sample.temperature = ToTemperatureCelsius(raw.temp);
}

void MPU6050::UpdateAttitude(const RawSample& raw, Sample& sample)
{
  const float accel_scale = AccelRangeG(config_.accel_range) / 32768.0F;
  const float gyro_scale_rad =
      GyroRangeDPS(config_.gyro_range) / 32768.0F * DEG_TO_RAD;
  const float dt =
      config_.publish_period_ms == 0U
          ? 0.01F
          : static_cast<float>(config_.publish_period_ms) / 1000.0F;
  // 加速度保留 g 单位用于归一化，角速度统一转成姿态微分所需的 rad/s。
  float ax = static_cast<float>(raw.acc_x) * accel_scale;
  float ay = static_cast<float>(raw.acc_y) * accel_scale;
  float az = static_cast<float>(raw.acc_z) * accel_scale;
  float gx = static_cast<float>(raw.gyro_x) * gyro_scale_rad;
  float gy = static_cast<float>(raw.gyro_y) * gyro_scale_rad;
  float gz = (static_cast<float>(raw.gyro_z) - gyro_zero_z_) * gyro_scale_rad;
  // 高动态时切换另一组 PI 增益，使车辆冲击下的修正强度可独立调节。
  const float acc_norm = std::sqrt(ax * ax + ay * ay + az * az);
  const float two_kp = acc_norm > config_.high_accel_threshold_g
                           ? config_.high_two_kp
                           : config_.normal_two_kp;
  const float two_ki = acc_norm > config_.high_accel_threshold_g
                           ? config_.high_two_ki
                           : config_.normal_two_ki;
  // 仅在加速度模长有效时使用重力方向反馈，异常数据则退化为纯陀螺积分。
  if (std::isfinite(acc_norm) && acc_norm > 0.0001F)
  {
    const float recip_norm = 1.0F / acc_norm;
    ax *= recip_norm;
    ay *= recip_norm;
    az *= recip_norm;
    // 由当前四元数计算机体系下的预测重力方向（使用半向量形式）。
    const float half_vx =
        quaternion_.x * quaternion_.z - quaternion_.w * quaternion_.y;
    const float half_vy =
        quaternion_.w * quaternion_.x + quaternion_.y * quaternion_.z;
    const float half_vz =
        quaternion_.w * quaternion_.w - 0.5F +
        quaternion_.z * quaternion_.z;
    // 测量重力与预测重力的叉积即姿态误差，方向用于修正角速度。
    const float half_ex = ay * half_vz - az * half_vy;
    const float half_ey = az * half_vx - ax * half_vz;
    const float half_ez = ax * half_vy - ay * half_vx;
    // 积分项补偿长期陀螺偏置；Ki 为 0 时清零，防止旧积分残留。
    if (two_ki > 0.0F)
    {
      integral_fb_x_ += two_ki * half_ex * dt;
      integral_fb_y_ += two_ki * half_ey * dt;
      integral_fb_z_ += two_ki * half_ez * dt;
      gx += integral_fb_x_;
      gy += integral_fb_y_;
      gz += integral_fb_z_;
    }
    else
    {
      integral_fb_x_ = 0.0F;
      integral_fb_y_ = 0.0F;
      integral_fb_z_ = 0.0F;
    }
    // 比例项立即抑制重力方向误差，与积分项共同构成 Mahony PI 反馈。
    gx += two_kp * half_ex;
    gy += two_kp * half_ey;
    gz += two_kp * half_ez;
  }
  // Z 轴角速度连续变化很小时累计稳定次数；运动恢复后立即解除 yaw 锁定。
  if (std::fabs(gz - last_gz_rad_s_) < config_.gyro_stable_threshold_rad_s)
  {
    if (stable_count_ != std::numeric_limits<uint8_t>::max())
    {
      stable_count_++;
    }
  }
  else
  {
    stable_count_ = 0;
    yaw_locked_ = false;
  }
  last_gz_rad_s_ = gz;
  // 使用 q_dot = 0.5 * q ⊗ omega 计算四元数导数。
  const float q_dot_w =
      0.5F * (-quaternion_.x * gx - quaternion_.y * gy - quaternion_.z * gz);
  const float q_dot_x =
      0.5F * (quaternion_.w * gx + quaternion_.y * gz - quaternion_.z * gy);
  const float q_dot_y =
      0.5F * (quaternion_.w * gy - quaternion_.x * gz + quaternion_.z * gx);
  const float q_dot_z =
      0.5F * (quaternion_.w * gz + quaternion_.x * gy - quaternion_.y * gx);
  // 前向欧拉积分姿态，并在 Z 分量叠加可配置的经验漂移补偿。
  quaternion_.w += q_dot_w * dt;
  quaternion_.x += q_dot_x * dt;
  quaternion_.y += q_dot_y * dt;
  quaternion_.z += q_dot_z * dt + config_.yaw_drift_compensation;
  // 每周期归一化以抑制数值漂移；非法范数则恢复为单位四元数。
  const float q_norm =
      std::sqrt(quaternion_.w * quaternion_.w + quaternion_.x * quaternion_.x +
                quaternion_.y * quaternion_.y + quaternion_.z * quaternion_.z);
  if (!std::isfinite(q_norm) || q_norm <= 0.0001F)
  {
    quaternion_ = {};
  }
  else
  {
    const float recip_norm = 1.0F / q_norm;
    quaternion_.w *= recip_norm;
    quaternion_.x *= recip_norm;
    quaternion_.y *= recip_norm;
    quaternion_.z *= recip_norm;
  }
  // 按 ZYX 顺序换算欧拉角；Clamp 防止浮点误差令 asin 输入越界。
  const float roll =
      std::atan2(2.0F * (quaternion_.w * quaternion_.x +
                         quaternion_.y * quaternion_.z),
                 1.0F - 2.0F * (quaternion_.x * quaternion_.x +
                                 quaternion_.y * quaternion_.y)) *
          RAD_TO_DEG +
      config_.roll_offset_deg;
  const float pitch =
      std::asin(Clamp(2.0F * (quaternion_.w * quaternion_.y -
                              quaternion_.z * quaternion_.x),
                      -1.0F, 1.0F)) *
          RAD_TO_DEG +
      config_.pitch_offset_deg;
  const float yaw =
      std::atan2(2.0F * (quaternion_.w * quaternion_.z +
                         quaternion_.x * quaternion_.y),
                 1.0F - 2.0F * (quaternion_.y * quaternion_.y +
                                 quaternion_.z * quaternion_.z)) *
          RAD_TO_DEG +
      config_.yaw_offset_deg;
  // 稳定达到阈值时冻结 yaw 输出，减小静止状态下无磁力计导致的航向漂移。
  if (stable_count_ >= config_.yaw_stable_samples && !yaw_locked_)
  {
    yaw_locked_ = true;
    locked_yaw_deg_ = yaw;
  }
  // 四元数始终输出最新积分值；仅 yaw 欧拉角可能使用锁定值。
  sample.quaternion = quaternion_;
  sample.angle.roll = roll;
  sample.angle.pitch = pitch;
  sample.angle.yaw = yaw_locked_ ? locked_yaw_deg_ : yaw;
}

void MPU6050::RecordFailure(LibXR::ErrorCode error)
{
  last_error_ = error;
  if (consecutive_failures_ != std::numeric_limits<uint32_t>::max())
  {
    consecutive_failures_++;
  }
}

void MPU6050::RecordSuccess()
{
  last_error_ = LibXR::ErrorCode::OK;
  consecutive_failures_ = 0;
}
