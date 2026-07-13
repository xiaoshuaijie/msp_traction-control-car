#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "app_framework.hpp"
#include "encoder_math.hpp"
#include "message.hpp"
#include "mspm0_gpio.hpp"
#include "timebase.hpp"
#include "ti_msp_dl_config.h"

namespace Module
{

/**
 * @brief 单路电机编码器的四倍频正交解码器。
 *
 * A、B 两相均使用双边沿 GPIO 中断。任意一相发生跳变时，解码器都会读取当前
 * 两相电平，并依据“上一状态 -> 当前状态”的转换查表得到 +1、-1 或 0。
 * 合法的单步格雷码跳变计数一次；两相同时变化等非法跳变视为抖动或丢边，不计数。
 *
 * `count_` 在 GPIO 中断中更新，在主循环的临界区内读取。计数采用无符号模
 * 2^32 累加，对外再按相同位模式解释为 int32_t，从而避免有符号溢出的未定义行为。
 */
class QuadratureDecoder
{
 public:
  /**
   * @brief 绑定一路正交编码器的 A、B 两相 GPIO。
   *
   * 构造函数只保存两相的硬件描述，不配置 GPIO，也不使能中断；调用方必须在对象
   * 完整构造后调用 Init()，避免中断回调访问尚未初始化完成的对象。
   *
   * @param a_port A 相所在的 GPIO 端口寄存器。
   * @param a_pin A 相引脚掩码。
   * @param a_iomux A 相对应的 IOMUX PINCM 索引。
   * @param b_port B 相所在的 GPIO 端口寄存器。
   * @param b_pin B 相引脚掩码。
   * @param b_iomux B 相对应的 IOMUX PINCM 索引。
   */
  QuadratureDecoder(GPIO_Regs* a_port, uint32_t a_pin, uint32_t a_iomux,
                    GPIO_Regs* b_port, uint32_t b_pin, uint32_t b_iomux);

  // GPIO 回调保存了对象地址，复制或移动会使已注册的回调指向错误实例。
  QuadratureDecoder(const QuadratureDecoder&) = delete;
  QuadratureDecoder(QuadratureDecoder&&) = delete;
  QuadratureDecoder& operator=(const QuadratureDecoder&) = delete;
  QuadratureDecoder& operator=(QuadratureDecoder&&) = delete;

  /**
   * @brief 将两相配置为上拉、双边沿中断输入并开始解码。
   *
   * 先配置 GPIO 和回调，再读取一次当前相位作为状态机起点，最后使能两相中断。
   * 这个顺序可避免把初始化期间的电平误判为一次有效跳变。每个实例只应初始化一次。
   */
  void Init();

  /**
   * @brief 读取带方向的四倍频累计计数。
   *
   * 返回值保留内部 uint32_t 计数器的位模式，因此跨过 INT32_MAX/INT32_MIN 时仍与
   * 模 2^32 计数一致。调用方如需与中断写入保持一致，应在禁用相关中断的临界区读取。
   *
   * @return 四倍频累计计数；正负方向由 A、B 相接线顺序决定。
   */
  [[nodiscard]] int32_t GetCount() const;

  /**
   * @brief 将累计计数清零。
   *
   * 本方法本身不屏蔽中断；聚合层通过 InterruptGuard 保证四路计数同步复位。
   */
  void ResetCount();

  /**
   * @brief 关闭 A、B 两相 GPIO 中断，使该解码器停止更新计数。
   *
   * 用于 Encoder 析构阶段，防止对象销毁后硬件回调继续访问其地址。
   */
  void Shutdown();

 private:
  /**
   * @brief 读取 A、B 两相并编码为两位状态。
   *
   * bit1 保存 A 相，bit0 保存 B 相，返回值范围为 0b00..0b11。
   */
  uint8_t ReadPhase();

  /**
   * @brief GPIO 边沿回调：推进正交状态机并更新累计计数。
   *
   * @param in_isr LibXR 传入的中断上下文标志；解码逻辑不依赖该值。
   * @param self 注册回调时绑定的解码器实例，生命周期必须覆盖中断使能期。
   */
  static void OnEdge(bool in_isr, QuadratureDecoder* self);

  /**
   * @brief 四倍频正交解码状态转换表。
   *
   * 索引由 `(上一状态 << 2) | 当前状态` 组成。每一行固定上一状态，四列依次
   * 对应新状态 00、01、10、11；+1/-1 表示两个旋转方向，0 表示未变化或非法跳变。
   */
  static constexpr int8_t kTransitionTable[16] = {
      0, +1, -1, 0,   // 上一状态 00 -> 当前状态 00/01/10/11。
      -1, 0, 0, +1,   // 上一状态 01 -> 当前状态 00/01/10/11。
      +1, 0, 0, -1,   // 上一状态 10 -> 当前状态 00/01/10/11。
      0, -1, +1, 0};  // 上一状态 11 -> 当前状态 00/01/10/11。

  LibXR::MSPM0GPIO a_;             ///< A 相 GPIO 驱动封装。
  LibXR::MSPM0GPIO b_;             ///< B 相 GPIO 驱动封装。
  volatile uint32_t count_ = 0;    ///< 中断更新的模 2^32 累计计数。
  uint8_t last_state_ = 0;         ///< 上一次边沿处理后的两相状态。
};

/**
 * @brief 聚合四路编码器并周期发布一致的四轮运动学采样。
 *
 * 四路 QuadratureDecoder 按前左、前右、后左、后右排列。OnMonitor() 在同一个
 * 中断临界区读取四路计数，再使用同一个采样时间计算角度与角速度，确保一帧中的
 * 四个车轮具有一致的时间基准。结果通过配置的 LibXR Topic 发布。
 *
 * Encoder 对象必须覆盖所有 ApplicationManager::MonitorAll() 调用。LibXR 的应用与
 * 硬件注册表会保留对象指针，因此销毁对象前必须停止监控；析构函数只负责关闭编码器
 * GPIO 中断，并不会从这些注册表中自动注销对象。
 */
class Encoder : public LibXR::Application
{
 public:
  /** @brief 电机/车轮在所有定长数组中的统一下标。 */
  enum MotorId : size_t
  {
    kFrontLeft = 0,  ///< 前左轮。
    kFrontRight,     ///< 前右轮。
    kBackLeft,       ///< 后左轮。
    kBackRight,      ///< 后右轮。
    kMotorCount      ///< 车轮数量，同时作为数组长度。
  };

  /** @brief 默认每圈计数；应按编码器线数、四倍频和减速比校准。 */
  static constexpr float kDefaultCountsPerRev = 1024.0F;

  /** @brief Encoder 的发布与运动学换算配置。 */
  struct Config
  {
    const char* topic_name = "encoder";       ///< 采样发布使用的 Topic 名称。
    uint32_t publish_period_ms = 5U;           ///< 最小发布间隔，单位 ms；0 表示每次监控均发布。
    float counts_per_rev = 1024.0F;            ///< 输出轴旋转一圈对应的四倍频计数。
  };

  /** @brief 一次同时采集的四轮编码器与运动学快照。 */
  struct Sample
  {
    std::array<int32_t, kMotorCount> count{};       ///< 四倍频累计计数，顺序由 MotorId 定义。
    std::array<double, kMotorCount> angle_rad{};    ///< 相对复位点的累计角度，单位 rad。
    std::array<float, kMotorCount> speed_rad_s{};   ///< 相邻有效采样间的平均角速度，单位 rad/s。
    uint32_t sample_time_ms = 0;                    ///< 采样时刻，单位 ms，允许 uint32_t 自然回绕。
    uint32_t sequence = 0;                          ///< 发布序号，每生成一帧后递增。
  };

  /**
   * @brief 使用默认配置构造并注册 Encoder 应用。
   * @param hw 保存模块入口的 LibXR 硬件容器。
   * @param app 周期调用 OnMonitor() 的应用管理器。
   */
  Encoder(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app);

  /**
   * @brief 使用显式配置构造并注册 Encoder 应用。
   *
   * 构造过程中会创建 Topic、初始化四路 GPIO 中断，并把当前对象注册到 hw 与 app。
   * `config.topic_name` 指向的字符串必须在 Topic 创建期间保持有效。
   *
   * @param hw 保存模块入口的 LibXR 硬件容器。
   * @param app 周期调用 OnMonitor() 的应用管理器。
   * @param config 发布周期、Topic 名称和每圈计数配置。
   */
  Encoder(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
          const Config& config);

  // LibXR 注册表和 GPIO 回调均持有本对象地址，因此实例地址在生命周期内必须稳定。
  Encoder(const Encoder&) = delete;
  Encoder(Encoder&&) = delete;
  Encoder& operator=(const Encoder&) = delete;
  Encoder& operator=(Encoder&&) = delete;

  /**
   * @brief 在对象销毁前关闭全部编码器 GPIO 中断。
   *
   * 调用方仍需保证 ApplicationManager 已停止访问该对象。
   */
  ~Encoder() override;

  /**
   * @brief 获取发布 Encoder::Sample 的 Topic。
   * @return 由本对象持有、生命周期与本对象相同的 Topic 引用。
   */
  LibXR::Topic& SampleTopic();

  /**
   * @brief 获取最近一次发布或复位后的缓存采样。
   * @return 内部缓存的只读引用；下一次发布或 ResetAll() 后内容会改变。
   */
  [[nodiscard]] const Sample& LatestSample() const;

  /**
   * @brief 原子清零四路计数、速度历史和缓存运动学数据。
   *
   * 发布序号保留当前值，避免复位后序号倒退；下一次 OnMonitor() 被视为首帧，
   * 因为没有前一帧计数和有效时间差，所以速度输出为 0。
   */
  void ResetAll();

  /**
   * @brief 在配置的最小发布周期到期后生成并发布一帧采样。
   *
   * `publish_period_ms == 0` 时不节流，每次监控调用均发布。角度由当前累计计数
   * 直接换算，速度由当前计数、上一帧计数和统一时间差计算。
   */
  void OnMonitor() override;

 private:
  /**
   * @brief 在同一中断临界区内读取四路计数，形成一致快照。
   * @return 顺序与 MotorId 一致的四轮累计计数。
   */
  [[nodiscard]] EncoderMath::WheelCounts ReadCounts() const;

  std::array<QuadratureDecoder, kMotorCount> decoders_;       ///< 按 MotorId 排列的四路硬件解码器。
  Config config_;                                             ///< 构造时复制并在生命周期内保持不变的配置。
  LibXR::Topic sample_topic_;                                 ///< Encoder::Sample 的发布通道。
  std::optional<EncoderMath::WheelCounts> previous_counts_;   ///< 上一有效帧计数；无值时不计算速度。
  LibXR::MicrosecondTimestamp last_sample_time_{};             ///< 上一帧发布时间，微秒时间基准。
  Sample latest_sample_{};                                    ///< 最近一次对外发布或复位后的快照。
  uint32_t sequence_ = 0;                                     ///< 下一帧将使用的发布序号。
  bool has_published_ = false;                                ///< 是否已有可用于周期/速度计算的上一帧。
};

}  // namespace Module（编码器业务模块）
