/**
 * @file NRF24L01.hpp
 * @brief 基于 LibXR Topic 和轮询状态机的单个 NRF24L01 应用接口。
 */
#pragma once

#include <array>
#include <cstdint>

#include "app_framework.hpp"
#include "message.hpp"
#include "nrf24l01_bus.hpp"
#include "nrf24l01_state.hpp"

/**
 * @brief 非阻塞驱动单个 NRF24L01 收发器的 Topic 应用。
 *
 * 应用通过 `OnMonitor()` 逐步推进初始化、接收、发送和恢复状态，不在监控回调中
 * 主动休眠。发送请求、接收数据和状态分别使用独立 Topic；发送通道采用单在途
 * 约束，不提供队列语义。
 *
 * 构造函数会把本对象地址注册到 `ApplicationManager` 和硬件容器。两者均没有在
 * 本类析构时自动注销该地址，因此对象地址和生命周期必须覆盖所有
 * `ApplicationManager::MonitorAll()` 调用，且应在停止监控后再销毁对象。
 */
class NRF24L01 : public LibXR::Application
{
 public:
  /** NRF24L01 固定地址宽度，单位字节。 */
  static constexpr size_t ADDRESS_WIDTH = Module::Nrf24l01Bus::kAddressWidth;
  /** 本应用使用的固定收发载荷宽度，单位字节。 */
  static constexpr size_t PAYLOAD_WIDTH = Module::Nrf24l01Bus::kPayloadWidth;

  /** 射频空中速率类型，定义由总线层统一提供。 */
  using DataRate = Module::Nrf24l01Bus::DataRate;
  /** 射频输出功率类型，定义由总线层统一提供。 */
  using OutputPower = Module::Nrf24l01Bus::OutputPower;

  /** @brief 应用层轮询状态机的当前状态。 */
  enum class State : uint8_t
  {
    INITIALIZING,  ///< 等待首次初始化；下一次监控会尝试配置设备。
    RECEIVE,       ///< 接收轮询状态，也是在途发送请求可以启动的状态。
    TRANSMITTING,  ///< 已启动一条发送请求，等待终态标志或超时。
    RECOVERING,    ///< 设备已进入恢复等待，周期到达后重新尝试初始化。
  };

  /** @brief 最近一次发送或设备故障在状态消息中的结果分类。 */
  enum class TxResult : uint8_t
  {
    NONE,            ///< 当前没有需要报告的发送终态结果。
    SUCCESS,         ///< 发送完成且 STATUS 报告 TX_DS。
    MAX_RETRIES,     ///< 自动重传达到配置上限。
    INVALID_STATUS,  ///< STATUS 标志组合或接收管道编号无效。
    TIMEOUT,         ///< 在配置的发送超时边界内未观察到终态标志。
    DEVICE_ERROR,    ///< 初始化失败或运行时检测到设备未上电。
  };

  /** @brief 无线参数、Topic 名称、轮询周期和硬件注册名称。 */
  struct Config
  {
    /** 本机接收地址；固定 5 字节，并在每次发送结束后继续作为接收地址使用。 */
    std::array<uint8_t, ADDRESS_WIDTH> address = {0x11, 0x22, 0x33, 0x44,
                                                  0x55};
    /** NRF24L01 射频频道号，有效范围为 0..125；越界会导致初始化失败。 */
    uint8_t channel = 2U;
    /** 空中数据速率，由总线层编码到 RF_SETUP。 */
    DataRate data_rate = DataRate::MBPS_2;
    /** 射频输出功率，由总线层编码到 RF_SETUP。 */
    OutputPower output_power = OutputPower::DBM_0;
    /** 自动重传间隔，单位 us；须为 250..4000 范围内的 250 倍数。 */
    uint16_t retry_delay_us = 250U;
    /** 自动重传次数，有效范围为 0..15。 */
    uint8_t retry_count = 3U;
    /** 固定载荷宽度；本应用要求必须等于 `PAYLOAD_WIDTH`。 */
    uint8_t payload_width = 32U;
    /** 接收 `TxRequest` 的 Topic 名称；构造时必须指向有效 C 字符串。 */
    const char* tx_topic_name = "nrf24l01_tx";
    /** 发布 `RxPacket` 的 Topic 名称；构造时必须指向有效 C 字符串。 */
    const char* rx_topic_name = "nrf24l01_rx";
    /** 发布 `Status` 的 Topic 名称；构造时必须指向有效 C 字符串。 */
    const char* status_topic_name = "nrf24l01_status";
    /** 接收状态最小轮询间隔，单位 ms；0 表示每次接收态监控都轮询。 */
    uint32_t rx_poll_period_ms = 10U;
    /** 空闲状态心跳最小发布间隔，单位 ms；0 表示每次空闲监控都发布。 */
    uint32_t status_period_ms = 100U;
    /** 进入恢复态后再次尝试初始化前的等待周期，单位 ms；0 表示无需等待。 */
    uint32_t recovery_period_ms = 100U;
    /** 单次发送超时阈值，单位 ms；必须大于 0，达到边界即判超时。 */
    uint32_t tx_timeout_ms = 100U;
    /** CE GPIO 在硬件容器中的名称；构造时用于查找推挽输出引脚。 */
    const char* ce_gpio_name = "nrf24l01_ce";
    /** CSN GPIO 在硬件容器中的名称；构造时用于查找推挽输出引脚。 */
    const char* csn_gpio_name = "nrf24l01_csn";
    /** 软件 SPI SCK GPIO 在硬件容器中的名称。 */
    const char* sck_gpio_name = "nrf24l01_sck";
    /** 软件 SPI MOSI GPIO 在硬件容器中的名称。 */
    const char* mosi_gpio_name = "nrf24l01_mosi";
    /** 软件 SPI MISO GPIO 在硬件容器中的名称；总线层将其配置为上拉输入。 */
    const char* miso_gpio_name = "nrf24l01_miso";
    /** 当前应用实例注册到硬件容器时使用的名称。 */
    const char* hardware_name = "nrf24l01";
  };

  /** @brief 发布到发送 Topic 的一条固定宽度发送请求。 */
  struct TxRequest
  {
    /** 调用方生成的关联标识；模块原样带入后续 `Status::request_id`。 */
    uint32_t request_id = 0U;
    /** 本次发送的 5 字节目标地址；不改变配置中的本机接收地址。 */
    std::array<uint8_t, 5> target_address{};
    /** 本次发送的固定 32 字节载荷；本接口不携带可变长度字段。 */
    std::array<uint8_t, 32> payload{};
  };

  /** @brief 从接收 FIFO 读取并发布的一条固定宽度数据包。 */
  struct RxPacket
  {
    /** 从无线设备读取的固定 32 字节载荷。 */
    std::array<uint8_t, 32> payload{};
    /** STATUS 中报告的接收管道编号；应用只接受 0..5。 */
    uint8_t pipe = 0U;
    /** 调用方时间基准下读取该包的毫秒时刻，允许 `uint32_t` 自然回绕。 */
    uint32_t received_at_ms = 0U;
    /** 接收包发布序号；从 0 开始，每发布一包后按 `uint32_t` 自然递增。 */
    uint32_t sequence = 0U;
  };

  /** @brief 状态 Topic 发布的应用状态、最近结果与累计计数快照。 */
  struct Status
  {
    /** 发布时应用所处的状态机状态。 */
    State state = State::INITIALIZING;
    /** 最近设置的发送/设备结果；开始发送或初始化成功时重置为 `NONE`。 */
    TxResult tx_result = TxResult::NONE;
    /** 最近一次保存的 STATUS 寄存器快照；部分状态切换会将其清零。 */
    uint8_t raw_status = 0U;
    /** 当前或最近一次已启动发送请求的关联标识；无活动请求时不保证清零。 */
    uint32_t request_id = 0U;
    /** 成功发送累计数；达到 `UINT32_MAX` 后保持饱和。 */
    uint32_t tx_success_count = 0U;
    /**
     * 发送终态失败累计数；自动重传耗尽、发送终态无效 STATUS 和发送超时计入，
     * 接收态无效 STATUS、非法 Pipe 或设备错误触发恢复时不递增；达到上限后饱和。
     */
    uint32_t tx_failure_count = 0U;
    /** 已发布接收包累计数；达到 `UINT32_MAX` 后保持饱和。 */
    uint32_t rx_count = 0U;
    /** 状态发布序号；从 0 开始，每次发布后按 `uint32_t` 自然递增。 */
    uint32_t sequence = 0U;
  };

  /**
   * @brief 使用 `Config{}` 默认值构造、配置引脚并注册应用。
   * @param hw 提供五个软件 SPI GPIO，并保存本应用硬件入口的容器。
   * @param app 后续通过 `MonitorAll()` 周期调用本对象的应用管理器。
   */
  NRF24L01(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app);

  /**
   * @brief 使用显式配置构造、配置引脚并注册应用。
   *
   * 构造时复制 `config`，解析 GPIO，创建三个 Topic，使发送异步订阅者等待第一条
   * 请求，并把对象注册到 `hw` 与 `app`。载荷宽度、发送超时和 GPIO 配置失败由
   * 断言约束；无线寄存器初始化延后到 `OnMonitor()`。
   *
   * @param hw 提供配置中五个 GPIO 名称对应硬件，并保存应用注册入口的容器。
   * @param app 后续通过 `MonitorAll()` 驱动状态机的应用管理器。
   * @param config 在对象生命周期内按值保存的无线、Topic、周期和硬件名称配置。
   */
  NRF24L01(LibXR::HardwareContainer& hw, LibXR::ApplicationManager& app,
           const Config& config);

  /**
   * @brief 关闭无线总线并销毁对象。
   *
   * 析构函数不会从 `ApplicationManager` 或硬件容器注销本对象；调用方必须先停止
   * 所有 `MonitorAll()` 调用，并保证之后不再通过已注册入口访问该对象。
   */
  ~NRF24L01() override;

  // 应用管理器和硬件容器保存本对象地址，实例在已注册期间不得复制或移动。
  NRF24L01(const NRF24L01&) = delete;
  NRF24L01(NRF24L01&&) = delete;
  NRF24L01& operator=(const NRF24L01&) = delete;
  NRF24L01& operator=(NRF24L01&&) = delete;

  /**
   * @brief 获取采用单在途语义的发送请求 Topic。
   *
   * 调用方应仅在观察到 `RECEIVE` 状态且上一条请求的终态 `Status` 已到达后发布。
   * 模块取走请求进入 `TRANSMITTING` 后，内部 `ASyncSubscriber` 为 `IDLE`；此时
   * 新发布会被静默丢弃，模块既无法接收也无法对这些丢弃计数。发送终态处理会重新
   * 装载订阅者，因此在 `RECOVERING` 期间最多捕获第一条新请求；后续发布仍会被
   * 忽略。即使如此，调用方仍应等待随后发布的 `RECEIVE` 状态再提交下一条请求。
   *
   * @return 本对象持有的发送 Topic 引用；不得在本对象销毁后继续使用。
   */
  LibXR::Topic& TxTopic() { return tx_topic_; }

  /**
   * @brief 获取发布 `RxPacket` 的接收 Topic。
   * @return 本对象持有的接收 Topic 引用；不得在本对象销毁后继续使用。
   */
  LibXR::Topic& RxTopic() { return rx_topic_; }

  /**
   * @brief 获取发布 `Status` 的状态 Topic。
   * @return 本对象持有的状态 Topic 引用；不得在本对象销毁后继续使用。
   */
  LibXR::Topic& StatusTopic() { return status_topic_; }

  /**
   * @brief 获取最近一次缓存的状态快照。
   * @return 内部 `Status` 的只读引用；后续 `OnMonitor()` 可能更新其内容。
   */
  [[nodiscard]] const Status& LatestStatus() const { return latest_status_; }

  /**
   * @brief 非阻塞推进初始化、接收、发送或恢复状态机一步。
   *
   * 每次调用只处理当前状态：初始化/恢复态尝试或等待初始化，发送态检查终态，
   * 接收态按周期轮询接收数据，再检查是否有一条发送请求，最后按需发布心跳。
   * 一次接收轮询至多读取并发布 RX FIFO 中的一包；读取后会清空整个 RX FIFO，
   * 当时仍在 FIFO 中排队的其他载荷会被丢弃，不会在后续监控中发布或计数。
   * 时间比较采用调用时读取的毫秒时间值及 `uint32_t` 模减法。
   */
  void OnMonitor() override;

 private:
  /**
   * @brief 尝试初始化并校验无线配置。
   * @param now_ms 本次监控读取的毫秒时间，用于记录后续状态切换时刻。
   */
  void TryInitialize(uint32_t now_ms);

  /**
   * @brief 在轮询周期到达时校验设备状态并处理至多一个接收载荷。
   *
   * 发现接收数据时只读取 RX FIFO 中的一包，随后由 `PublishRx()` 完成接收收尾并
   * 清空整个 RX FIFO；其余已经排队的载荷会被丢弃，而不会留到下一次轮询处理。
   *
   * @param now_ms 本次监控读取的毫秒时间，用于轮询节流、状态切换和消息时间戳。
   */
  void MonitorReceive(uint32_t now_ms);

  /**
   * @brief 取出已就绪的单条发送请求，启动发送并发布 `TRANSMITTING` 状态。
   * @param now_ms 发送开始及状态切换的毫秒时间。
   * @pre `tx_subscriber_` 已有可用请求，且应用仍处于 `RECEIVE`。
   */
  void StartTransmission(uint32_t now_ms);

  /**
   * @brief 读取发送 STATUS，并按标志优先级和超时边界推进终态处理。
   * @param now_ms 当前毫秒时间，用于计算从 `tx_started_ms_` 起的模 2^32 间隔。
   */
  void MonitorTransmission(uint32_t now_ms);

  /**
   * @brief 执行已解析发送终态对应的收尾、计数、订阅者重装和状态切换。
   * @param outcome 已判定的非 `kPending` 发送结果。
   * @param transition 与 `outcome` 对应的声明式终态转换要求。
   * @param now_ms 终态处理及后续状态切换的毫秒时间。
   */
  void CompleteTransmission(
      Module::Nrf24l01State::TxOutcome outcome,
      Module::Nrf24l01State::TerminalTransition transition, uint32_t now_ms);

  /**
   * @brief 更新软件状态为 `RECEIVE`、记录切换时刻并发布状态。
   *
   * 此接口本身的契约不包含任何硬件切换保证；进入接收所需的具体硬件行为由实现层
   * 及其调用路径负责，不应仅依据本声明推断设备寄存器或 GPIO 已发生何种变化。
   *
   * @param now_ms 记录到 `state_changed_ms_` 并用于状态发布的毫秒时间。
   */
  void EnterReceive(uint32_t now_ms);

  /**
   * @brief 关闭设备、进入 `RECOVERING`、保存失败结果并发布状态。
   * @param result 触发恢复的发送或设备错误分类。
   * @param now_ms 恢复等待起点及状态发布的毫秒时间。
   */
  void EnterRecovery(TxResult result, uint32_t now_ms);

  /**
   * @brief 读取一个接收载荷，填充元数据后发布并更新累计数。
   *
   * 读取首包后调用接收收尾逻辑清空整个 RX FIFO，而不是仅弹出已读取的一包；
   * FIFO 中其他排队载荷会直接丢弃，也不会增加 `rx_count` 或获得发布序号。
   *
   * @param pipe STATUS 解码所得接收管道编号，调用前已验证在 0..5。
   * @param now_ms 写入 `RxPacket::received_at_ms` 的毫秒时间。
   */
  void PublishRx(uint8_t pipe, uint32_t now_ms);

  /**
   * @brief 为缓存状态分配序号、更新时间戳并发布状态快照。
   * @param now_ms 本次状态发布时间，保存为后续心跳节流基准。
   */
  void PublishStatus(uint32_t now_ms);

  /**
   * @brief 在状态周期到达时发布心跳；周期为 0 时每次调用均发布。
   * @param now_ms 当前毫秒时间，用于与上次状态发布时间作模 2^32 比较。
   */
  void PublishHeartbeat(uint32_t now_ms);

  /**
   * @brief 对累计计数执行饱和加一。
   * @param value 要更新的计数器；已为 `UINT32_MAX` 时保持不变。
   */
  static void Increment(uint32_t& value);

  // 配置与硬件总线；声明顺序保证总线先于依赖它的状态机操作可用。
  Config config_;              ///< 构造时复制、此后供状态机读取的配置。
  Module::Nrf24l01Bus bus_;   ///< 单个 NRF24L01 的软件 SPI 与寄存器访问层。

  // Topic 及单槽异步发送订阅者；订阅者只在 WAITING 时捕获下一次发布。
  LibXR::Topic tx_topic_;                              ///< `TxRequest` 输入通道。
  LibXR::Topic::ASyncSubscriber<TxRequest> tx_subscriber_;  ///< 单在途请求槽。
  LibXR::Topic rx_topic_;                              ///< `RxPacket` 输出通道。
  LibXR::Topic status_topic_;                          ///< `Status` 输出通道。

  // 对外状态缓存与当前发送上下文。
  Status latest_status_{};       ///< 最近状态快照，也是下一次发布的可变缓存。
  TxRequest active_request_{};   ///< 已从订阅者取出、当前正在处理的发送请求。

  // 所有毫秒时间均使用同一 `uint32_t` 时间基准，并通过无符号模减法比较。
  uint32_t tx_started_ms_ = 0U;           ///< 当前发送的起始时刻。
  uint32_t state_changed_ms_ = 0U;        ///< 最近一次软件状态切换时刻。
  uint32_t last_rx_poll_ms_ = 0U;         ///< 最近一次实际读取接收 STATUS 的时刻。
  uint32_t last_status_publish_ms_ = 0U;  ///< 最近一次状态/心跳发布时刻。

  // 下一条消息使用的序号；递增采用 `uint32_t` 自然回绕。
  uint32_t rx_sequence_ = 0U;      ///< 下一条 `RxPacket` 的序号。
  uint32_t status_sequence_ = 0U;  ///< 下一条 `Status` 的序号。

  // 时间戳有效位用于区分“尚未执行”与合法的 0 ms 时间值。
  bool has_rx_poll_time_ = false;       ///< 是否已有有效的 `last_rx_poll_ms_`。
  bool has_status_publish_time_ = false;  ///< 是否已有有效的状态发布时间。
};
