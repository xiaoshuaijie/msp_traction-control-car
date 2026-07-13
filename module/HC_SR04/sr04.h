#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: HC-SR04 ultrasonic distance module with PWM trigger, pulse-width capture, and Topic publishing
constructor_args:
  - resources:
      trigger_pwm: "pwm_ultrasonic"
      capture_timer: "capture_ultrasonic"
      capture_channel: "capture_ultrasonic"
      capture_event_mask: "capture_ultrasonic"
      capture_clock_hz: 1000000
  - config:
      topic_name: "hc_sr04"
      trigger_frequency_hz: 16
      trigger_pulse_width_us: 10
      min_distance_mm: 20.0
      max_distance_mm: 4000.0
      filter_old_weight: 0.6
      no_echo_timeout_ms: 100
template_args: []
required_hardware:
  - pwm_ultrasonic
  - capture_ultrasonic
depends: []
=== END MANIFEST === */
// clang-format on

#include "app_framework.hpp"
#include "message.hpp"
#include "mspm0_pwm.hpp"
#include "sr04_math.hpp"

#include <cstdint>

/**
 * @brief 使用 PWM 周期性触发 HC-SR04，并将回波脉宽换算后的测距样本发布到 Topic。
 *
 * @details 构造阶段会完成以下生命周期初始化：配置并启动 TRIG PWM、清除 ECHO
 * 捕获通道的遗留事件、启动捕获计数器、初始化超时基准，最后向
 * ApplicationManager 注册本对象。注册后由 ApplicationManager 周期性调用
 * OnMonitor()；该回调以非阻塞方式消费捕获事件，或者在持续无回波时发布
 * NO_ECHO 样本。
 *
 * ApplicationManager 保存的是本对象的 Application 指针，因此 HC_SR04 实例必须
 * 比管理器对它的监控和回调存活得更久。Resources 中的底层外设也必须在模块整个
 * 生命周期内保持有效，尤其是 capture_timer 指向的寄存器实例。
 */
class HC_SR04 : public LibXR::Application
{
 public:
  /**
   * @brief TRIG 输出链路与 ECHO 输入捕获链路所需的硬件资源。
   *
   * @details 这些资源描述硬件连接关系，而不是运行时测量值。PWM 与捕获定时器应已
   * 由板级初始化代码完成引脚复用、时钟和捕获模式配置；本模块只负责设置 PWM 参数、
   * 启动计数器并读取指定捕获通道。
   */
  struct Resources
  {
    /**
     * @brief 连接 HC-SR04 TRIG 引脚的 MSPM0 PWM 资源。
     *
     * 构造时会根据 Config 设置周期频率和高电平占空比，用周期性脉冲启动测量。
     */
    LibXR::MSPM0PWM::Resources trigger_pwm;

    /**
     * @brief 连接 HC-SR04 ECHO 引脚的 GPTIMER 寄存器基址。
     *
     * 定时器必须预先配置为脉宽捕获模式；指针不得为空，且所指外设在模块存活期间
     * 必须始终有效。
     */
    GPTIMER_Regs* capture_timer;

    /**
     * @brief 用于读取 ECHO 高电平持续时间的捕获/比较通道索引。
     *
     * 该索引必须是 DriverLib 支持的 CC 通道，并与 capture_event_mask 指向同一个
     * 通道，否则构造时的资源一致性检查会失败。
     */
    DL_TIMER_CC_INDEX capture_channel;

    /**
     * @brief capture_channel 对应的脉宽上升计数完成事件位掩码，无物理单位。
     *
     * 应使用相应的 DL_TIMER_INTERRUPT_CCx_UP_EVENT。模块通过此掩码查询并清除
     * 原始中断状态；它必须与 capture_channel 精确匹配，不能用其他通道或组合事件。
     */
    std::uint32_t capture_event_mask;

    /**
     * @brief 捕获定时器计数时钟频率，单位 Hz。
     *
     * 该值必须大于 0，用于把捕获寄存器中的 tick 数四舍五入换算为微秒；它必须与
     * 定时器实际输入时钟一致，否则所有距离结果都会按相同比例产生系统误差。
     */
    std::uint32_t capture_clock_hz;
  };

  /**
   * @brief 触发、距离判定、滤波、无回波检测与发布使用的运行配置。
   *
   * @details 构造函数会检查各字段的基本约束。超出 [min_distance_mm,
   * max_distance_mm] 的捕获仍会发布并标记状态，但不会更新有效距离滤波器。
   */
  struct Config
  {
    /**
     * @brief 发布 Sample 的 Topic 名称，默认值为 "hc_sr04"。
     *
     * 必须指向非空字符串，且首字符不能为 '\0'；该名称在构造 Topic 时使用。
     */
    const char* topic_name = "hc_sr04";

    /**
     * @brief TRIG 周期脉冲频率，单位 Hz，默认值为 16。
     *
     * 必须大于 0。模块还用 ceil(1000 / frequency) 得到毫秒级触发周期，用于限制
     * NO_ECHO 样本的发布频率，避免在一次触发周期内重复报告超时。
     */
    std::uint32_t trigger_frequency_hz = 16;

    /**
     * @brief 每个 TRIG 脉冲的高电平宽度，单位 us，默认值为 10。
     *
     * 必须大于 0，并满足 trigger_frequency_hz * trigger_pulse_width_us <=
     * 1000000，保证换算得到的 PWM 占空比不超过 100%。
     */
    std::uint32_t trigger_pulse_width_us = 10;

    /**
     * @brief 可接受测距区间的下限，单位 mm，默认值为 20.0。
     *
     * 小于该值的回波标记为 BELOW_MIN，不进入滤波器；必须不大于
     * max_distance_mm。
     */
    float min_distance_mm = 20.0F;

    /**
     * @brief 可接受测距区间的上限，单位 mm，默认值为 4000.0。
     *
     * 大于该值的回波标记为 ABOVE_MAX，不进入滤波器；必须不小于
     * min_distance_mm。
     */
    float max_distance_mm = 4000.0F;

    /**
     * @brief 一阶低通滤波中旧滤波值的权重，默认值为 0.6，无物理单位。
     *
     * 取值范围为 [0, 1]。已有有效样本时使用
     * filtered = old * weight + raw * (1 - weight)；权重越大，输出越平滑但响应越慢。
     * 第一笔有效样本直接作为滤波初值，越界和无回波样本不改变该状态。
     */
    float filter_old_weight = 0.6F;

    /**
     * @brief 距离最近一次捕获事件的无回波超时时间，单位 ms，默认值为 100。
     *
     * 必须大于 0。只有同时超过此超时且距离上次发布至少一个触发周期时，模块才发布
     * NO_ECHO；该样本会保留此前的有效滤波距离，供订阅者结合状态字段判断。
     */
    std::uint32_t no_echo_timeout_ms = 100;
  };

  /** @brief 测距结果状态类型，包括无回波、有效、低于下限和高于上限。 */
  using Status = Module::HC_SR04Logic::Status;

  /** @brief 发布的数据类型，包含脉宽、原始/滤波距离、状态和按发布递增的序号。 */
  using Sample = Module::HC_SR04Logic::Sample;

  /**
   * @brief 使用 Config 的全部默认值构造、初始化并注册模块。
   * @param app 负责注册本对象并周期性驱动 OnMonitor() 的应用管理器。
   * @param resources 已完成板级配置且在模块生命周期内有效的 PWM 与捕获资源。
   *
   * @note app 会保留本对象的指针，因此调用方必须保证本对象的生命周期覆盖注册后的
   * 所有监控调用。
   */
  HC_SR04(LibXR::ApplicationManager& app, const Resources& resources);

  /**
   * @brief 使用指定配置构造模块，初始化硬件链路并注册到应用管理器。
   * @param app 负责注册本对象并周期性驱动 OnMonitor() 的应用管理器。
   * @param resources 已完成板级配置且在模块生命周期内有效的 PWM 与捕获资源。
   * @param config TRIG、距离范围、滤波、无回波超时及 Topic 配置；违反字段约束会触发
   * 构造期断言。
   *
   * @details 硬件启动顺序为：停用 PWM、写入频率和占空比、清除捕获事件、启动捕获
   * 计数器、重新启用 PWM，最后记录初始时间并注册应用。该顺序用于避免配置过程中
   * 产生的伪触发或陈旧捕获被当作有效测量。
   */
  HC_SR04(LibXR::ApplicationManager& app, const Resources& resources,
          const Config& config);

  /**
   * @brief 获取测距样本的发布 Topic。
   * @return 与本模块绑定、用于发布 Sample 值的 Topic 引用。
   * @note 返回引用的有效期不超过本 HC_SR04 实例的生命周期。
   */
  LibXR::Topic& SampleTopic();

  /**
   * @brief 获取模块最近一次实际发布的样本缓存。
   * @return 最近发布的 Sample；首次发布前返回 Sample 的默认初始值。
   *
   * 返回值可能表示 VALID、BELOW_MIN、ABOVE_MAX 或 NO_ECHO，调用方应检查 status，
   * 不能仅凭 filtered_distance_mm 判断当前是否收到有效回波。
   */
  [[nodiscard]] const Sample& GetLatestSample() const;

  /**
   * @brief 查询滤波器是否曾被至少一笔范围内的回波初始化。
   * @return 收到过至少一笔 VALID 样本时返回 true，否则返回 false。
   *
   * 该状态表示“已有可复用的有效滤波历史”，不表示最近一次发布必然有效；后续
   * NO_ECHO 或越界样本不会将它清零。
   */
  [[nodiscard]] bool HasValidSample() const;

  /**
   * @brief 轮询 ECHO 捕获状态，并在满足条件时发布捕获结果或无回波结果。
   *
   * @details 捕获事件优先：读取脉宽 tick、清除对应事件、换算距离并立即发布。没有
   * 捕获事件时，仅在 no_echo_timeout_ms 和发布节流周期都已满足后发布 NO_ECHO。
   * 此函数不阻塞，预期由 ApplicationManager 持续调用；调用频率会影响事件处理与
   * Topic 发布延迟。
   */
  void OnMonitor() override;

 private:
  /**
   * @brief 为样本补充分发元数据，更新模块缓存并发布到 Topic。
   * @param sample 尚未写入本模块序号的测距样本。
   * @param now_ms 当前无符号毫秒时间戳，用于更新发布节流基准。
   *
   * sequence 在此处统一递增，从而让有效、越界和无回波样本共享同一发布顺序；缓存
   * 在 Topic 发布前更新，因此 GetLatestSample() 与订阅者收到的数据保持一致。
   */
  void Publish(Sample sample, std::uint32_t now_ms);

  /** @brief 构造时复制的硬件资源描述；capture_timer 仍指向外部外设寄存器。 */
  Resources resources_;

  /** @brief 构造时复制的运行配置，供监控、换算、滤波与超时判定持续使用。 */
  Config config_;

  /** @brief TRIG PWM 的驱动封装，负责产生配置指定的周期性高电平触发脉冲。 */
  LibXR::MSPM0PWM trigger_pwm_;

  /** @brief 本模块创建并持有的 Sample 发布 Topic。 */
  LibXR::Topic sample_topic_;

  /** @brief 跨样本保留的滤波值及“是否已有有效样本”标志。 */
  Module::HC_SR04Logic::FilterState filter_state_;

  /** @brief 最近一次已发布的完整样本；发布前保持 Sample 的默认值。 */
  Sample latest_sample_;

  /** @brief 最近一次捕获到 ECHO 事件的时间戳，单位 ms，用作无回波超时起点。 */
  std::uint32_t last_capture_ms_ = 0;

  /** @brief 最近一次发布任意状态样本的时间戳，单位 ms，用于限制发布频率。 */
  std::uint32_t last_publish_ms_ = 0;

  /** @brief 由触发频率向上取整得到的周期，单位 ms，用作 NO_ECHO 发布节流间隔。 */
  std::uint32_t trigger_period_ms_ = 0;

  /** @brief 下一笔发布样本使用的序号；每次 Publish 后递增，溢出时按无符号数回绕。 */
  std::uint32_t sequence_ = 0;
};
