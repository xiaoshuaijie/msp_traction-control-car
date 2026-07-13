#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: Topic-driven line tracking controller that consumes GreySensor samples and publishes wheel speed targets
constructor_args:
  - app: ApplicationManager used to run OnMonitor()
  - grey_topic_name: "grey_sensor"
  - tracking_topic_name: "tracking"
template_args: []
required_hardware: []
depends:
  - GreySensor
=== END MANIFEST === */
// clang-format on

#include "GreySensor.hpp"
#include "app_framework.hpp"
#include "message.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief 基于 topic 通信的循迹控制模块。
 *
 * 三方协作关系：
 * - GreySensor 负责采集 8 路灰度 GPIO，并在自己的 OnMonitor() 中发布
 *   "grey_sensor" topic，消息类型为 GreySensor::Sample。
 * - Tracking 继承 LibXR::Application，注册到同一个 ApplicationManager 后，
 *   由 app_manager.MonitorAll() 周期调用 OnMonitor()。Tracking 在 OnMonitor()
 *   中非阻塞订阅 "grey_sensor"，把 active_mask 转换成循迹误差和四轮目标速度，
 *   再发布 "tracking" topic。
 * - app_main.cpp 不再直接调用 grey_sensor.Read() 做循迹，而是订阅
 *   "tracking"，在 K2 循迹模式下把 Tracking::Output::wheel_speed_rad_s
 *   交给四路速度闭环 PID。
 */
class Tracking : public LibXR::Application
{
 public:
  /**
   * @brief 循迹传感器通道数。
   *
   * 直接跟随 GreySensor::MAX_CHANNEL_COUNT，确保 Tracking 处理的 bit 位数量
   * 与 GreySensor::Sample::active_mask 的定义一致。
   */
  static constexpr size_t kSensorCount = GreySensor::MAX_CHANNEL_COUNT;

  /**
   * @brief 输出车轮数量。
   *
   * 当前底盘按四轮差速处理，数组下标顺序与 Module::MotorGroup 保持一致：
   * 0=前左，1=前右，2=后左，3=后右。
   */
  static constexpr size_t kWheelCount = 4;

  /**
   * @brief 循迹控制参数。
   *
   * 这些参数只决定 Tracking 输出的目标轮速，不直接控制电机 PWM。app_main.cpp
   * 会把目标轮速再交给编码器速度环 PID 和前馈控制。
   */
  struct Config
  {
    /// 黑线位于中心时的基础前进速度，单位 rad/s。
    float base_speed_rad_s = 7.0f;

    /// 正常循迹时单侧车轮目标速度上限，避免大误差时给速度环过高目标。
    float max_speed_rad_s = 10.0f;

    /// 丢线搜索速度，丢线时单侧车轮低速前进以找回黑线。
    float search_speed_rad_s = 3.0f;

    /// 循迹比例增益，误差越大，左右轮差速越大。
    float turn_kp = 1.6f;

    /// 循迹微分增益，默认关闭；高速摆动时可小幅启用。
    float turn_kd = 0.00f;

    /// 连续多少帧未检测到黑线后才确认丢线，用于过滤瞬时采样空洞。
    uint8_t lost_confirm_samples = 2;
  };

  /**
   * @brief Tracking 发布给 app_main.cpp 的完整循迹输出。
   *
   * 该结构体会被发布到 "tracking" topic。app_main.cpp 订阅该 topic 后，
   * 使用 black_mask/error/lost_line 做调试观测，使用 wheel_speed_rad_s
   * 作为 K2 循迹模式的四轮目标速度。
   */
  struct Output
  {
    /// GreySensor 采到的原始 GPIO 电平掩码，bit 为 1 表示该路当前为高电平。
    uint8_t raw_mask = 0;

    /// 本次计算使用的黑线检测掩码，来自 GreySensor::Sample::active_mask。
    uint8_t black_mask = 0;

    /// active_mask 中为 1 的通道数量，便于判断是否全白、压线或极性接反。
    uint8_t active_count = 0;

    /// 加权平均后的循迹误差；正值表示黑线偏右，负值表示黑线偏左。
    float error = 0.0f;

    /// true 表示 8 路灰度当前都没有检测到黑线，Tracking 正在输出丢线搜索速度。
    bool lost_line = false;

    /// 左侧两轮共同目标速度，单位 rad/s。
    float left_speed_rad_s = 0.0f;

    /// 右侧两轮共同目标速度，单位 rad/s。
    float right_speed_rad_s = 0.0f;

    /// 四轮目标速度，顺序为前左、前右、后左、后右。
    std::array<float, kWheelCount> wheel_speed_rad_s{};

    /// 对应 GreySensor::Sample::sequence，用于调试确认这帧输出来自哪帧传感器数据。
    uint32_t source_sequence = 0;

    /// Tracking 自己的输出递增序号，用于调试确认 "tracking" topic 是否持续发布。
    uint32_t sequence = 0;
  };

  /**
   * @brief 使用默认参数构造循迹模块。
   *
   * @param app 应用管理器。构造函数会把 Tracking 注册进去，之后
   *        app_manager.MonitorAll() 会周期调用 OnMonitor()。
   * @param grey_topic_name GreySensor 发布的输入 topic 名，默认 "grey_sensor"。
   * @param tracking_topic_name Tracking 输出 topic 名，默认 "tracking"。
   */
  explicit Tracking(LibXR::ApplicationManager& app,
                    const char* grey_topic_name = "grey_sensor",
                    const char* tracking_topic_name = "tracking");

  /**
   * @brief 使用指定参数构造循迹模块。
   */
  Tracking(LibXR::ApplicationManager& app, const char* grey_topic_name,
           const char* tracking_topic_name, const Config& config);

  /**
   * @brief 清空循迹历史状态。
   *
   * app_main.cpp 在 K2/K3/K4 等模式切换时调用该函数，避免上一轮循迹误差
   * 影响下一次丢线搜索方向。
   */
  void Reset();

  /**
   * @brief 更新循迹参数并清空历史状态。
   */
  void SetConfig(const Config& config);

  /**
   * @brief 获取当前循迹参数。
   */
  const Config& GetConfig() const;

  /**
   * @brief 获取最近一次发布的循迹输出。
   *
   * 该函数给 app_main.cpp 做兜底：如果本控制周期没有新的 "tracking" topic，
   * 主循环仍可沿用最近一次输出，避免目标速度突然归零。
   */
  const Output& GetLatestOutput() const;

  /**
   * @brief 根据黑线掩码和采样间隔计算循迹输出。
   *
   * @param black_mask GreySensor::Sample::active_mask，bit 为 1 表示该路检测到黑线。
   * @param dt_s 距离上次 Tracking 计算的时间，单位秒，仅 turn_kd 非 0 时影响微分项。
   */
  Output Calculate(uint8_t black_mask, float dt_s);

  /**
   * @brief ApplicationManager 周期回调。
   *
   * OnMonitor() 是 Tracking 与 libxr 应用框架的连接点：它从 "grey_sensor"
   * topic 取最新 GreySensor::Sample，调用 Calculate()，然后发布 "tracking"。
   */
  void OnMonitor() override;

 private:
  /**
   * @brief 8 路灰度传感器横向权重。
   *
   * 当前硬件约定：Line_OUT1/bit0 在车头右侧，Line_OUT8/bit7 在车头左侧。
   * 因此 bit0 权重为 +3.5，bit7 权重为 -3.5。误差为正时说明黑线偏右，
   * Tracking 会让左轮更快、右轮更慢，使车头向右修正。
   */
  static constexpr std::array<float, kSensorCount> kSensorWeights = {
      3.5f, 2.5f, 1.5f, 0.5f, -0.5f, -1.5f, -2.5f, -3.5f};

  /// 把左右侧目标速度整理成统一的 Output 结构。
  static Output MakeOutput(uint8_t black_mask, float error, bool lost_line,
                           float left_speed, float right_speed);

  /// 计算 active_mask 的加权平均误差。
  float CalculateError(uint8_t black_mask) const;

  /// 限制正常循迹速度范围；正常循迹不让单侧车轮反转。
  float ClampNormalSpeed(float speed) const;

  /// 生成前进式丢线恢复输出，根据最近一次有效误差决定向哪边找线。
  Output MakeRecoveryOutput(bool lost_line) const;

  /// 填充序号、缓存最新输出，并发布到 "tracking" topic。
  void Publish(Output output, const GreySensor::Sample& sample);

  /// 当前循迹参数。
  Config config_;

  /// 订阅 GreySensor 发布的 "grey_sensor" topic。
  LibXR::Topic::ASyncSubscriber<GreySensor::Sample> grey_subscriber_;

  /// 对外发布 Tracking::Output 的 "tracking" topic。
  LibXR::Topic tracking_topic_;

  /// 最近一次输出，供 app_main.cpp 在没有新 topic 帧时沿用。
  Output latest_output_{};

  /// Tracking 输出序号。
  uint32_t sequence_ = 0;

  /// 最近一次有效循迹误差，丢线搜索时用它判断搜索方向。
  float last_error_ = 0.0f;

  /// 是否已经有过有效误差；没有历史误差时丢线搜索默认向右侧方向找线。
  bool has_last_error_ = false;

  /// 最近一次有效循迹输出，单帧采样空洞时继续沿用以避免目标突变。
  Output last_valid_output_{};

  /// 连续未检测到黑线的样本数量。
  uint32_t lost_sample_count_ = 0;

  /// 最近一次处理 GreySensor sample 的系统时间，单位 ms。
  uint32_t last_update_ms_ = 0;

  /// 是否已经记录过 last_update_ms_。
  bool has_update_time_ = false;
};
