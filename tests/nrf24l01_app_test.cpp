#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

#include "nrf24l01_state.hpp"

namespace
{

int g_failure_count = 0;

/** Records a failed behavioral expectation. */
void Expect(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "FAIL: " << message << '\n';
    ++g_failure_count;
  }
}

/** Verifies a custom timeout is pending before, and terminal at, its boundary. */
void TestCustomTimeoutBoundary()
{
  const auto before_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 24U, 25U);
  const auto at_timeout =
      Module::Nrf24l01State::EvaluateTx(0U, 25U, 25U);

  Expect(before_timeout.outcome ==
             Module::Nrf24l01State::TxOutcome::kPending,
         "24 ms must remain pending with a 25 ms timeout");
  Expect(at_timeout.outcome == Module::Nrf24l01State::TxOutcome::kTimeout,
         "25 ms must time out with a 25 ms timeout");
}

/** Verifies TX_DS wins over an elapsed timeout. */
void TestSuccessHasPriorityOverTimeout()
{
  const auto decision = Module::Nrf24l01State::EvaluateTx(
      Module::Nrf24l01State::kTxDsMask, 25U, 25U);

  Expect(decision.outcome == Module::Nrf24l01State::TxOutcome::kSuccess,
         "TX_DS must win over timeout at the same boundary");
}

/** Verifies MAX_RT wins over an elapsed timeout. */
void TestMaximumRetriesHasPriorityOverTimeout()
{
  const auto decision = Module::Nrf24l01State::EvaluateTx(
      Module::Nrf24l01State::kMaxRtMask, 25U, 25U);

  Expect(decision.outcome ==
             Module::Nrf24l01State::TxOutcome::kMaximumRetries,
         "MAX_RT must win over timeout at the same boundary");
}

/** Verifies an impossible pair of terminal flags has highest priority. */
void TestInvalidStatusHasPriorityOverTerminalFlagsAndTimeout()
{
  const std::uint8_t invalid_status = static_cast<std::uint8_t>(
      Module::Nrf24l01State::kTxDsMask |
      Module::Nrf24l01State::kMaxRtMask);
  const auto decision =
      Module::Nrf24l01State::EvaluateTx(invalid_status, 25U, 25U);

  Expect(decision.outcome ==
             Module::Nrf24l01State::TxOutcome::kInvalidStatus,
         "TX_DS plus MAX_RT must win over success, retries, and timeout");
}

/** Verifies a pending decision has no observable terminal side effects. */
void TestPendingDecisionDoesNotTransition()
{
  const auto transition = Module::Nrf24l01State::ResolveTerminalTransition(
      Module::Nrf24l01State::EvaluateTx(0U, 24U, 25U));

  Expect(transition.target_state ==
             Module::Nrf24l01State::TerminalState::kUnchanged,
         "pending TX must keep the current state");
  Expect(!transition.rearm_subscriber,
         "pending TX must not rearm the subscriber");
  Expect(!transition.publish_status,
         "pending TX must not publish a terminal status");
  Expect(!transition.requires_reinitialize,
         "pending TX must not request reinitialization");
}

/** Verifies successful TX requests exactly one receive transition/status. */
void TestSuccessTransitionsToReceiveOnce()
{
  const auto transition = Module::Nrf24l01State::ResolveTerminalTransition(
      Module::Nrf24l01State::EvaluateTx(
          Module::Nrf24l01State::kTxDsMask, 1U, 25U));

  Expect(transition.target_state ==
             Module::Nrf24l01State::TerminalState::kReceive,
         "successful TX must transition to RECEIVE");
  Expect(transition.rearm_subscriber,
         "successful TX must rearm the subscriber once");
  Expect(transition.publish_status,
         "successful TX must publish one terminal status");
  Expect(!transition.requires_reinitialize,
         "successful TX must not reinitialize the radio");
}

/** Verifies every failed terminal outcome requests recovery and reinit. */
void TestFailedTerminalOutcomesTransitionToRecoveryOnce()
{
  const std::array<Module::Nrf24l01State::TxDecision, 3> decisions = {
      Module::Nrf24l01State::EvaluateTx(
          Module::Nrf24l01State::kMaxRtMask, 1U, 25U),
      Module::Nrf24l01State::EvaluateTx(
          static_cast<std::uint8_t>(Module::Nrf24l01State::kTxDsMask |
                                    Module::Nrf24l01State::kMaxRtMask),
          1U, 25U),
      Module::Nrf24l01State::EvaluateTx(0U, 25U, 25U)};

  for (const auto& decision : decisions)
  {
    const auto transition =
        Module::Nrf24l01State::ResolveTerminalTransition(decision);
    Expect(transition.target_state ==
               Module::Nrf24l01State::TerminalState::kRecovering,
           "failed terminal TX must transition to RECOVERING");
    Expect(transition.rearm_subscriber,
           "failed terminal TX must rearm the subscriber once");
    Expect(transition.publish_status,
           "failed terminal TX must publish one terminal status");
    Expect(transition.requires_reinitialize,
           "failed terminal TX must request reinitialization");
  }
}

/** Verifies recovery timing remains correct across uint32 wrap. */
void TestRecoveryElapsedUsesWrapSafeBoundary()
{
  constexpr std::uint32_t changed =
      std::numeric_limits<std::uint32_t>::max() - 10U;
  constexpr std::uint32_t now = 4U;

  Expect(!Module::Nrf24l01State::RecoveryElapsed(now, changed, 16U),
         "recovery must wait before its wrapped boundary");
  Expect(Module::Nrf24l01State::RecoveryElapsed(now, changed, 15U),
         "recovery must elapse at its wrapped boundary");
}

}  // namespace

int main()
{
  TestCustomTimeoutBoundary();
  TestSuccessHasPriorityOverTimeout();
  TestMaximumRetriesHasPriorityOverTimeout();
  TestInvalidStatusHasPriorityOverTerminalFlagsAndTimeout();
  TestPendingDecisionDoesNotTransition();
  TestSuccessTransitionsToReceiveOnce();
  TestFailedTerminalOutcomesTransitionToRecoveryOnce();
  TestRecoveryElapsedUsesWrapSafeBoundary();

  if (g_failure_count != 0)
  {
    std::cerr << g_failure_count << " NRF24L01 state test(s) failed.\n";
    return 1;
  }

  std::cout << "NRF24L01 state tests passed.\n";
  return 0;
}
