#pragma once

#include <cstdint>

/**
 * @brief HC-SR04 采样数据的纯计算逻辑。
 *
 * 本命名空间不直接操作 GPIO、定时器或消息发布接口，而是集中处理捕获计数换算、
 * 距离有效性判定和滤波状态更新，使这些与硬件访问无关的规则能够独立复用和测试。
 */
namespace Module::HC_SR04Logic
{

/** @brief 单次测距结果的有效性状态。 */
enum class Status : std::uint8_t
{
  /// 未捕获到回波脉冲；此时脉宽和原始距离保持 Sample 的默认值，不代表实际测得 0 mm。
  NO_ECHO = 0,
  /// 距离处于配置的闭区间 [min_distance_mm, max_distance_mm] 内，可用于更新滤波器。
  VALID,
  /// 距离严格小于最小量程；该帧有回波，但不会用于更新滤波器。
  BELOW_MIN,
  /// 距离严格大于最大量程；该帧有回波，但不会用于更新滤波器。
  ABOVE_MAX
};

/** @brief 一次对外发布的测距样本及其质量信息。 */
struct Sample
{
  /// 回波高电平持续时间，单位为微秒（us）；NO_ECHO 时默认置 0。
  std::uint32_t pulse_width_us = 0;
  /// 根据回波脉宽换算的未滤波距离，单位为毫米（mm）；NO_ECHO 时默认置 0。
  float raw_distance_mm = 0.0F;
  /// 最近一次有效 EMA 结果，单位为毫米（mm）；无效帧仅携带旧值，不会更新它。
  float filtered_distance_mm = 0.0F;
  /// 当前帧状态，用于区分有效距离、量程外回波和完全无回波。
  Status status = Status::NO_ECHO;
  /// 样本发布序号；本文件中的构造函数不赋号，由上层发布流程统一填写。
  std::uint32_t sequence = 0;
};

/** @brief 跨采样周期保存的指数移动平均（EMA）状态。 */
struct FilterState
{
  /// 最近一次由有效样本更新得到的滤波距离，单位为毫米（mm）。
  float filtered_distance_mm = 0.0F;
  /// 是否已接收过有效样本；用于避免首帧与默认 0 值混合而产生启动偏差。
  bool has_valid_sample = false;
};

/**
 * @brief 将捕获定时器计数换算为微秒，并按最接近的整数微秒取整。
 *
 * 先将 ticks 提升为 64 位再乘以 1,000,000，避免 32 位中间乘积溢出。
 * 除法前加上 capture_clock_hz / 2，相当于对正整数商执行四舍五入，而不是简单截断。
 * 调用方必须保证 capture_clock_hz 非 0，并保证换算结果可由 std::uint32_t 表示。
 */
inline std::uint32_t TicksToMicroseconds(std::uint32_t ticks,
                                         std::uint32_t capture_clock_hz)
{
  const std::uint64_t scaled =
      static_cast<std::uint64_t>(ticks) * 1000000ULL;
  return static_cast<std::uint32_t>(
      (scaled + capture_clock_hz / 2U) / capture_clock_hz);
}

/**
 * @brief 按传感器配置量程对距离分类。
 *
 * 最小值和最大值均属于有效范围：只有严格越过边界才分别判为 BELOW_MIN 或
 * ABOVE_MAX，因此 distance_mm == min_distance_mm 和 distance_mm == max_distance_mm
 * 均返回 VALID。
 */
inline Status ClassifyDistance(float distance_mm, float min_distance_mm,
                               float max_distance_mm)
{
  if (distance_mm < min_distance_mm)
  {
    return Status::BELOW_MIN;
  }
  if (distance_mm > max_distance_mm)
  {
    return Status::ABOVE_MAX;
  }
  return Status::VALID;
}

/**
 * @brief 将一次有效捕获转换为距离样本，并按状态决定是否更新 EMA。
 *
 * 常温下声速近似为 0.34 mm/us。超声波需要从探头传播到目标后再返回，因此脉宽
 * 对应的是往返路程，单程距离使用 0.34 / 2 = 0.17 mm/us 进行换算。
 *
 * 仅状态为 VALID 的样本参与滤波。首个有效样本直接作为滤波初值，避免默认 0 值
 * 拉低结果；后续有效样本按
 * `旧滤波值 * filter_old_weight + 新距离 * (1 - filter_old_weight)` 更新。
 * filter_old_weight 越大，结果越平滑但响应越慢，调用方应将其配置在 [0, 1] 内。
 * BELOW_MIN 和 ABOVE_MAX 帧仍保留本次原始测距及状态，但不会污染滤波状态，返回的
 * filtered_distance_mm 因而是最近一次有效结果。
 */
inline Sample ProcessCapture(std::uint32_t ticks,
                             std::uint32_t capture_clock_hz,
                             float min_distance_mm, float max_distance_mm,
                             float filter_old_weight, FilterState& state)
{
  Sample sample;
  sample.pulse_width_us = TicksToMicroseconds(ticks, capture_clock_hz);
  sample.raw_distance_mm =
      static_cast<float>(sample.pulse_width_us) * 0.17F;
  sample.status = ClassifyDistance(sample.raw_distance_mm, min_distance_mm,
                                   max_distance_mm);

  if (sample.status == Status::VALID)
  {
    state.filtered_distance_mm =
        state.has_valid_sample
            ? state.filtered_distance_mm * filter_old_weight +
                  sample.raw_distance_mm * (1.0F - filter_old_weight)
            : sample.raw_distance_mm;
    state.has_valid_sample = true;
  }

  sample.filtered_distance_mm = state.filtered_distance_mm;
  return sample;
}

/**
 * @brief 构造无回波样本，并携带最近一次有效滤波结果。
 *
 * 无回波只说明当前测量失败，不等价于目标距离为 0，也不应让瞬时丢波把平滑结果
 * 清零。因此该样本保持默认的 0 脉宽和 0 原始距离，通过 NO_ECHO 明确无效语义，
 * 同时保留 state 中最近的有效滤波值，供上层在结合 status 后决定如何展示或控制。
 */
inline Sample BuildNoEcho(const FilterState& state)
{
  Sample sample;
  sample.status = Status::NO_ECHO;
  sample.filtered_distance_mm = state.filtered_distance_mm;
  return sample;
}

}  // namespace Module::HC_SR04Logic
