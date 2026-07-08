#include "GreySensor.hpp"

#include "libxr_def.hpp"
#include "thread.hpp"

uint8_t GreySensor::BuildBit(size_t channel)
{
  // 通道下标直接对应掩码位：第 0 路 -> bit0，第 7 路 -> bit7。
  return static_cast<uint8_t>(1U << channel);
}

int16_t GreySensor::BuildPosition(size_t channel, size_t channel_count)
{
  // 把离散通道映射到以 0 为中心的坐标轴。8 路时位置依次为：
  // -3500, -2500, -1500, -500, 500, 1500, 2500, 3500。
  const int32_t centered =
      (static_cast<int32_t>(channel) * 2) - static_cast<int32_t>(channel_count - 1);
  return static_cast<int16_t>((centered * POSITION_SCALE) / 2);
}

uint8_t GreySensor::GetLostSide(int16_t position)
{
  // 记忆位置小于 0 表示线最后一次出现在坐标左侧。
  if (position < 0)
  {
    return LOST_SIDE_LEFT;
  }

  // 记忆位置大于 0 表示线最后一次出现在坐标右侧。
  if (position > 0)
  {
    return LOST_SIDE_RIGHT;
  }

  // 正好在中心或没有有效记忆时，无法判断应该优先向哪一侧找线。
  return LOST_SIDE_UNKNOWN;
}

GreySensor::GreySensor(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
                       std::initializer_list<const char*> channel_names,
                       bool active_low, const char* topic_name,
                       uint32_t publish_period_ms)
    : channel_count_(channel_names.size()),
      topic_(LibXR::Topic::CreateTopic<Sample>(topic_name)),
      active_low_(active_low),
      publish_period_ms_(publish_period_ms)
{
  // 通道数量由配置列表决定，必须至少 1 路且不能超过 Sample 掩码容量。
  ASSERT(channel_count_ > 0);
  ASSERT(channel_count_ <= MAX_CHANNEL_COUNT);

  size_t i = 0;
  for (const char* name : channel_names)
  {
    // 每个别名都必须能在硬件容器中找到 GPIO，否则说明接入配置不完整。
    ASSERT(name != nullptr);
    channels_[i] = hw.FindOrExit<LibXR::GPIO>({name});

    // 模块默认只要求数字输入；若上层硬件需要上拉/下拉，可在构造后重新配置。
    const LibXR::ErrorCode err = channels_[i]->SetConfig(
        {LibXR::GPIO::Direction::INPUT, LibXR::GPIO::Pull::NONE});
    ASSERT(err == LibXR::ErrorCode::OK);
    i++;
  }

  // 记录初始有效掩码，第一次发布时 changed_mask 只反映构造后的真实变化。
  last_active_mask_ = ReadActiveMask();

  // 注册到应用管理器后，外部调用 ApplicationManager::MonitorAll() 即可驱动发布。
  app.Register(*this);
}

GreySensor::Sample GreySensor::ReadDigital() const
{
  // 该函数只采集电平和有效掩码，不修改 remembered_position_ 等历史状态。
  Sample sample;
  sample.channel_count = static_cast<uint8_t>(channel_count_);

  for (size_t i = 0; i < channel_count_; i++)
  {
    // raw_high 是真实 GPIO 电平；active 是按 active_low 极性转换后的逻辑有效状态。
    const bool raw_high = channels_[i]->Read();
    const bool active = active_low_ ? !raw_high : raw_high;

    // 数组形式便于上层逐通道查看，掩码形式便于控制算法快速使用。
    sample.raw[i] = raw_high ? 1U : 0U;
    sample.active[i] = active ? 1U : 0U;

    if (raw_high)
    {
      sample.raw_mask |= BuildBit(i);
    }

    if (active)
    {
      sample.active_mask |= BuildBit(i);
      sample.active_count++;
    }
  }

  return sample;
}

void GreySensor::UpdatePositionState(Sample& sample)
{
  // 多路同时有效时使用位置权重求平均，得到更平滑的线位置。
  int32_t weighted_sum = 0;

  for (size_t i = 0; i < channel_count_; i++)
  {
    if (sample.active[i] != 0U)
    {
      weighted_sum += BuildPosition(i, channel_count_);
    }
  }

  if (sample.active_count != 0U)
  {
    // 至少一路有效：认为检测到线，更新当前位置和丢线记忆。
    sample.line_detected = 1;
    sample.line_lost = 0;
    sample.weighted_position =
        static_cast<int16_t>(weighted_sum / sample.active_count);
    sample.position = sample.weighted_position;

    remembered_position_ = sample.position;
    has_position_memory_ = true;
    lost_count_ = 0;
  }
  else
  {
    // 全部通道无效：进入丢线状态，位置输出保持最近一次有效位置。
    sample.line_detected = 0;
    sample.line_lost = 1;
    lost_count_++;

    sample.position = has_position_memory_ ? remembered_position_ : 0;

    // 丢线方向只在有位置记忆时可靠；否则保持 UNKNOWN。
    sample.lost_side =
        has_position_memory_ ? GetLostSide(remembered_position_) : LOST_SIDE_UNKNOWN;
    sample.lost_count = lost_count_;
  }

  // 无论当前是否丢线，都把内部记忆位置同步到样本里，方便调试观察。
  sample.remembered_position = has_position_memory_ ? remembered_position_ : 0;
}

GreySensor::Sample GreySensor::Read()
{
  // 对外完整读取：先采集数字量，再更新位置/丢线状态机。
  Sample sample = ReadDigital();
  UpdatePositionState(sample);
  return sample;
}

uint8_t GreySensor::ReadRawMask() const { return ReadDigital().raw_mask; }

uint8_t GreySensor::ReadActiveMask() const { return ReadDigital().active_mask; }

int16_t GreySensor::ReadPosition() { return Read().position; }

size_t GreySensor::ChannelCount() const { return channel_count_; }

void GreySensor::OnMonitor()
{
  // OnMonitor 由应用管理器周期调用；这里按 publish_period_ms 做发布节流。
  const uint32_t now_ms = LibXR::Thread::GetTime();
  if (has_published_ && publish_period_ms_ != 0U &&
      static_cast<uint32_t>(now_ms - last_publish_ms_) < publish_period_ms_)
  {
    return;
  }

  Sample sample = Read();

  // changed_mask 使用本次 active_mask 与上一次发布值异或，标记变化通道。
  sample.changed_mask = static_cast<uint8_t>(sample.active_mask ^ last_active_mask_);
  sample.sequence = sequence_++;

  last_active_mask_ = sample.active_mask;
  last_publish_ms_ = now_ms;
  has_published_ = true;

  // 发布完整 Sample，订阅者可直接获得掩码、位置、丢线和序号信息。
  topic_.Publish(sample);
}
