#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Module::EncoderMath
{

/** @brief 四个车轮的四倍频累计计数，顺序与 Encoder::MotorId 保持一致。 */
using WheelCounts = std::array<std::int32_t, 4>;

/** @brief 由一帧编码器计数换算得到的四轮运动学数据。 */
struct Sample
{
  std::array<double, 4> angle_rad{};    ///< 相对复位点的累计角度，单位 rad。
  std::array<float, 4> speed_rad_s{};   ///< 相邻采样间的平均角速度，单位 rad/s。
};

/**
 * @brief 根据当前/上一帧计数构造四轮角度与角速度采样。
 *
 * 角度始终由当前累计计数直接换算。仅当存在上一帧且 `dt_seconds > 0` 时计算速度；
 * 首帧和 ResetAll() 后首帧保持零速度。若时间差或每圈计数不是有限数，或者
 * `counts_per_rev <= 0`，函数返回全零采样，避免无效配置传播 NaN/Inf。
 *
 * @param current_counts 当前四轮累计计数。
 * @param previous_counts 上一帧四轮计数；std::nullopt 表示没有速度基准。
 * @param dt_seconds 当前帧与上一帧的时间差，单位 s。
 * @param counts_per_rev 输出轴旋转一圈对应的四倍频计数。
 * @return 当前累计角度，以及条件允许时计算出的平均角速度。
 */
inline Sample BuildSample(const WheelCounts& current_counts,
                          const std::optional<WheelCounts>& previous_counts,
                          float dt_seconds, float counts_per_rev)
{
  Sample sample{};
  // 无效时间或分辨率会同时破坏角度/速度换算，统一返回安全的全零结果。
  if (!std::isfinite(dt_seconds) || !std::isfinite(counts_per_rev) ||
      counts_per_rev <= 0.0f)
  {
    return sample;
  }

  constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
  // 每个四倍频计数对应的机械轴转角，后续四个车轮复用同一比例因子。
  const double radians_per_count = kTwoPi / counts_per_rev;
  // dt == 0 仍可计算累计角度，但不能作为速度分母。
  const bool can_calculate_speed = previous_counts.has_value() && dt_seconds > 0.0f;

  for (std::size_t wheel = 0; wheel < current_counts.size(); ++wheel)
  {
    // 使用 double 保存长期累计角度，降低大计数值换算时的精度损失。
    sample.angle_rad[wheel] =
        static_cast<double>(current_counts[wheel]) * radians_per_count;

    if (can_calculate_speed)
    {
      // 先按 uint32_t 做模 2^32 差分，可在计数跨越 INT32_MAX/INT32_MIN 时保持连续。
      const std::uint32_t modular_delta =
          static_cast<std::uint32_t>(current_counts[wheel]) -
          static_cast<std::uint32_t>((*previous_counts)[wheel]);
      // 将环形差值映射到 [-2^31, 2^31-1]，得到一次采样周期内的最短有符号位移。
      // 该规则假定相邻两帧的真实计数变化绝对值不会达到或超过 2^31。
      const std::int64_t count_delta =
          modular_delta <= static_cast<std::uint32_t>(INT32_MAX)
              ? static_cast<std::int64_t>(modular_delta)
              : static_cast<std::int64_t>(modular_delta) - (INT64_C(1) << 32);
      // 平均角速度 = 计数增量 * 每计数弧度 / 采样时间。
      sample.speed_rad_s[wheel] =
          static_cast<float>(static_cast<double>(count_delta) *
                             radians_per_count / dt_seconds);
    }
  }

  return sample;
}

}  // namespace Module::EncoderMath（硬件无关运动学换算）
