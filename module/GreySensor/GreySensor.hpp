#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: Runtime-configurable digital grey sensor module with weighted line position and line-loss memory
constructor_args:
  - channel_names:
      - "grey_sensor_gpio_0"
      - "grey_sensor_gpio_1"
      - "grey_sensor_gpio_2"
      - "grey_sensor_gpio_3"
      - "grey_sensor_gpio_4"
      - "grey_sensor_gpio_5"
      - "grey_sensor_gpio_6"
      - "grey_sensor_gpio_7"
  - active_low: false
  - topic_name: "grey_sensor"
  - publish_period_ms: 10
template_args: []
required_hardware:
  - grey_sensor_gpio_0
  - grey_sensor_gpio_1
  - grey_sensor_gpio_2
  - grey_sensor_gpio_3
  - grey_sensor_gpio_4
  - grey_sensor_gpio_5
  - grey_sensor_gpio_6
  - grey_sensor_gpio_7
depends: []
=== END MANIFEST === */
// clang-format on

#include "app_framework.hpp"
#include "gpio.hpp"
#include "message.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

class GreySensor : public LibXR::Application
{
 public:
  /**
   * @brief 模块最多支持的数字灰度传感器通道数量。
   *
   * 当前循迹车使用 8 路输入，掩码的 bit0 到 bit7 分别对应构造函数传入的
   * 第 0 到第 7 个 GPIO 别名。调用方必须保证传入顺序与硬件安装顺序一致。
   */
  static constexpr size_t MAX_CHANNEL_COUNT = 8;

  /**
   * @brief 位置输出的缩放系数。
   *
   * BuildPosition() 会把传感器横向位置转换为以 POSITION_SCALE 为单位的整数坐标，
   * 这样在没有浮点需求的场景下也能保存较细的位置分辨率。
   */
  static constexpr int16_t POSITION_SCALE = 1000;

  /**
   * @brief 丢线方向未知。
   *
   * 常见于上电后还没有任何有效检测，或者上一次有效位置正好在中心。
   */
  static constexpr uint8_t LOST_SIDE_UNKNOWN = 0;

  /**
   * @brief 上一次有效线位置在左侧。
   */
  static constexpr uint8_t LOST_SIDE_LEFT = 1;

  /**
   * @brief 上一次有效线位置在右侧。
   */
  static constexpr uint8_t LOST_SIDE_RIGHT = 2;

  /**
   * @brief 一次传感器采样的完整结果。
   *
   * raw_* 字段表示未经 active_low 转换的真实 GPIO 电平；active_* 字段表示经过
   * 有效电平转换后的逻辑检测结果。循迹控制一般使用 active_mask，因为它已经把
   * “低电平有效/高电平有效”的硬件差异统一成“1 表示检测到线”。
   */
  struct Sample
  {
    /**
     * @brief 原始 GPIO 电平掩码。
     *
     * 某一位为 1 表示对应通道读取到高电平，为 0 表示读取到低电平。
     */
    uint8_t raw_mask = 0;

    /**
     * @brief 逻辑有效通道掩码。
     *
     * 某一位为 1 表示对应通道处于有效状态。若构造时 active_low=true，则 GPIO
     * 低电平会被转换为 active=1。
     */
    uint8_t active_mask = 0;

    /**
     * @brief 与上一次发布样本相比发生变化的 active_mask 位。
     *
     * 该字段由 OnMonitor() 填充，用于订阅者判断哪些传感器通道刚刚变化。
     */
    uint8_t changed_mask = 0;

    /**
     * @brief 本实例实际绑定的通道数量。
     */
    uint8_t channel_count = 0;

    /**
     * @brief 本次采样中 active=1 的通道数量。
     */
    uint8_t active_count = 0;

    /**
     * @brief 是否检测到线。
     *
     * active_count 非 0 时为 1，否则为 0。
     */
    uint8_t line_detected = 0;

    /**
     * @brief 是否丢线。
     *
     * active_count 为 0 时为 1，表示当前所有通道均未检测到线。
     */
    uint8_t line_lost = 0;

    /**
     * @brief 丢线前记忆位置所在方向。
     *
     * 仅在 line_lost=1 时有实际参考意义，用于上层控制决定向哪边搜索。
     */
    uint8_t lost_side = LOST_SIDE_UNKNOWN;

    /**
     * @brief 当前有效通道的加权平均位置。
     *
     * 只有检测到线时更新；数值单位由 POSITION_SCALE 定义。
     */
    int16_t weighted_position = 0;

    /**
     * @brief 对外输出的位置。
     *
     * 检测到线时等于 weighted_position；丢线时保持最近一次有效位置，若从未检测到
     * 有效位置则为 0。
     */
    int16_t position = 0;

    /**
     * @brief 模块内部记忆的最近一次有效位置。
     */
    int16_t remembered_position = 0;

    /**
     * @brief 连续丢线采样次数。
     */
    uint32_t lost_count = 0;

    /**
     * @brief 发布序号。
     *
     * OnMonitor() 每次发布 topic 前递增一次，便于订阅者判断消息先后顺序。
     */
    uint32_t sequence = 0;

    /**
     * @brief 每个通道的原始电平数组。
     *
     * 数组下标与构造函数 channel_names 的顺序一致。
     */
    std::array<uint8_t, MAX_CHANNEL_COUNT> raw = {};

    /**
     * @brief 每个通道的逻辑有效状态数组。
     *
     * 数组下标与 active_mask 的位序一致。
     */
    std::array<uint8_t, MAX_CHANNEL_COUNT> active = {};
  };

  /**
   * @brief 构造灰度传感器应用模块。
   *
   * 构造函数会根据 channel_names 到 HardwareContainer 中查找 GPIO，保存通道指针，
   * 配置输入方向，创建 topic，并把自身注册到 ApplicationManager。channel_names
   * 的顺序同时决定 raw_mask/active_mask 的位序。
   *
   * @param hw 硬件容器，必须已注册所有通道别名。
   * @param app 应用管理器，用于周期性调用 OnMonitor()。
   * @param channel_names GPIO 别名列表，数量范围为 1 到 MAX_CHANNEL_COUNT。
   * @param active_low true 表示低电平为有效状态，false 表示高电平为有效状态。
   * @param topic_name 采样结果发布到的 topic 名称。
   * @param publish_period_ms OnMonitor() 的最小发布周期，0 表示每次调用都发布。
   */
  GreySensor(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
             std::initializer_list<const char*> channel_names,
             bool active_low = false, const char* topic_name = "grey_sensor",
             uint32_t publish_period_ms = 10);

  /**
   * @brief 读取一次完整样本并更新位置记忆状态。
   *
   * @return 包含电平、有效掩码、线位置和丢线状态的 Sample。
   */
  Sample Read();

  /**
   * @brief 读取原始 GPIO 电平掩码。
   *
   * @return bit 为 1 表示对应通道当前为高电平。
   */
  uint8_t ReadRawMask() const;

  /**
   * @brief 读取逻辑有效掩码。
   *
   * @return bit 为 1 表示对应通道当前处于有效状态。
   */
  uint8_t ReadActiveMask() const;

  /**
   * @brief 读取当前线位置。
   *
   * @return 当前加权位置；丢线时返回最近一次有效位置或 0。
   */
  int16_t ReadPosition();

  /**
   * @brief 获取实际绑定的通道数量。
   */
  size_t ChannelCount() const;

  /**
   * @brief 应用框架周期回调。
   *
   * 达到发布周期后读取一次样本，填充 changed_mask 和 sequence，然后发布到 topic。
   */
  void OnMonitor() override;

 private:
  /**
   * @brief 根据通道下标生成对应掩码位。
   */
  static uint8_t BuildBit(size_t channel);

  /**
   * @brief 根据通道下标生成横向位置坐标。
   */
  static int16_t BuildPosition(size_t channel, size_t channel_count);

  /**
   * @brief 根据记忆位置判断丢线前线在左侧还是右侧。
   */
  static uint8_t GetLostSide(int16_t position);

  /**
   * @brief 只读取数字量状态，不更新位置记忆。
   */
  Sample ReadDigital() const;

  /**
   * @brief 根据 active 通道计算位置并维护丢线记忆。
   */
  void UpdatePositionState(Sample& sample);

  /// 绑定到每一路传感器的 GPIO 指针，顺序决定掩码位序。
  std::array<LibXR::GPIO*, MAX_CHANNEL_COUNT> channels_ = {};
  /// 当前实例实际使用的通道数量。
  size_t channel_count_ = 0;
  /// 对外发布 Sample 的消息主题。
  LibXR::Topic topic_;
  /// 有效电平极性。true 表示低电平有效。
  bool active_low_ = false;
  /// 最小发布周期，单位 ms。
  uint32_t publish_period_ms_ = 10;
  /// 上一次发布时刻，单位 ms。
  uint32_t last_publish_ms_ = 0;
  /// 下一次发布使用的递增序号。
  uint32_t sequence_ = 0;
  /// 上一次发布的 active_mask，用于计算 changed_mask。
  uint8_t last_active_mask_ = 0;
  /// 最近一次检测到线时的位置。
  int16_t remembered_position_ = 0;
  /// 连续丢线计数。
  uint32_t lost_count_ = 0;
  /// 是否已经拥有一次有效位置记忆。
  bool has_position_memory_ = false;
  /// 是否已经发布过至少一次样本。
  bool has_published_ = false;
};
