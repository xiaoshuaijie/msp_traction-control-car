#include "Tracking.hpp"

#include "thread.hpp"

Tracking::Tracking(LibXR::ApplicationManager& app, const char* grey_topic_name,
                   const char* tracking_topic_name)
    : Tracking(app, grey_topic_name, tracking_topic_name, Config{})
{
}

Tracking::Tracking(LibXR::ApplicationManager& app, const char* grey_topic_name,
                   const char* tracking_topic_name, const Config& config)
    : config_(config),
      grey_subscriber_(grey_topic_name),
      tracking_topic_(LibXR::Topic::CreateTopic<Output>(tracking_topic_name))
{
  // ASyncSubscriber 只有在 WAITING 状态下才会捕获下一次发布。
  // 构造阶段先进入等待，之后 GreySensor::OnMonitor() 发布的第一帧 sample
  // 才能被 Tracking::OnMonitor() 消费。
  grey_subscriber_.StartWaiting();

  // 注册到 ApplicationManager 后，app_main.cpp 每轮调用 MonitorAll() 时，
  // Tracking::OnMonitor() 会被周期执行。GreySensor 也注册在同一个 manager 中，
  // 因此两者通过 topic 在同一个主循环里完成“采样 -> 计算”的数据流。
  app.Register(*this);
}

void Tracking::Reset()
{
  // 模式切换或重新开始循迹时清空历史误差，避免旧误差决定新的丢线搜索方向。
  last_error_ = 0.0f;
  has_last_error_ = false;
  latest_output_ = Output{};
  has_update_time_ = false;
}

void Tracking::SetConfig(const Config& config)
{
  config_ = config;
  Reset();
}

const Tracking::Config& Tracking::GetConfig() const { return config_; }

const Tracking::Output& Tracking::GetLatestOutput() const { return latest_output_; }

Tracking::Output Tracking::MakeOutput(uint8_t black_mask, float error, bool lost_line,
                                      float left_speed, float right_speed)
{
  // app_main.cpp 的速度环按四路电机下标取目标值。当前底盘按差速车处理：
  // 左前/左后同速，右前/右后同速。
  Output output;
  output.black_mask = black_mask;
  output.error = error;
  output.lost_line = lost_line;
  output.left_speed_rad_s = left_speed;
  output.right_speed_rad_s = right_speed;
  output.wheel_speed_rad_s = {left_speed, right_speed, left_speed, right_speed};
  return output;
}

float Tracking::CalculateError(uint8_t black_mask) const
{
  // 多路同时检测到黑线时使用加权平均，而不是只取最左/最右一路。
  // 这样经过宽线、线边缘或轻微偏摆时，误差变化更连续。
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

float Tracking::ClampNormalSpeed(float speed) const
{
  // 正常循迹只允许车轮前进或停止。需要反转找线时由 MakeSearchOutput()
  // 单独处理，避免大误差让某侧车轮突然反转。
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

Tracking::Output Tracking::MakeSearchOutput() const
{
  // 丢线时没有新的误差可算，只能根据最近一次有效误差判断黑线最后在哪边。
  // last_error_ >= 0 表示黑线最后偏右：左轮正转、右轮反转，车头向右找线。
  // last_error_ < 0 表示黑线最后偏左：反向搜索。
  const float direction = (!has_last_error_ || last_error_ >= 0.0f) ? 1.0f : -1.0f;
  const float left_speed = config_.search_speed_rad_s * direction;
  const float right_speed = -config_.search_speed_rad_s * direction;
  return MakeOutput(0U, last_error_, true, left_speed, right_speed);
}

Tracking::Output Tracking::Calculate(uint8_t black_mask, float dt_s)
{
  if (black_mask == 0U)
  {
    // active_mask 为 0 表示 GreySensor 8 路都没有检测到黑线，进入丢线搜索。
    return MakeSearchOutput();
  }

  const float error = CalculateError(black_mask);
  float derivative = 0.0f;
  if (has_last_error_ && dt_s > 1e-6f)
  {
    derivative = (error - last_error_) / dt_s;
  }

  const float correction = config_.turn_kp * error + config_.turn_kd * derivative;

  // 误差为正表示黑线偏右：左侧目标速度提高、右侧目标速度降低，
  // 让车头向右修正；误差为负时逻辑相反。
  const float left_speed = ClampNormalSpeed(config_.base_speed_rad_s + correction);
  const float right_speed = ClampNormalSpeed(config_.base_speed_rad_s - correction);

  // 只在检测到黑线时更新历史误差；丢线时保留它作为搜索方向依据。
  last_error_ = error;
  has_last_error_ = true;

  return MakeOutput(black_mask, error, false, left_speed, right_speed);
}

void Tracking::Publish(Output output, const GreySensor::Sample& sample)
{
  // source_sequence 把 Tracking 输出与 GreySensor 输入帧关联起来，调试时可用来
  // 确认 topic 链路是否连续、是否有漏帧。
  output.raw_mask = sample.raw_mask;
  output.active_count = sample.active_count;
  output.source_sequence = sample.sequence;
  output.sequence = sequence_++;

  // latest_output_ 是 app_main.cpp 的兜底数据源；同时发布到 "tracking" topic，
  // 让 app_main.cpp 或其它模块都能以 topic 方式消费循迹结果。
  latest_output_ = output;
  tracking_topic_.Publish(latest_output_);
}

void Tracking::OnMonitor()
{
  if (!grey_subscriber_.Available())
  {
    // 没有新 GreySensor sample 时不重复发布旧帧。app_main.cpp 会沿用
    // GetLatestOutput()，避免目标速度因一帧无消息而归零。
    return;
  }

  // GetData() 会把 ASyncSubscriber 从 DATA_READY 置回 IDLE，因此必须马上
  // StartWaiting()，让订阅者准备接收 GreySensor 下一次发布。
  const GreySensor::Sample sample = grey_subscriber_.GetData();
  grey_subscriber_.StartWaiting();

  // Tracking 的微分项使用两次 GreySensor topic 到达之间的时间间隔。
  // 首帧没有历史时间，dt_s 保持 0，微分项自然不生效。
  const uint32_t now_ms = LibXR::Thread::GetTime();
  float dt_s = 0.0f;
  if (has_update_time_)
  {
    dt_s = static_cast<float>(now_ms - last_update_ms_) * 0.001f;
  }
  last_update_ms_ = now_ms;
  has_update_time_ = true;

  Publish(Calculate(sample.active_mask, dt_s), sample);
}
