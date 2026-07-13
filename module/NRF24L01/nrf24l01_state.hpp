/**
 * @file nrf24l01_state.hpp
 * @brief NRF24L01 发送终态判定与恢复等待时间判定的无副作用辅助逻辑。
 *
 * 本文件只把调用方提供的寄存器快照和时间值转换为状态机描述，不访问硬件、
 * 不发布 Topic，也不直接修改应用状态。实际执行切换、恢复和发布的职责属于调用方。
 */
#pragma once

#include <cstdint>

/**
 * @brief NRF24L01 应用状态机使用的纯判定类型与函数。
 *
 * 该命名空间刻意与总线访问层分离，使发送结果、终态切换和恢复周期边界可以在
 * 不连接 NRF24L01 硬件的情况下独立测试。
 */
namespace Module::Nrf24l01State
{

/** NRF24L01 STATUS 寄存器的 TX_DS 位（bit 5），表示数据发送成功。 */
constexpr std::uint8_t kTxDsMask = 1U << 5U;
/** NRF24L01 STATUS 寄存器的 MAX_RT 位（bit 4），表示自动重传达到上限。 */
constexpr std::uint8_t kMaxRtMask = 1U << 4U;

/** @brief 一次发送在当前 STATUS 快照和超时边界下的判定结果。 */
enum class TxOutcome
{
  kPending,         ///< 尚无终态标志且未到超时边界，应继续等待。
  kSuccess,         ///< STATUS 的 TX_DS 置位，发送成功。
  kMaximumRetries,  ///< STATUS 的 MAX_RT 置位，自动重传达到上限。
  kInvalidStatus,   ///< TX_DS 与 MAX_RT 同时置位，视为不可信的终态组合。
  kTimeout,         ///< 没有更高优先级终态标志，且已到或超过超时边界。
};

/**
 * @brief `EvaluateTx()` 随判定返回的建议后续策略。
 *
 * 枚举值只描述调用方适合采取的策略，不代表相应动作已经执行，也不产生任何
 * 硬件或状态机副作用。`ResolveTerminalTransition()` 当前仅依据 `TxOutcome`
 * 生成转换描述，不读取此策略字段。
 */
enum class RecoveryAction
{
  kKeepWaiting,               ///< 建议保持当前发送等待状态。
  kResumeRx,                  ///< 建议结束发送处理并恢复接收流程。
  kReinitializeAndResumeRx,   ///< 建议重新初始化设备后再恢复接收流程。
};

/** @brief 将发送结果与建议后续策略绑定的一次纯判定快照。 */
struct TxDecision
{
  TxOutcome outcome;         ///< 本次采样得到的发送结果。
  RecoveryAction recovery;  ///< 建议策略；仅为描述，不表示策略已经执行。
};

/** @brief 发送判定对应的软件状态机目标。 */
enum class TerminalState
{
  kUnchanged,   ///< 保持调用方当前状态，尚不发生终态切换。
  kReceive,     ///< 终态处理后目标为接收状态。
  kRecovering,  ///< 终态处理后目标为恢复状态。
};

/**
 * @brief 调用方处理一次发送判定时应执行的终态转换描述。
 *
 * 所有字段都是声明式要求；构造该结构不会重装订阅者、发布状态或重新初始化设备。
 */
struct TerminalTransition
{
  TerminalState target_state;  ///< 建议切换到的状态；`kUnchanged` 表示保持原状态。
  bool rearm_subscriber;       ///< 是否应重新使发送订阅者进入等待下一条请求的状态。
  bool publish_status;         ///< 是否应发布本次发送的终态状态消息。
  bool requires_reinitialize;  ///< 恢复流程后续是否要求重新初始化无线设备。
};

/**
 * @brief 按固定优先级判定一次发送是否到达终态。
 *
 * 判定优先级从高到低为：TX_DS 与 MAX_RT 同时置位的无效组合、TX_DS 成功、
 * MAX_RT 重试耗尽、超时、继续等待。因此终态标志在恰好到达超时边界时仍优先于
 * 超时；`elapsed_ms == timeout_ms` 且无终态标志时判为超时。除 TX_DS 和 MAX_RT
 * 外的 STATUS 位不参与本函数判定。
 *
 * 本函数不计算时间差；`elapsed_ms` 的来源及是否采用模运算由调用方负责。
 * `timeout_ms == 0` 时，只要没有更高优先级终态标志就会立即判为超时。
 *
 * @param status NRF24L01 STATUS 寄存器快照，仅检查 `kTxDsMask` 和 `kMaxRtMask`。
 * @param elapsed_ms 调用方计算的本次发送已持续时间，单位 ms。
 * @param timeout_ms 超时阈值，单位 ms；达到阈值即超时。
 * @return 发送结果及与该结果配套的建议后续策略；不表示策略已执行。
 */
constexpr TxDecision EvaluateTx(std::uint8_t status,
                                std::uint32_t elapsed_ms,
                                std::uint32_t timeout_ms)
{
  const bool tx_succeeded = (status & kTxDsMask) != 0U;
  const bool maximum_retries = (status & kMaxRtMask) != 0U;

  if (tx_succeeded && maximum_retries)
  {
    return {TxOutcome::kInvalidStatus,
            RecoveryAction::kReinitializeAndResumeRx};
  }
  if (tx_succeeded)
  {
    return {TxOutcome::kSuccess, RecoveryAction::kResumeRx};
  }
  if (maximum_retries)
  {
    return {TxOutcome::kMaximumRetries,
            RecoveryAction::kReinitializeAndResumeRx};
  }
  if (elapsed_ms >= timeout_ms)
  {
    return {TxOutcome::kTimeout,
            RecoveryAction::kReinitializeAndResumeRx};
  }

  return {TxOutcome::kPending, RecoveryAction::kKeepWaiting};
}

/**
 * @brief 把发送结果映射为调用方需要执行的终态转换描述。
 *
 * `kPending` 映射为全部副作用关闭且状态不变；`kSuccess` 映射为重装订阅者、
 * 发布终态并进入接收；其余结果映射为重装订阅者、发布终态并请求重新初始化后
 * 进入恢复状态。本函数只检查 `decision.outcome`，不会读取或执行
 * `decision.recovery`。
 *
 * @param decision 已完成的发送判定；通常由 `EvaluateTx()` 返回。
 * @return 声明式终态转换要求；本函数自身不执行其中任何动作。
 */
constexpr TerminalTransition ResolveTerminalTransition(TxDecision decision)
{
  if (decision.outcome == TxOutcome::kPending)
  {
    return {TerminalState::kUnchanged, false, false, false};
  }
  if (decision.outcome == TxOutcome::kSuccess)
  {
    return {TerminalState::kReceive, true, true, false};
  }
  return {TerminalState::kRecovering, true, true, true};
}

/**
 * @brief 判断从进入恢复状态起是否已达到恢复等待周期。
 *
 * 计算严格等价于先执行 `uint32_t(now_ms - state_changed_ms)` 的模 2^32 减法，
 * 再与等待周期比较；相等时返回 true，`recovery_period_ms == 0` 时始终返回 true。
 * 该函数不读取时钟，也不识别时钟回拨或跨越多个完整 2^32 周期的情况，正确性仅
 * 依赖调用方传入的无符号时间值及其模减法语义。
 *
 * @param now_ms 调用方提供的当前毫秒时间值。
 * @param state_changed_ms 进入当前恢复状态时记录的毫秒时间值。
 * @param recovery_period_ms 要求等待的恢复周期，单位 ms。
 * @return 模减法所得间隔大于或等于恢复周期时为 true，否则为 false。
 */
constexpr bool RecoveryElapsed(std::uint32_t now_ms,
                               std::uint32_t state_changed_ms,
                               std::uint32_t recovery_period_ms)
{
  return static_cast<std::uint32_t>(now_ms - state_changed_ms) >=
         recovery_period_ms;
}

}  // namespace Module::Nrf24l01State（NRF24L01 状态机纯判定逻辑）
