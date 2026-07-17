# PID Tuning Speed Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the VOFA+ JustFloat stream to 12 channels containing six tuning values, left/right targets, and four measured wheel speeds, emitted non-blockingly at 50 Hz.

**Architecture:** Keep the existing ASCII command parser and `LibXR::UART::Write()` transport. Pass the main-loop millisecond timestamp into `Console::Poll(uint32_t)`, schedule one pending snapshot every 20 ms, and build the frame from the existing PID/Tracking configuration plus `car_control_sample`.

**Tech Stack:** C++20 firmware, LibXR UART/WritePort, TI MSPM0G3507, PowerShell test runners, host `g++`, CMake/Ninja ARM build.

---

## Workspace Safety

The current worktree contains pre-existing uncommitted changes, including changes in
`src/app_main.cpp` and knowledge-base files. Do not reset, revert, or broadly stage the
worktree. Apply only the edits listed below and leave implementation changes uncommitted
for the user to review; a path-level commit of `src/app_main.cpp` would otherwise include
unrelated work.

The following baseline commands passed before implementation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_protocol_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_console_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_pid_tuning_justfloat_integration.ps1
```

## File Structure

- Modify `tests/pid_tuning_protocol_test.cpp`
  - Defines the 12-channel, 52-byte wire-contract expectations.
- Modify `tests/fakes/pid_tuning/car_control_support.hpp`
  - Provides the target/measured speed snapshot used by the real Console implementation.
- Modify `tests/pid_tuning_console_test.cpp`
  - Verifies channel mapping, 20 ms scheduling, GET behavior, and UART retry behavior.
- Modify `tests/verify_pid_tuning_justfloat_integration.ps1`
  - Replaces the old GET-only source contract with the 12-channel periodic contract.
- Modify `src/pid_tuning_console.hpp`
  - Owns the 12-value frame constants, 20 ms period, timestamped Poll API, and last successful TX time.
- Modify `src/pid_tuning_console.cpp`
  - Builds the 12-value snapshot and implements wrap-safe periodic scheduling.
- Modify `src/app_main.cpp`
  - Passes the already computed `now` value to `Console::Poll(now)`.
- Modify `.helloagents/modules/pid_tuning.md`
  - Documents the final 12-channel protocol and trigger behavior.
- Modify `.helloagents/CHANGELOG.md`
  - Records the new speed telemetry capability under `Unreleased / 新增`.

### Task 1: Lock the 12-Channel Wire Contract with a Failing Test

**Files:**
- Modify: `tests/pid_tuning_protocol_test.cpp`
- Test: `tests/run_pid_tuning_protocol_tests.ps1`

- [ ] **Step 1: Replace the JustFloat test values and size assertions**

Update `TestJustFloatFramePreservesChannelOrderAndTail()` to use 12 distinguishable
values and the new payload/tail offsets:

```cpp
void TestJustFloatFramePreservesChannelOrderAndTail()
{
  const PidTuning::SnapshotValues values{
      1.0f, -2.5f, 0.125f, 0.25f, 2.0f, 0.0625f,
      7.0f, 6.0f, 6.8f, 5.9f, 6.7f, 5.8f};
  PidTuning::JustFloatFrame frame{};
  PidTuning::EncodeJustFloatFrame(values, frame);

  static_assert(PidTuning::kSnapshotValueCount == 12U);
  static_assert(PidTuning::kJustFloatFrameSize == 52U);
  Expect(frame.size() == PidTuning::kJustFloatFrameSize,
         "JustFloat frame must contain twelve floats and a four-byte tail");
  Expect(frame[0] == 0x00U && frame[1] == 0x00U &&
             frame[2] == 0x80U && frame[3] == 0x3FU,
         "first channel must be little-endian IEEE-754 1.0");

  PidTuning::SnapshotValues decoded{};
  std::memcpy(decoded.data(), frame.data(),
              decoded.size() * sizeof(decoded[0]));
  Expect(decoded == values,
         "JustFloat payload must preserve all twelve channels in order");

  constexpr std::array<uint8_t, 4U> expected_tail{
      0x00U, 0x00U, 0x80U, 0x7FU};
  for (std::size_t index = 0U; index < expected_tail.size(); ++index)
  {
    Expect(frame[48U + index] == expected_tail[index],
           "JustFloat frame tail must start after twelve float channels");
  }
}
```

- [ ] **Step 2: Run the protocol test and verify RED**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_protocol_tests.ps1
```

Expected: compilation fails at the new static assertions because the production header
still defines 8 channels and a 36-byte frame.

### Task 2: Lock Runtime Mapping and Scheduling with Failing Tests

**Files:**
- Modify: `tests/fakes/pid_tuning/car_control_support.hpp`
- Modify: `tests/pid_tuning_console_test.cpp`
- Modify: `tests/verify_pid_tuning_justfloat_integration.ps1`
- Test: `tests/run_pid_tuning_console_tests.ps1`

- [ ] **Step 1: Extend the control-support fake with the real snapshot shape**

Add `<array>` and the four motor indexes to the fake, then add the minimal fields used by
the Console:

```cpp
namespace Module
{
class MotorGroup
{
 public:
  enum MotorId : uint8_t
  {
    kFrontLeft = 0,
    kFrontRight,
    kBackLeft,
    kBackRight,
    kMotorCount,
  };
};
}  // namespace Module

namespace CarControlSupport
{
struct CarControlSample
{
  std::array<float, Module::MotorGroup::kMotorCount> target_speed{};
  std::array<float, Module::MotorGroup::kMotorCount> measured_speed{};
};

inline uint32_t reset_speed_pids_count = 0U;
inline void ResetSpeedPids() { ++reset_speed_pids_count; }
}  // namespace CarControlSupport

inline CarControlSupport::CarControlSample car_control_sample{};
```

Replace the fake's current `namespace Module::MotorGroup` constants with the class above
so its public names match production `Module::MotorGroup`.

- [ ] **Step 2: Reset and configure telemetry values in the Console test**

Add this reset inside `ResetFixtures()`:

```cpp
car_control_sample = {};
```

Add these values inside `ConfigureSnapshotValues()`:

```cpp
car_control_sample.target_speed = {7.0f, 6.0f, 7.0f, 6.0f};
car_control_sample.measured_speed = {6.8f, 5.9f, 6.7f, 5.8f};
```

Change `DecodeSnapshot()`'s failure message from 36 bytes to 52 bytes.

- [ ] **Step 3: Change the GET mapping assertion to the approved order**

Call `console.Poll(0U)` and assert:

```cpp
Expect(get_values[0] == 0.05f && get_values[1] == 0.15f &&
           get_values[2] == 0.4f && get_values[3] == 0.1f &&
           get_values[4] == 3.0f && get_values[5] == 0.05f &&
           get_values[6] == 7.0f && get_values[7] == 6.0f &&
           get_values[8] == 6.8f && get_values[9] == 5.9f &&
           get_values[10] == 6.7f && get_values[11] == 5.8f,
       "GET snapshot channels must contain tuning, target, and feedback values");
```

Keep `speed_pid_config[0].i_limit` and `.out_limit` configured in the fixture so their
absence from this assertion proves they are no longer uplink fields. Keep the existing
SET boundary tests unchanged so both parameters remain writable.

- [ ] **Step 4: Add the exact 20 ms periodic boundary test**

Add:

```cpp
void TestAutomaticSnapshotPeriodIsTwentyMilliseconds()
{
  ResetFixtures();
  LibXR::UART uart;
  Tracking tracking;
  ConfigureSnapshotValues(tracking);
  PidTuning::Console console(uart, tracking);

  console.Poll(19U);
  Expect(uart.SuccessfulWriteCount() == 0U,
         "automatic snapshot must not be sent before 20 ms");

  console.Poll(20U);
  Expect(uart.SuccessfulWriteCount() == 1U,
         "automatic snapshot must be sent at the 20 ms boundary");

  console.Poll(39U);
  Expect(uart.SuccessfulWriteCount() == 1U,
         "next snapshot must remain pending until another 20 ms elapsed");

  console.Poll(40U);
  Expect(uart.SuccessfulWriteCount() == 2U,
         "automatic snapshots must continue at 50 Hz");
}
```

Call it from `main()`. Pass explicit times to every existing `Poll()` invocation:
use `0U` while exercising command-triggered snapshots, and monotonically increasing
values below 20 ms for retry-only assertions.

- [ ] **Step 5: Update the integration contract to expect periodic telemetry**

Replace the block that rejects automatic scheduling with checks for:

```powershell
if ($consoleHeader -notmatch 'kSnapshotValueCount\s*=\s*12U' -or
    $consoleHeader -notmatch 'kSnapshotPeriodMs\s*=\s*20U')
{
  throw 'PID tuning header must define twelve channels and a 20 ms period.'
}

if ($consoleSource -notmatch
      'void\s+Console::Poll\s*\(\s*uint32_t\s+now_ms\s*\)' -or
    $consoleSource -notmatch
      'static_cast<uint32_t>\s*\(\s*now_ms\s*-\s*last_snapshot_time_ms_\s*\)')
{
  throw 'Console::Poll must implement wrap-safe 20 ms scheduling.'
}
```

Remove the old rule requiring exactly one `snapshot_pending_ = true` assignment. Require
one assignment in the GET branch and one assignment in the periodic due branch.

Change the guarded app-main polling pattern to:

```powershell
Assert-SingleGuardedBlock $appMainCode `
  'pid_tuning_console\.Poll\s*\(\s*now\s*\)\s*;' 'PID console polling'
```

Add a source-order check that the `SnapshotValues` initializer contains, in order:

```text
velocity_gain, static_output, speed_pid_config[0].p, speed_pid_config[0].i,
turn_kp, turn_kd, target_speed[kFrontLeft], target_speed[kFrontRight],
measured_speed[kFrontLeft], measured_speed[kFrontRight],
measured_speed[kBackLeft], measured_speed[kBackRight]
```

The initializer check must reject `.i_limit` and `.out_limit`; do not reject those names
from the command parser or `ApplyParameter()`.

- [ ] **Step 6: Run the Console and integration tests and verify RED**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_console_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_pid_tuning_justfloat_integration.ps1
```

Expected: the Console test fails to compile because `Poll(uint32_t)` and the 12-channel
snapshot are not implemented; the integration check fails because production still uses
the GET-only `Poll()` contract.

### Task 3: Implement 12 Channels and 50 Hz Non-Blocking TX

**Files:**
- Modify: `src/pid_tuning_console.hpp`
- Modify: `src/pid_tuning_console.cpp`
- Modify: `src/app_main.cpp`
- Test: `tests/run_pid_tuning_protocol_tests.ps1`
- Test: `tests/run_pid_tuning_console_tests.ps1`

- [ ] **Step 1: Update frame and period constants**

In `src/pid_tuning_console.hpp`, use:

```cpp
constexpr std::size_t kLineCapacity = 64U;
constexpr std::size_t kSnapshotValueCount = 12U;
constexpr std::size_t kJustFloatTailSize = 4U;
constexpr uint32_t kSnapshotPeriodMs = 20U;
constexpr std::size_t kJustFloatFrameSize =
    kSnapshotValueCount * sizeof(float) + kJustFloatTailSize;
```

Update the encoder comment to say “十二通道” and “52 字节”.

- [ ] **Step 2: Make Poll and snapshot emission time-aware**

Change the public and private declarations to:

```cpp
void Poll(uint32_t now_ms);
bool TryEmitSnapshot(uint32_t now_ms);
```

Add this member after `snapshot_pending_`:

```cpp
uint32_t last_snapshot_time_ms_ = 0U;
```

- [ ] **Step 3: Build the approved 12-channel snapshot**

Replace the current initializer in `TryEmitSnapshot()` with:

```cpp
const Tracking::Config tracking_config = tracking_.GetConfig();
const SnapshotValues values{
    speed_feedforward_config[0].velocity_gain,
    speed_feedforward_config[0].static_output,
    speed_pid_config[0].p,
    speed_pid_config[0].i,
    tracking_config.turn_kp,
    tracking_config.turn_kd,
    car_control_sample.target_speed[Module::MotorGroup::kFrontLeft],
    car_control_sample.target_speed[Module::MotorGroup::kFrontRight],
    car_control_sample.measured_speed[Module::MotorGroup::kFrontLeft],
    car_control_sample.measured_speed[Module::MotorGroup::kFrontRight],
    car_control_sample.measured_speed[Module::MotorGroup::kBackLeft],
    car_control_sample.measured_speed[Module::MotorGroup::kBackRight]};
```

Change `TryEmitSnapshot()` so successful enqueue finishes with:

```cpp
snapshot_pending_ = false;
last_snapshot_time_ms_ = now_ms;
return true;
```

Every early failure path must return without changing either value.

- [ ] **Step 4: Add wrap-safe automatic scheduling**

Implement `Poll()` as:

```cpp
void Console::Poll(uint32_t now_ms)
{
  if (static_cast<uint32_t>(now_ms - last_snapshot_time_ms_) >=
      kSnapshotPeriodMs)
  {
    snapshot_pending_ = true;
  }

  for (std::size_t count = 0U; count < kRxByteBudget; ++count)
  {
    char byte = '\0';
    const ReadByteResult result = TryReadByte(byte);
    if (result == ReadByteResult::kNone)
    {
      break;
    }
    if (result == ReadByteResult::kError)
    {
      return;
    }
    if (ProcessByte(byte))
    {
      break;
    }
  }

  (void)TryEmitSnapshot(now_ms);
}
```

This deliberately keeps a failed periodic write pending and makes the next successful
write the start of the following 20 ms interval.

- [ ] **Step 5: Pass the existing main-loop timestamp**

In the PID tuning guard in `src/app_main.cpp`, replace:

```cpp
pid_tuning_console.Poll();
```

with:

```cpp
pid_tuning_console.Poll(now);
```

Do not move the call or change the 5 ms vehicle-control cadence.

- [ ] **Step 6: Run focused tests and verify GREEN**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_protocol_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_console_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_pid_tuning_justfloat_integration.ps1
```

Expected:

```text
pid tuning protocol tests passed.
pid tuning console tests passed.
pid tuning JustFloat integration contract passed.
```

### Task 4: Synchronize Documentation and Run Full Verification

**Files:**
- Modify: `.helloagents/modules/pid_tuning.md`
- Modify: `.helloagents/CHANGELOG.md`
- Modify: `.helloagents/plan/202607172130_pid-tuning-speed-telemetry/tasks.md`
- Test: `build/pid-debug`

- [ ] **Step 1: Rewrite the knowledge-base wire contract**

Update `.helloagents/modules/pid_tuning.md` to state:

```text
每帧固定 52 字节：前 48 字节是 12 个 float，最后四字节为 00 00 80 7F。
自动发送周期为 20 ms；pid,get 可立即请求；失败发送保留 pending。
```

Replace the channel table with the exact I0–I11 table from the approved design:

```text
velocity_gain, static_output, p, i, turn_kp, turn_kd,
left_target, right_target, front_left_measured, front_right_measured,
back_left_measured, back_right_measured
```

State explicitly that `i_limit/out_limit` remain writable but are not uplink channels.
Correct stale references to the retired `src/pid_tuning_protocol.hpp`; the protocol types
currently live in `src/pid_tuning_console.hpp`.

- [ ] **Step 2: Run all focused checks again**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_protocol_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_pid_tuning_console_tests.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_pid_tuning_justfloat_integration.ps1
```

Expected: all three pass with the messages listed in Task 3.

- [ ] **Step 3: Build the PID-enabled ARM firmware**

Run:

```powershell
cmake --build build/pid-debug
```

Expected: Ninja exits with code 0, `src/pid_tuning_console.cpp` compiles, the ELF links,
and the post-build flash-budget check succeeds.

- [ ] **Step 4: Review the scoped diff**

Run:

```powershell
git diff --check -- src/pid_tuning_console.hpp src/pid_tuning_console.cpp src/app_main.cpp tests/pid_tuning_protocol_test.cpp tests/pid_tuning_console_test.cpp tests/fakes/pid_tuning/car_control_support.hpp tests/verify_pid_tuning_justfloat_integration.ps1 .helloagents/modules/pid_tuning.md .helloagents/CHANGELOG.md
git diff --stat -- src/pid_tuning_console.hpp src/pid_tuning_console.cpp src/app_main.cpp tests/pid_tuning_protocol_test.cpp tests/pid_tuning_console_test.cpp tests/fakes/pid_tuning/car_control_support.hpp tests/verify_pid_tuning_justfloat_integration.ps1 .helloagents/modules/pid_tuning.md .helloagents/CHANGELOG.md
```

Expected: no whitespace errors; only the nine listed files appear in the scoped review.
Do not stage or commit these implementation changes because several overlap pre-existing
worktree changes.

- [ ] **Step 5: Complete the HelloAGENTS task record**

Mark tasks 1.1–3.1 complete in
`.helloagents/plan/202607172130_pid-tuning-speed-telemetry/tasks.md`, record the RED/GREEN
commands and ARM build result, then validate the package.

- [ ] **Step 6: Archive the package and verify the changelog**

Archive the validated package through PackageService. Its changelog update must appear
under `.helloagents/CHANGELOG.md` → `Unreleased` → `新增` with this content and the final
archive link:

```markdown
- **[PidTuning]**: 将 VOFA+ JustFloat 扩展为 12 路、50 Hz 轮速遥测，发送六项调参值、左右目标速度和四轮编码器实测速度；`i_limit/out_limit` 保留可调但不再上传 — by xiaoshuaijie
  - 方案: [202607172130_pid-tuning-speed-telemetry](archive/2026-07/202607172130_pid-tuning-speed-telemetry/)
  - 决策: pid-tuning-speed-telemetry#D001（主循环时间驱动 20 ms 非阻塞发送）
```

Keep every unrelated changelog entry unchanged. Confirm the active package directory no
longer exists and the archived `proposal.md` and `tasks.md` are both present.
