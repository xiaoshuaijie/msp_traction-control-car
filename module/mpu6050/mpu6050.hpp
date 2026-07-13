#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: MPU6050 I2C IMU module with non-blocking calibration and Topic publishing
constructor_args:
  - config:
      topic_name: "mpu6050"
      publish_period_ms: 10
      recovery_period_ms: 100
      sample_rate_hz: 200
      filter: MPU6050::Filter::BAND_5HZ
      gyro_range: MPU6050::GyroRange::DPS_250
      accel_range: MPU6050::AccelRange::G_2
      calibration_samples: 200
      high_accel_threshold_g: 1.2
      normal_two_kp: 20.0
      normal_two_ki: 0.05
      high_two_kp: 30.0
      high_two_ki: 0.1
      gyro_stable_threshold_rad_s: 0.002
      yaw_stable_samples: 20
      roll_offset_deg: -2.3
      pitch_offset_deg: 5.0
      yaw_offset_deg: 0.0
      yaw_drift_compensation: 0.00003
template_args: []
required_hardware:
  - i2c_mpu6050/imu/i2c2
depends: []
=== END MANIFEST === */
// clang-format on

#include <cstddef>
#include <cstdint>

#include "app_framework.hpp"
#include "i2c.hpp"
#include "libxr_def.hpp"
#include "message.hpp"
#include "semaphore.hpp"

/**
 * @brief MPU6050 六轴 IMU 应用模块。
 *
 * @details 模块通过 LibXR I2C 访问 MPU6050，构造后注册到 ApplicationManager。
 * OnMonitor() 会先初始化芯片，再按配置周期累计 Z 轴零漂，校准完成后发布完整
 * Sample 到 Topic。I2C 失败时不发布半成品样本。
 */
class MPU6050 : public LibXR::Application
{
 public:
  static constexpr uint8_t I2C_ADDRESS = 0x68;     ///< AD0 接地时的 7 位地址。
  static constexpr uint32_t I2C_CLOCK = 400000;   ///< I2C Fast-mode 时钟，单位 Hz。
  static constexpr size_t BUFFER_SIZE = 32;       ///< 总线批量读取的共享缓冲区大小。

  /** @brief 数字低通滤波器带宽，对应 CONFIG 寄存器的 DLPF_CFG 位。 */
  enum class Filter : uint8_t
  {
    BAND_256HZ = 0x00,  ///< 最高带宽，响应快但高频噪声较多。
    BAND_186HZ = 0x01,  ///< 约 186 Hz 带宽。
    BAND_96HZ = 0x02,   ///< 约 96 Hz 带宽。
    BAND_43HZ = 0x03,   ///< 约 43 Hz 带宽。
    BAND_21HZ = 0x04,   ///< 约 21 Hz 带宽。
    BAND_10HZ = 0x05,   ///< 约 10 Hz 带宽。
    BAND_5HZ = 0x06,    ///< 最低带宽，适合低速车辆姿态估计。
  };

  /** @brief 陀螺仪满量程，对应 GYRO_CONFIG 寄存器的 FS_SEL 位。 */
  enum class GyroRange : uint8_t
  {
    DPS_250 = 0x00,   ///< 正负 250 deg/s，分辨率最高。
    DPS_500 = 0x08,   ///< 正负 500 deg/s。
    DPS_1000 = 0x10,  ///< 正负 1000 deg/s。
    DPS_2000 = 0x18,  ///< 正负 2000 deg/s，可测快速旋转。
  };

  /** @brief 加速度计满量程，对应 ACCEL_CONFIG 寄存器的 AFS_SEL 位。 */
  enum class AccelRange : uint8_t
  {
    G_2 = 0x00,   ///< 正负 2 g，分辨率最高。
    G_4 = 0x08,   ///< 正负 4 g。
    G_8 = 0x10,   ///< 正负 8 g。
    G_16 = 0x18,  ///< 正负 16 g，可承受较大冲击。
  };

  /**
   * @brief 模块运行参数。
   *
   * @details 配置在构造时复制，运行期间保持不变。增益名称沿用 Mahony
   * 姿态融合算法中的 2*Kp 和 2*Ki 记法。
   */
  struct Config
  {
    const char* topic_name = "mpu6050";  ///< 发布 Sample 的 Topic 名称。
    uint32_t publish_period_ms = 10;     ///< 采样及发布周期，0 表示每次轮询均执行。
    uint32_t recovery_period_ms = 100;   ///< 初始化失败后的重试间隔，单位 ms。
    uint16_t sample_rate_hz = 200;       ///< 芯片内部采样率，实际限制为 4~1000 Hz。
    Filter filter = Filter::BAND_5HZ;    ///< 数字低通滤波器带宽。
    GyroRange gyro_range = GyroRange::DPS_250;  ///< 陀螺仪满量程。
    AccelRange accel_range = AccelRange::G_2;   ///< 加速度计满量程。
    uint16_t calibration_samples = 200;  ///< 上电时用于估算 Z 轴零漂的样本数。
    float high_accel_threshold_g = 1.2F;  ///< 切换高动态融合增益的加速度模长阈值。
    float normal_two_kp = 20.0F;  ///< 正常状态下的比例反馈增益 2*Kp。
    float normal_two_ki = 0.05F;  ///< 正常状态下的积分反馈增益 2*Ki。
    float high_two_kp = 30.0F;    ///< 高动态状态下的比例反馈增益 2*Kp。
    float high_two_ki = 0.1F;     ///< 高动态状态下的积分反馈增益 2*Ki。
    float gyro_stable_threshold_rad_s = 0.002F;  ///< 相邻 Z 轴角速度的稳定阈值。
    uint8_t yaw_stable_samples = 20;  ///< 连续稳定多少次后锁定 yaw 输出。
    float roll_offset_deg = -2.3F;    ///< 横滚角安装误差补偿，单位 degree。
    float pitch_offset_deg = 5.0F;    ///< 俯仰角安装误差补偿，单位 degree。
    float yaw_offset_deg = 0.0F;      ///< 航向角安装误差补偿，单位 degree。
    float yaw_drift_compensation = 0.00003F;  ///< 每周期叠加到四元数 Z 分量的漂移补偿。
  };

  /** @brief 三维向量，分量含义由所在 Sample 字段决定。 */
  struct Vector3
  {
    float x = 0.0F;  ///< X 轴分量。
    float y = 0.0F;  ///< Y 轴分量。
    float z = 0.0F;  ///< Z 轴分量。
  };

  /** @brief ZYX 欧拉角，所有字段单位均为 degree。 */
  struct Angle
  {
    float roll = 0.0F;   ///< 绕 X 轴的横滚角。
    float pitch = 0.0F;  ///< 绕 Y 轴的俯仰角。
    float yaw = 0.0F;    ///< 绕 Z 轴的航向角。
  };

  /** @brief 姿态单位四元数，默认值表示无旋转。 */
  struct Quaternion
  {
    float w = 1.0F;  ///< 标量分量。
    float x = 0.0F;  ///< X 轴虚部。
    float y = 0.0F;  ///< Y 轴虚部。
    float z = 0.0F;  ///< Z 轴虚部。
  };

  /**
   * @brief 从芯片连续寄存器读取的原始有符号 16 位数据。
   * @note 字段尚未应用量程、零漂或单位换算。
   */
  struct RawSample
  {
    int16_t acc_x = 0;   ///< X 轴原始加速度。
    int16_t acc_y = 0;   ///< Y 轴原始加速度。
    int16_t acc_z = 0;   ///< Z 轴原始加速度。
    int16_t temp = 0;    ///< 芯片温度原始值。
    int16_t gyro_x = 0;  ///< X 轴原始角速度。
    int16_t gyro_y = 0;  ///< Y 轴原始角速度。
    int16_t gyro_z = 0;  ///< Z 轴原始角速度。
  };

  /** @brief 发布给业务层的一帧物理量与姿态解算结果。 */
  struct Sample
  {
    Vector3 acceleration;      ///< 三轴加速度，单位 m/s^2。
    Vector3 angular_velocity;  ///< 三轴角速度，单位 deg/s。
    Angle angle;               ///< 由融合四元数换算的欧拉角。
    Quaternion quaternion;     ///< 当前归一化姿态四元数。
    float temperature = 0.0F;  ///< 芯片内部温度，单位 degree Celsius。
    uint32_t sequence = 0;     ///< 成功发布序号，从 0 开始递增。
    uint32_t calibration_count = 0;  ///< 已累计的 Z 轴零漂样本数。
    bool calibrated = false;         ///< Z 轴零漂标定是否完成。
  };

  /**
   * @brief 使用默认配置创建并注册 MPU6050 模块。
   * @param hw 用于查找 I2C 外设并注册模块实例的硬件容器。
   * @param app 负责周期调用 OnMonitor() 的应用管理器。
   */
  MPU6050(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app);

  /**
   * @brief 使用指定配置创建并注册 MPU6050 模块。
   * @param hw 用于查找 I2C 外设并注册模块实例的硬件容器。
   * @param app 负责周期调用 OnMonitor() 的应用管理器。
   * @param config 采样、标定、滤波和姿态补偿参数。
   */
  MPU6050(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
          const Config& config);

  /** @brief 获取样本发布 Topic，供订阅者绑定。 */
  LibXR::Topic& SampleTopic() { return sample_topic_; }

  /**
   * @brief 读取 WHO_AM_I 寄存器并校验设备身份。
   * @param who_am_i 可选输出参数，返回芯片实际响应值。
   * @return 通信错误、CHECK_ERR（身份不符）或 OK。
   */
  LibXR::ErrorCode Probe(uint8_t* who_am_i = nullptr);

  /**
   * @brief 一次性读取加速度、温度和陀螺仪的 14 字节原始数据块。
   * @param sample 成功时接收按大端格式解码后的原始值。
   * @return I2C 批量读取结果。
   */
  LibXR::ErrorCode ReadRawSample(RawSample& sample);

  /**
   * @brief 读取并换算一帧物理量，同时更新姿态融合状态。
   * @param sample 成功时接收完整 Sample；调用方负责设置发布序号。
   * @return 原始数据读取结果。
   */
  LibXR::ErrorCode ReadSample(Sample& sample);

  /** @brief 获取最近一次初始化、标定或采样错误。 */
  [[nodiscard]] LibXR::ErrorCode LastError() const { return last_error_; }
  /** @brief 获取自最近一次成功操作以来的连续失败次数。 */
  [[nodiscard]] uint32_t ConsecutiveFailures() const
  {
    return consecutive_failures_;
  }
  /** @brief 判断模块是否至少成功发布过一帧样本。 */
  [[nodiscard]] bool HasValidSample() const { return has_valid_sample_; }
  /** @brief 判断上电 Z 轴零漂标定是否完成。 */
  [[nodiscard]] bool IsCalibrated() const { return calibrated_; }
  /** @brief 获取当前已累计的标定样本数。 */
  [[nodiscard]] uint32_t CalibrationCount() const { return calibration_count_; }

  /**
   * @brief 非阻塞周期入口，依次驱动初始化、标定、采样和 Topic 发布。
   * @details 初始化失败按 recovery_period_ms 重试；标定完成前不会发布样本。
   */
  void OnMonitor() override;

 private:
  /** @brief 本模块实际访问的 MPU6050 寄存器地址。 */
  enum class Register : uint8_t
  {
    SMPLRT_DIV = 0x19,   ///< 采样率分频。
    CONFIG = 0x1A,       ///< 数字低通滤波配置。
    GYRO_CONFIG = 0x1B,  ///< 陀螺仪满量程配置。
    ACCEL_CONFIG = 0x1C, ///< 加速度计满量程配置。
    FIFO_EN = 0x23,      ///< FIFO 数据源使能。
    INT_ENABLE = 0x38,   ///< 中断使能。
    ACCEL_XOUT_H = 0x3B, ///< 连续传感器数据块的首地址。
    USER_CTRL = 0x6A,    ///< FIFO、I2C 主机等功能控制。
    PWR_MGMT_1 = 0x6B,   ///< 复位、休眠和时钟源控制。
    WHO_AM_I = 0x75,     ///< 芯片身份寄存器。
  };

  /// 将量程枚举换算为加速度满量程，单位 g。
  static float AccelRangeG(AccelRange range);
  /// 将量程枚举换算为角速度满量程，单位 deg/s。
  static float GyroRangeDPS(GyroRange range);
  /// 将 value 限制在闭区间 [min, max]，用于保护反三角函数输入。
  static float Clamp(float value, float min, float max);
  /// 按 MPU6050 的大端字节序解码一个有符号 16 位整数。
  static int16_t DecodeInt16(const uint8_t* bytes);
  /// 按数据手册公式将温度原始值换算为 degree Celsius。
  static float ToTemperatureCelsius(int16_t raw);

  /// 读取单个 8 位寄存器。
  LibXR::ErrorCode ReadRegister(Register reg, uint8_t& value);
  /// 写入单个 8 位寄存器。
  LibXR::ErrorCode WriteRegister(Register reg, uint8_t value);
  /// 校验身份、复位芯片并写入采样率、滤波和量程配置。
  LibXR::ErrorCode InitializeDevice();
  /// 累计一个 Z 轴静止样本，并在达到目标数量后计算零漂均值。
  LibXR::ErrorCode AccumulateCalibrationSample();
  /// 清除与芯片初始化周期绑定的标定和姿态融合状态。
  void ResetRuntimeState();
  /// 把原始数据按当前量程换算为标准物理单位。
  void ConvertRaw(const RawSample& raw, Sample& sample) const;
  /// 使用加速度反馈修正陀螺积分，并生成四元数和欧拉角。
  void UpdateAttitude(const RawSample& raw, Sample& sample);
  /// 记录一次失败并对连续失败计数执行饱和递增。
  void RecordFailure(LibXR::ErrorCode error);
  /// 清除最近错误和连续失败计数。
  void RecordSuccess();

  // 外设依赖、配置和 LibXR 通信对象。
  LibXR::I2C* i2c_ = nullptr;
  Config config_;
  LibXR::Topic sample_topic_;
  uint8_t buffer_[BUFFER_SIZE] = {};
  LibXR::Semaphore read_sem_;
  LibXR::Semaphore write_sem_;
  LibXR::ReadOperation read_op_ = LibXR::ReadOperation(read_sem_, 32);
  LibXR::WriteOperation write_op_ = LibXR::WriteOperation(write_sem_, 32);

  // 调度、健康状态和发布序号；时间戳使用无符号减法以自然处理计数回绕。
  bool initialized_ = false;
  bool calibrated_ = false;
  bool has_valid_sample_ = false;
  bool has_poll_attempted_ = false;
  uint32_t last_poll_ms_ = 0;
  uint32_t last_recovery_ms_ = 0;
  uint32_t sequence_ = 0;
  uint32_t consecutive_failures_ = 0;
  LibXR::ErrorCode last_error_ = LibXR::ErrorCode::OK;

  // 上电标定状态：仅估算 Z 轴陀螺仪的静态零偏。
  int32_t calibration_sum_z_ = 0;
  uint32_t calibration_count_ = 0;
  float gyro_zero_z_ = 0.0F;

  // 姿态融合状态：四元数、PI 反馈积分项以及静止航向锁定数据。
  Quaternion quaternion_;
  float integral_fb_x_ = 0.0F;
  float integral_fb_y_ = 0.0F;
  float integral_fb_z_ = 0.0F;
  float last_gz_rad_s_ = 0.0F;
  float locked_yaw_deg_ = 0.0F;
  uint8_t stable_count_ = 0;
  bool yaw_locked_ = false;
};
