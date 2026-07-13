/**
 * @file NRF24L01.cpp
 * @brief NRF24L01 应用层的周期轮询状态机与 Topic 消息编排实现。
 *
 * 本实现由应用管理器周期调用 `OnMonitor()` 推进状态，不使用 IRQ。发送请求经
 * Topic 输入，接收包与状态快照经独立 Topic 输出；每次监控按固定优先级完成当前
 * 状态的一轮工作，从而将同步总线访问纳入非阻塞的应用级调度。
 */
#include "NRF24L01.hpp"

#include <limits>

#include "libxr_def.hpp"
#include "nrf24l01_state.hpp"
#include "thread.hpp"

namespace
{
// 接收态 MonitorReceive 使用的 STATUS 位掩码：bit 6 表示 RX 数据就绪，bit 7
// 仅在该接收态检查中被视为无效状态；发送态 EvaluateTx 不检查 bit 7。RX_P_NO
// 位于 bit [3:1]，解码值 0..5 为有效 Pipe，0b111 表示没有可读 Pipe。
constexpr uint8_t kRxDataReadyMask = 1U << 6U;
constexpr uint8_t kInvalidStatusMask = 1U << 7U;
constexpr uint8_t kRxPipeMask = 0x0EU;
constexpr uint8_t kRxPipeShift = 1U;
constexpr uint8_t kNoRxPipe = 0x07U;
}  // namespace

// 默认构造仅委托给完整构造路径，因而具有完全相同的断言、Topic 和注册副作用。
NRF24L01::NRF24L01(LibXR::HardwareContainer& hw,
                   LibXR::ApplicationManager& app)
    : NRF24L01(hw, app, Config{})
{
}

NRF24L01::NRF24L01(LibXR::HardwareContainer& hw,
                   LibXR::ApplicationManager& app, const Config& config)
    : config_(config),
      bus_(hw, config.ce_gpio_name, config.csn_gpio_name,
           config.sck_gpio_name, config.mosi_gpio_name,
           config.miso_gpio_name),
      tx_topic_(LibXR::Topic::CreateTopic<TxRequest>(config.tx_topic_name)),
      tx_subscriber_(tx_topic_),
      rx_topic_(LibXR::Topic::CreateTopic<RxPacket>(config.rx_topic_name)),
      status_topic_(LibXR::Topic::CreateTopic<Status>(config.status_topic_name))
{
  // 进入函数体前，成员初始化期间 bus_ 已通过 FindOrExit 解析 CE、CSN、SCK、
  // MOSI、MISO 五个 GPIO，并已创建三个 Topic。函数体再用 ASSERT 检查固定载荷
  // 宽度、非零发送超时和 ConfigurePins 结果；无线参数与射频频道的设备初始化
  // 留给后续轮询。发送订阅等待以及向应用管理器和硬件容器注册均在构造时发生，
  // 并会对对应容器产生持久副作用。
  ASSERT(config_.payload_width == PAYLOAD_WIDTH);
  ASSERT(config_.tx_timeout_ms > 0U);
  ASSERT(bus_.ConfigurePins());

  tx_subscriber_.StartWaiting();
  app.Register(*this);
  hw.Register(LibXR::Entry<NRF24L01>{*this, {config.hardware_name}});
}

NRF24L01::~NRF24L01() { bus_.Shutdown(); }

void NRF24L01::OnMonitor()
{
  const uint32_t now_ms = LibXR::Thread::GetTime();

  // 四态状态机每次调用只推进一个分支：INITIALIZING 尝试初始化，RECOVERING
  // 等待恢复周期，TRANSMITTING 检查发送终态，RECEIVE 处理常规收发与心跳。
  switch (latest_status_.state)
  {
    case State::INITIALIZING:
      TryInitialize(now_ms);
      return;

    case State::RECOVERING:
      if (!Module::Nrf24l01State::RecoveryElapsed(
              now_ms, state_changed_ms_, config_.recovery_period_ms))
      {
        return;
      }
      TryInitialize(now_ms);
      return;

    case State::TRANSMITTING:
      MonitorTransmission(now_ms);
      return;

    case State::RECEIVE:
      // 接收态的优先级固定为：先轮询并处理接收包；仍处于接收态时再启动一条
      // 已就绪发送请求；两者均未结束本轮处理时，最后才检查是否发布状态心跳。
      MonitorReceive(now_ms);
      if (latest_status_.state != State::RECEIVE)
      {
        return;
      }
      if (tx_subscriber_.Available())
      {
        StartTransmission(now_ms);
        return;
      }
      PublishHeartbeat(now_ms);
      return;
  }
}

void NRF24L01::TryInitialize(uint32_t now_ms)
{
  // 初始化失败立即关闭设备并进入恢复态，等待下一次重试；此路径不会清零发送、
  // 接收累计计数。成功时只清理最近结果、原始 STATUS 和接收轮询时间有效标记，
  // 随后进入接收软件状态，累计计数与消息序号继续沿用。
  const bool initialized = bus_.Initialize(
      config_.address, config_.channel, config_.data_rate,
      config_.output_power, config_.retry_delay_us, config_.retry_count,
      config_.payload_width);
  if (!initialized)
  {
    EnterRecovery(TxResult::DEVICE_ERROR, now_ms);
    return;
  }

  latest_status_.tx_result = TxResult::NONE;
  latest_status_.raw_status = 0U;
  has_rx_poll_time_ = false;
  EnterReceive(now_ms);
}

void NRF24L01::MonitorReceive(uint32_t now_ms)
{
  // 非零轮询周期用于限制 STATUS 读取频率；无符号减法按模 2^32 计算，因此允许
  // `uint32_t` 毫秒时钟自然回绕。周期为 0 时不节流，每次接收态监控都会轮询。
  if (has_rx_poll_time_ && config_.rx_poll_period_ms != 0U &&
      static_cast<uint32_t>(now_ms - last_rx_poll_ms_) <
          config_.rx_poll_period_ms)
  {
    return;
  }

  has_rx_poll_time_ = true;
  last_rx_poll_ms_ = now_ms;
  const uint8_t raw_status = bus_.ReadStatus();
  latest_status_.raw_status = raw_status;

  const bool invalid_status =
      (raw_status & kInvalidStatusMask) != 0U ||
      ((raw_status & Module::Nrf24l01State::kTxDsMask) != 0U &&
       (raw_status & Module::Nrf24l01State::kMaxRtMask) != 0U);
  // 在本接收态检查中，bit 7 或 TX_DS/MAX_RT 同时置位均说明 STATUS 不可信；
  // 此无效状态路径会关机并进入恢复态，不继续解释本次接收信息。
  if (invalid_status)
  {
    EnterRecovery(TxResult::INVALID_STATUS, now_ms);
    return;
  }
  // IsPoweredUp 回读 CONFIG；读到 0xFF 或 PWR_UP 未置位都会返回 false，二者在
  // 应用层统一映射为 DEVICE_ERROR，随后关机并进入恢复态。
  if (!bus_.IsPoweredUp())
  {
    EnterRecovery(TxResult::DEVICE_ERROR, now_ms);
    return;
  }

  if ((raw_status & kRxDataReadyMask) == 0U)
  {
    return;
  }

  const uint8_t pipe =
      static_cast<uint8_t>((raw_status & kRxPipeMask) >> kRxPipeShift);
  // Pipe 从 STATUS 的 RX_P_NO 字段解析；只接受硬件定义的 0..5，空 Pipe 编码
  // 0b111 及其他越界值均按无效状态恢复，避免读取来源不明的载荷。
  if (pipe == kNoRxPipe || pipe > 5U)
  {
    EnterRecovery(TxResult::INVALID_STATUS, now_ms);
    return;
  }
  PublishRx(pipe, now_ms);
}

void NRF24L01::StartTransmission(uint32_t now_ms)
{
  // 订阅者只提供一个已捕获请求槽：取出后立即装载该请求，形成唯一在途发送；
  // 总线启动过程会清除旧发送标志、清空 TX FIFO，再写入一个固定 32 字节载荷。
  // 订阅者要等发送终态处理显式重挂后，才能再次捕获下一条请求。
  active_request_ = tx_subscriber_.GetData();
  bus_.StartTx(active_request_.target_address, active_request_.payload);

  tx_started_ms_ = now_ms;
  state_changed_ms_ = now_ms;
  latest_status_.state = State::TRANSMITTING;
  latest_status_.tx_result = TxResult::NONE;
  latest_status_.raw_status = 0U;
  latest_status_.request_id = active_request_.request_id;
  PublishStatus(now_ms);
}

void NRF24L01::MonitorTransmission(uint32_t now_ms)
{
  // 监控阶段只采集 STATUS 和按模 2^32 计算的发送历时；两个状态辅助函数均为
  // 纯判定逻辑，负责按标志与超时优先级生成结果和转换描述，不直接访问硬件、
  // 修改本对象状态或发布 Topic。只有终态要求发布时才进入实际收尾。
  const uint8_t raw_status = bus_.ReadStatus();
  latest_status_.raw_status = raw_status;
  const uint32_t elapsed_ms =
      static_cast<uint32_t>(now_ms - tx_started_ms_);
  const Module::Nrf24l01State::TxDecision decision =
      Module::Nrf24l01State::EvaluateTx(raw_status, elapsed_ms,
                                        config_.tx_timeout_ms);
  const Module::Nrf24l01State::TerminalTransition transition =
      Module::Nrf24l01State::ResolveTerminalTransition(decision);

  if (!transition.publish_status)
  {
    return;
  }
  CompleteTransmission(decision.outcome, transition, now_ms);
}

void NRF24L01::CompleteTransmission(
    Module::Nrf24l01State::TxOutcome outcome,
    Module::Nrf24l01State::TerminalTransition transition, uint32_t now_ms)
{
  // 将纯状态机结果映射到公开 TxResult；kPending 不应到达收尾路径。无论终态
  // 成功与否，FinishTx 都先清除相关状态标志、清空整个 TX/RX FIFO 并恢复接收硬件
  // 配置，随后才更新成功/失败饱和计数并按转换描述重挂单槽订阅者。
  TxResult result = TxResult::INVALID_STATUS;
  switch (outcome)
  {
    case Module::Nrf24l01State::TxOutcome::kSuccess:
      result = TxResult::SUCCESS;
      break;
    case Module::Nrf24l01State::TxOutcome::kMaximumRetries:
      result = TxResult::MAX_RETRIES;
      break;
    case Module::Nrf24l01State::TxOutcome::kInvalidStatus:
      result = TxResult::INVALID_STATUS;
      break;
    case Module::Nrf24l01State::TxOutcome::kTimeout:
      result = TxResult::TIMEOUT;
      break;
    case Module::Nrf24l01State::TxOutcome::kPending:
      ASSERT(false);
      return;
  }

  bus_.FinishTx(config_.address);
  latest_status_.tx_result = result;

  if (result == TxResult::SUCCESS)
  {
    Increment(latest_status_.tx_success_count);
  }
  else
  {
    Increment(latest_status_.tx_failure_count);
  }

  if (transition.rearm_subscriber)
  {
    tx_subscriber_.StartWaiting();
  }
  if (transition.target_state ==
      Module::Nrf24l01State::TerminalState::kReceive)
  {
    // 成功终态直接发布接收软件状态；自动重传耗尽、无效状态和超时则转入恢复，
    // 并保留本次映射后的失败结果供状态 Topic 观察。
    ASSERT(!transition.requires_reinitialize);
    EnterReceive(now_ms);
    return;
  }

  ASSERT(transition.target_state ==
         Module::Nrf24l01State::TerminalState::kRecovering);
  ASSERT(transition.requires_reinitialize);
  EnterRecovery(result, now_ms);
}

void NRF24L01::EnterReceive(uint32_t now_ms)
{
  // 此函数只更新软件状态、切换时间并发布状态，不执行任何硬件模式操作。
  latest_status_.state = State::RECEIVE;
  state_changed_ms_ = now_ms;
  PublishStatus(now_ms);
}

void NRF24L01::EnterRecovery(TxResult result, uint32_t now_ms)
{
  // 恢复态入口先关闭设备，再记录恢复起点与原因；后续由周期轮询决定何时重试。
  bus_.Shutdown();
  latest_status_.state = State::RECOVERING;
  latest_status_.tx_result = result;
  state_changed_ms_ = now_ms;
  PublishStatus(now_ms);
}

void NRF24L01::PublishRx(uint8_t pipe, uint32_t now_ms)
{
  // 每次仅读取并发布 RX FIFO 队首的一包；FinishRx 随后清空整个 RX FIFO，
  // 当时仍排队的其他包会被丢弃，不获得序号，也不计入接收累计数。
  RxPacket packet;
  bus_.ReadRxPayload(packet.payload.data(), packet.payload.size());
  bus_.FinishRx();

  packet.pipe = pipe;
  packet.received_at_ms = now_ms;
  packet.sequence = rx_sequence_++;
  rx_topic_.Publish(packet);

  Increment(latest_status_.rx_count);
  PublishStatus(now_ms);
}

void NRF24L01::PublishStatus(uint32_t now_ms)
{
  // 每次状态发布使用当前序号后自然递增，并同时刷新心跳节流基准；序号本身采用
  // `uint32_t` 自然回绕，不使用饱和策略。
  latest_status_.sequence = status_sequence_++;
  last_status_publish_ms_ = now_ms;
  has_status_publish_time_ = true;
  status_topic_.Publish(latest_status_);
}

void NRF24L01::PublishHeartbeat(uint32_t now_ms)
{
  // 尚无发布基准时立即发布；period=0 表示每次调用都发布，否则以支持回绕的
  // 无符号时间差判断是否到达周期。心跳复用 PublishStatus，因此共享状态序号。
  if (!has_status_publish_time_ || config_.status_period_ms == 0U ||
      static_cast<uint32_t>(now_ms - last_status_publish_ms_) >=
          config_.status_period_ms)
  {
    PublishStatus(now_ms);
  }
}

void NRF24L01::Increment(uint32_t& value)
{
  // 累计计数采用饱和加一，达到 uint32_t 最大值后保持不变，避免回绕为零。
  if (value != std::numeric_limits<uint32_t>::max())
  {
    ++value;
  }
}
