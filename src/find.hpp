#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Jie
{

/**
 * @brief 8 路循迹传感器到四轮目标速度的差速控制器。
 *
 * 输入使用 Module::LineTracker 的 black_mask 位映射：bit0 对应 Line_OUT1，
 * bit7 对应 Line_OUT8。当前小车安装方向为 Line_OUT1 在车头右侧、Line_OUT8
 * 在车头左侧，因此权重为右正左负。误差为正时，左轮目标速度提高、右轮目标
 * 速度降低，使车头向右修正。
 */
class LineFollower
{
 public:
  /**
   * @brief 循迹传感器通道数量。
   *
   * 与 Module::LineTracker::kSensorCount 保持一致。这里不直接依赖 line.hpp，
   * 是为了让 find.hpp 只保存纯控制算法，方便在主机侧单独编译测试。
   */
  static constexpr size_t kSensorCount = 8;

  /**
   * @brief 小车受控车轮数量。
   *
   * 当前底盘按四轮差速处理：前左/后左使用同一个左侧目标速度，前右/后右使用
   * 同一个右侧目标速度。
   */
  static constexpr size_t kWheelCount = 4;

  /**
   * @brief 循迹控制器参数。
   *
   * 所有速度单位均为 rad/s，直接作为编码器速度环 PID 的目标值。调车时一般先降低
   * base_speed_rad_s 和 search_speed_rad_s，确认传感器方向、电机方向和 PID 输出
   * 都正确后，再逐步提高速度和转向增益。
   */
  struct Config
  {
    /**
     * @brief 正常循迹时的基础前进速度。
     *
     * 当误差为 0 时，左右两侧车轮都会使用这个速度。数值越大，小车直线速度越快，
     * 但转弯和丢线恢复也会更难。
     */
    float base_speed_rad_s = 8.0f;

    /**
     * @brief 正常循迹时单侧车轮目标速度上限。
     *
     * 黑线偏到一侧时，转向修正会让一侧车轮加速、另一侧车轮减速；这里限制加速侧
     * 的最大目标速度，避免循迹误差很大时给速度环过高目标。
     */
    float max_speed_rad_s = 14.0f;

    /**
     * @brief 丢线搜索速度。
     *
     * 8 路传感器都没有检测到黑线时使用。控制器会根据上一次误差方向让左右轮反向
     * 低速转动，从而原地或小半径找回黑线。
     */
    float search_speed_rad_s = 3.0f;

    /**
     * @brief 循迹误差比例增益。
     *
     * correction = turn_kp * error + turn_kd * derivative。增大该值会让转向更积极，
     * 过大则可能在黑线两侧来回摆动。
     */
    float turn_kp = 1.6f;

    /**
     * @brief 循迹误差微分增益。
     *
     * 默认关闭微分项。实车高速运行且出现明显超调时，可以小幅增大该值，用误差变化
     * 速度来提前抑制摆动。
     */
    float turn_kd = 0.0f;
  };

  /**
   * @brief 一次循迹计算的完整输出。
   *
   * app_main.cpp 会把 wheel_speed_rad_s 交给四路速度 PID，同时把其它字段暴露为
   * volatile 调试变量，便于在调试器里观察传感器掩码、循迹误差和丢线状态。
   */
  struct Output
  {
    /**
     * @brief 本次参与计算的黑线检测掩码。
     *
     * 位为 1 表示对应传感器检测到黑线。bit0 是 Line_OUT1，bit7 是 Line_OUT8。
     */
    uint8_t black_mask = 0;

    /**
     * @brief 加权平均后的循迹误差。
     *
     * 当前安装方向下：正值表示黑线更靠车体右侧，负值表示黑线更靠车体左侧，0 表示
     * 黑线大致位于传感器中央。
     */
    float error = 0.0f;

    /**
     * @brief 是否进入丢线搜索模式。
     *
     * true 表示 black_mask 为 0，当前没有任意一路传感器检测到黑线。
     */
    bool lost_line = false;

    /**
     * @brief 左侧两轮共同目标速度。
     */
    float left_speed_rad_s = 0.0f;

    /**
     * @brief 右侧两轮共同目标速度。
     */
    float right_speed_rad_s = 0.0f;

    /**
     * @brief 四个车轮的目标速度数组。
     *
     * 下标顺序与 Module::MotorGroup::MotorId 保持一致：
     * 0=前左，1=前右，2=后左，3=后右。
     */
    std::array<float, kWheelCount> wheel_speed_rad_s{};
  };

  /**
   * @brief 使用默认参数构造循迹控制器。
   */
  LineFollower() = default;

  /**
   * @brief 使用指定参数构造循迹控制器。
   *
   * @param config 循迹速度、限幅和转向增益配置。
   */
  explicit LineFollower(const Config& config) : config_(config) {}

  /**
   * @brief 更新控制器参数并清空历史误差。
   *
   * 重新整定参数时清空历史误差，可以避免旧误差方向影响下一次丢线搜索，也避免微分项
   * 在参数切换后因为旧状态产生突变。
   *
   * @param config 新的循迹控制参数。
   */
  void SetConfig(const Config& config)
  {
    config_ = config;
    Reset();
  }

  /**
   * @brief 获取当前控制参数。
   *
   * @return 当前正在使用的配置引用。
   */
  const Config& GetConfig() const { return config_; }

  /**
   * @brief 清空控制器内部历史状态。
   *
   * 该函数不会改变 Config，只会清掉上一次误差和“是否已有历史误差”的标记。适合在
   * 小车重新放回赛道、重新开始一轮测试或修改传感器方向后调用。
   */
  void Reset()
  {
    last_error_ = 0.0f;
    has_last_error_ = false;
  }

  /**
   * @brief 根据 8 路循迹状态计算四轮目标速度。
   *
   * @param black_mask Module::LineTracker::State::black_mask，低电平有效转换后的黑线掩码。
   * @param dt_s 距离上次控制计算的时间，单位为秒；只在 turn_kd 非 0 时影响微分项。
   * @return 本次循迹输出，包含误差、丢线状态、左右速度和四轮速度。
   */
  Output Update(uint8_t black_mask, float dt_s)
  {
    if (black_mask == 0U)
    {
      // 没有传感器压到黑线时，不再继续使用正常差速公式，而是进入低速找线。
      return MakeSearchOutput();
    }

    const float error = CalculateError(black_mask);
    float derivative = 0.0f;
    if (has_last_error_ && dt_s > 1e-6f)
    {
      derivative = (error - last_error_) / dt_s;
    }

    const float correction = config_.turn_kp * error + config_.turn_kd * derivative;

    // 误差为正表示黑线偏右：左侧加速、右侧减速，车头向右修正。
    const float left_speed = ClampNormalSpeed(config_.base_speed_rad_s + correction);
    const float right_speed = ClampNormalSpeed(config_.base_speed_rad_s - correction);

    last_error_ = error;
    has_last_error_ = true;

    return MakeOutput(black_mask, error, false, left_speed, right_speed);
  }

 private:
  /**
   * @brief 8 路传感器的横向位置权重。
   *
   * 当前硬件安装方向为 Line_OUT1 在右侧、Line_OUT8 在左侧，所以 bit0 权重最大且
   * 为正，bit7 权重最小且为负。若实车转向方向反了，优先检查这里的符号或传感器
   * 实际安装方向。
   */
  static constexpr std::array<float, kSensorCount> kSensorWeights = {
      3.5f, 2.5f, 1.5f, 0.5f, -0.5f, -1.5f, -2.5f, -3.5f};

  /**
   * @brief 当前控制参数。
   */
  Config config_{};

  /**
   * @brief 上一次有效循迹误差。
   *
   * 仅在检测到黑线时更新。丢线时保留该值，用于判断应该朝哪一侧搜索。
   */
  float last_error_ = 0.0f;

  /**
   * @brief 是否已经记录过一次有效误差。
   *
   * 上电后如果还没有任何一次有效黑线检测，丢线搜索默认按正方向搜索。
   */
  bool has_last_error_ = false;

  /**
   * @brief 把左右速度整理成统一 Output 结构。
   *
   * @param black_mask 本次黑线检测掩码。
   * @param error 本次或最近一次循迹误差。
   * @param lost_line 是否为丢线搜索输出。
   * @param left_speed 左侧两轮目标速度。
   * @param right_speed 右侧两轮目标速度。
   * @return 填充好的循迹输出。
   */
  static Output MakeOutput(uint8_t black_mask, float error, bool lost_line,
                           float left_speed, float right_speed)
  {
    Output output;
    output.black_mask = black_mask;
    output.error = error;
    output.lost_line = lost_line;
    output.left_speed_rad_s = left_speed;
    output.right_speed_rad_s = right_speed;
    output.wheel_speed_rad_s = {left_speed, right_speed, left_speed, right_speed};
    return output;
  }

  /**
   * @brief 通过黑线掩码计算横向误差。
   *
   * 多个传感器同时检测到黑线时使用权重平均值，而不是只取最左或最右一路。这样在
   * 黑线较宽、传感器压到边缘或经过交叉线时，误差会更平滑。
   *
   * @param black_mask 黑线检测掩码，至少有一位为 1。
   * @return 加权平均后的循迹误差。
   */
  float CalculateError(uint8_t black_mask) const
  {
    float weighted_sum = 0.0f;
    float active_count = 0.0f;

    for (size_t i = 0; i < kSensorCount; ++i)
    {
      if ((black_mask & static_cast<uint8_t>(1U << i)) != 0U)
      {
        weighted_sum += kSensorWeights[i];
        active_count += 1.0f;
      }
    }

    return active_count > 0.0f ? weighted_sum / active_count : 0.0f;
  }

  /**
   * @brief 限制正常循迹速度范围。
   *
   * 正常循迹模式下不允许目标速度为负，避免大误差时直接让一侧轮子反转；丢线搜索
   * 需要反转时会绕过该函数，由 MakeSearchOutput() 单独生成。
   *
   * @param speed 待限幅的目标速度。
   * @return 限制在 [0, max_speed_rad_s] 内的速度。
   */
  float ClampNormalSpeed(float speed) const
  {
    if (speed < 0.0f)
    {
      return 0.0f;
    }
    if (speed > config_.max_speed_rad_s)
    {
      return config_.max_speed_rad_s;
    }
    return speed;
  }

  /**
   * @brief 生成丢线搜索输出。
   *
   * 如果上一次误差为正，说明黑线最后出现在右侧，搜索时左轮正转、右轮反转，使车头
   * 朝右侧找线；如果上一次误差为负，则反向搜索。没有历史误差时默认按正方向搜索。
   *
   * @return 丢线搜索模式下的四轮目标速度。
   */
  Output MakeSearchOutput() const
  {
    const float direction = (!has_last_error_ || last_error_ >= 0.0f) ? 1.0f : -1.0f;
    const float left_speed = config_.search_speed_rad_s * direction;
    const float right_speed = -config_.search_speed_rad_s * direction;
    return MakeOutput(0U, last_error_, true, left_speed, right_speed);
  }
};

}  // namespace Jie
