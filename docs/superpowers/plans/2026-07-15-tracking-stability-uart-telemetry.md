# Tracking Stability and UART Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修正灰度输入极性和速度前馈饱和，并通过 `UART_0_INST` 非阻塞输出可用于实车复盘的循迹 CSV 遥测。

**Architecture:** 用一个可在主机编译的 `tracking_diagnostics.hpp` 保存循迹运行配置、定点 CSV 编码器和固定容量发送队列；控制层复用其中的前馈限制函数，应用层只负责组装快照并在 UART FIFO 可写时排空队列。所有算法行为先在现有 leader mission 主机测试中失败，再写最小实现。

**Tech Stack:** C++20 固件、C++17 主机测试、TI MSPM0 DriverLib、PowerShell 测试入口、CMake/arm-none-eabi-g++。

---

## 文件边界

- Create: `src/tracking_diagnostics.hpp`：低电平有效配置、前馈限制、定点 CSV 编码和环形发送队列。
- Modify: `src/car_control_support.hpp`：移除重复的灰度极性常量，避免配置出现两个来源。
- Modify: `src/car_control_support.cpp`：将原始前馈交给 `TrackingDiagnostics::LimitFeedForward()`。
- Modify: `src/app_main.cpp`：组装 20 Hz 遥测帧并使用 `UART_0_INST` 非阻塞排空。
- Modify: `tests/leader_mission_logic_test.cpp`：覆盖控制配置、前馈余量、CSV 内容和整帧丢弃。

当前工作区包含用户已有的暂存和未暂存改动。实施期间不得回退这些改动，也不得用普通 `git commit` 收入已有暂存内容；完成后先交付 diff 和验证结果，只有用户明确要求时才提交实现。

### Task 1: 用失败测试锁定灰度极性和前馈余量

**Files:**
- Create: `src/tracking_diagnostics.hpp`
- Modify: `src/car_control_support.hpp:31-37`
- Modify: `src/car_control_support.cpp:1-6,109-122`
- Test: `tests/leader_mission_logic_test.cpp`

- [ ] **Step 1: 写控制配置失败测试**

在测试文件 include 区加入：

```cpp
#include "src/tracking_diagnostics.hpp"
```

在匿名命名空间加入：

```cpp
void TestTrackingRuntimeConfigurationPreservesControlAuthority()
{
  Expect(TrackingDiagnostics::kGreySensorActiveLow,
         "installed pull-up grey sensors must treat low level as active");
  ExpectNear(TrackingDiagnostics::LimitFeedForward(1.60f), 0.65f, 1e-6f,
             "positive feedforward must leave PID output headroom");
  ExpectNear(TrackingDiagnostics::LimitFeedForward(-1.60f), -0.65f, 1e-6f,
             "negative feedforward must leave PID output headroom");
}
```

在 `main()` 的测试调用列表加入该函数。

- [ ] **Step 2: 运行测试并确认 RED**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
```

Expected: 编译失败，提示找不到 `src/tracking_diagnostics.hpp`。这是预期失败，证明测试依赖尚未实现的运行配置。

- [ ] **Step 3: 写最小控制配置实现**

创建 `src/tracking_diagnostics.hpp` 的第一版：

```cpp
#pragma once

namespace TrackingDiagnostics
{

constexpr bool kGreySensorActiveLow = true;
constexpr float kFeedForwardLimit = 0.65f;

constexpr float LimitFeedForward(float output)
{
  return output > kFeedForwardLimit
             ? kFeedForwardLimit
             : (output < -kFeedForwardLimit ? -kFeedForwardLimit : output);
}

}  // namespace TrackingDiagnostics
```

从 `src/car_control_support.hpp` 删除旧的：

```cpp
constexpr bool kGreySensorActiveLow = false;
```

在 `src/car_control_support.cpp` include 区加入：

```cpp
#include "tracking_diagnostics.hpp"
```

并把前馈返回值改为：

```cpp
const float output = config.static_output + config.velocity_gain * abs_target;
return direction * TrackingDiagnostics::LimitFeedForward(output);
```

- [ ] **Step 4: 运行测试并确认 GREEN**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
```

Expected: `leader mission logic tests passed.`

### Task 2: 用失败测试实现定点 CSV 和整帧发送队列

**Files:**
- Modify: `src/tracking_diagnostics.hpp`
- Test: `tests/leader_mission_logic_test.cpp`

- [ ] **Step 1: 写 CSV 与队列失败测试**

在测试文件加入：

```cpp
void TestTrackingTelemetryEncodesCompleteCsvFrame()
{
  TrackingDiagnostics::Frame frame{};
  frame.time_ms = 1250U;
  frame.raw_mask = 0xA5U;
  frame.active_mask = 0x18U;
  frame.changed_mask = 0x81U;
  frame.position = -500;
  frame.lost_side = 1U;
  frame.lost_count = 2U;
  frame.error_milli = -750;
  frame.left_target_centi = 0;
  frame.right_target_centi = 547;
  frame.target_centi = {0, 547, 0, 547};
  frame.measured_centi = {12, 500, 10, 490};
  frame.output_milli = {0, 800, 0, -790};
  frame.source_sequence = 41U;
  frame.tracking_sequence = 40U;
  frame.ready_flags = 0x0FU;

  TrackingDiagnostics::CsvQueue<512U> queue;
  Expect(queue.Push(frame), "a telemetry frame must fit in the normal queue");

  std::string actual;
  while (queue.HasData())
  {
    actual.push_back(static_cast<char>(queue.Front()));
    queue.Pop();
  }

  const std::string expected =
      "TC,1250,165,24,129,-500,1,2,-750,0,547,0,547,0,547,"
      "12,500,10,490,0,800,0,-790,41,40,15,0\r\n";
  Expect(actual == expected, "telemetry CSV fields must remain stable");
}

void TestTrackingTelemetryDropsOnlyWholeFrames()
{
  TrackingDiagnostics::Frame frame{};
  TrackingDiagnostics::CsvQueue<32U> queue;
  Expect(!queue.Push(frame), "an undersized queue must reject the whole frame");
  Expect(!queue.HasData(), "a rejected frame must not leave partial CSV bytes");
  Expect(queue.DroppedFrames() == 1U,
         "a rejected frame must increment the dropped frame counter");
}
```

在 `main()` 调用两个新测试。

- [ ] **Step 2: 运行测试并确认 RED**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
```

Expected: 编译失败，提示 `Frame` 或 `CsvQueue` 尚未定义。

- [ ] **Step 3: 实现定点帧和环形队列**

在 `tracking_diagnostics.hpp` 增加 `<array>`, `<cstddef>`, `<cstdint>`，定义：

```cpp
struct Frame
{
  uint32_t time_ms = 0;
  uint8_t raw_mask = 0;
  uint8_t active_mask = 0;
  uint8_t changed_mask = 0;
  int16_t position = 0;
  uint8_t lost_side = 0;
  uint32_t lost_count = 0;
  int32_t error_milli = 0;
  int32_t left_target_centi = 0;
  int32_t right_target_centi = 0;
  std::array<int32_t, 4> target_centi{};
  std::array<int32_t, 4> measured_centi{};
  std::array<int32_t, 4> output_milli{};
  uint32_t source_sequence = 0;
  uint32_t tracking_sequence = 0;
  uint8_t ready_flags = 0;
};

constexpr int32_t Scale(float value, float scale)
{
  return static_cast<int32_t>(value * scale);
}
```

实现 `CsvQueue<Capacity>`：

- 内部使用 `std::array<uint8_t, Capacity>`、`head_`、`tail_`、`size_`。
- `Push()` 先在局部 `std::array<char, 256>` 完整编码，再检查剩余容量。
- 编码字段顺序严格匹配规格和测试字符串，最后追加 `dropped_frames_` 与 `\r\n`。
- 容量不足或编码超过 256 字节时只递增 `dropped_frames_`，不得写入任何字节。
- `Front()` 返回队首字节，`Pop()` 移除一个字节，`HasData()` 和 `DroppedFrames()` 为只读查询。
- 数字编码使用私有 `AppendUnsigned()` / `AppendSigned()`，不得使用动态内存或浮点 `printf`。

- [ ] **Step 4: 运行测试并确认 GREEN**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
```

Expected: `leader mission logic tests passed.`

### Task 3: 接入 20 Hz 非阻塞 UART 遥测

**Files:**
- Modify: `src/app_main.cpp:1-20,210-250,264-550`
- Modify: `src/tracking_diagnostics.hpp`
- Test: `tests/leader_mission_logic_test.cpp`

- [ ] **Step 1: 写限频行为失败测试**

在 `tracking_diagnostics.hpp` 的期望 API 基础上，先在测试加入：

```cpp
void TestTrackingTelemetryUsesWrapSafeFiftyMillisecondPeriod()
{
  Expect(!TrackingDiagnostics::IsSampleDue(1049U, 1000U),
         "telemetry must not sample before 50 ms");
  Expect(TrackingDiagnostics::IsSampleDue(1050U, 1000U),
         "telemetry must sample at 50 ms");
  Expect(TrackingDiagnostics::IsSampleDue(40U, 0xFFFFFFF0U),
         "telemetry period arithmetic must survive uint32 wraparound");
}
```

- [ ] **Step 2: 运行测试并确认 RED**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
```

Expected: 编译失败，提示 `IsSampleDue` 尚未定义。

- [ ] **Step 3: 实现限频函数并确认单测 GREEN**

在 `tracking_diagnostics.hpp` 增加：

```cpp
constexpr uint32_t kSamplePeriodMs = 50U;

constexpr bool IsSampleDue(uint32_t now_ms, uint32_t last_sample_ms)
{
  return static_cast<uint32_t>(now_ms - last_sample_ms) >= kSamplePeriodMs;
}
```

再次运行 leader mission 测试，Expected: PASS。

- [ ] **Step 4: 在应用层组装遥测帧**

在 `src/app_main.cpp` include 区加入：

```cpp
#include "tracking_diagnostics.hpp"
```

在局部状态中增加：

```cpp
TrackingDiagnostics::CsvQueue<512U> tracking_telemetry;
uint32_t last_telemetry_time_ms = 0U;
bool grey_topic_fresh = false;
bool tracking_topic_fresh = false;
bool encoder_topic_fresh = false;
```

把控制周期内三个 freshness 局部变量改为对上述状态变量赋值。每次控制计算完成后，若 `IsSampleDue(now, last_telemetry_time_ms)`，组装 `Frame`：

```cpp
TrackingDiagnostics::Frame frame{};
frame.time_ms = now;
frame.raw_mask = grey_sensor_sample.raw_mask;
frame.active_mask = grey_sensor_sample.active_mask;
frame.changed_mask = grey_sensor_sample.changed_mask;
frame.position = grey_sensor_sample.position;
frame.lost_side = grey_sensor_sample.lost_side;
frame.lost_count = grey_sensor_sample.lost_count;
frame.error_milli = TrackingDiagnostics::Scale(tracking_output.error, 1000.0f);
frame.left_target_centi =
    TrackingDiagnostics::Scale(tracking_output.left_speed_rad_s, 100.0f);
frame.right_target_centi =
    TrackingDiagnostics::Scale(tracking_output.right_speed_rad_s, 100.0f);
for (size_t i = 0; i < Module::MotorGroup::kMotorCount; ++i)
{
  frame.target_centi[i] =
      TrackingDiagnostics::Scale(car_control_sample.target_speed[i], 100.0f);
  frame.measured_centi[i] =
      TrackingDiagnostics::Scale(car_control_sample.measured_speed[i], 100.0f);
  frame.output_milli[i] =
      TrackingDiagnostics::Scale(car_control_sample.motor_output[i], 1000.0f);
}
frame.source_sequence = tracking_output.source_sequence;
frame.tracking_sequence = tracking_output.sequence;
frame.ready_flags = static_cast<uint8_t>((grey_topic_fresh ? 1U : 0U) |
    (tracking_topic_fresh ? 2U : 0U) |
    (encoder_topic_fresh ? 4U : 0U) |
    (car_control_sample.line_lost ? 8U : 0U));
(void)tracking_telemetry.Push(frame);
last_telemetry_time_ms = now;
```

初始化模块配置时使用唯一极性来源：

```cpp
config.grey_active_low = TrackingDiagnostics::kGreySensorActiveLow;
car_control_sample.grey_sensor_active_low =
    TrackingDiagnostics::kGreySensorActiveLow;
```

- [ ] **Step 5: 非阻塞排空 UART FIFO**

在每轮主循环末尾、`Sleep(1)` 之前加入：

```cpp
while (tracking_telemetry.HasData() &&
       !DL_UART_Main_isTXFIFOFull(UART_0_INST))
{
  DL_UART_Main_transmitData(UART_0_INST, tracking_telemetry.Front());
  tracking_telemetry.Pop();
}
```

禁止使用 `DL_UART_Main_transmitDataBlocking`，禁止等待发送完成。

- [ ] **Step 6: 运行主机测试和固件构建**

Run:

```powershell
& 'tests/run_leader_mission_logic_tests.ps1'
& 'tests/verify_xrobot_module_management.ps1'
cmake --build build
```

Expected:

- leader mission tests passed
- XRobot module management verification passed
- 固件链接成功并生成 `.elf/.hex/.bin`
- CMake 的 `CheckFirmwareSize.cmake` 后置检查确认 Flash 使用量不超过 `128450` 字节

- [ ] **Step 7: 检查实现差异和阻塞 API**

Run:

```powershell
git diff --check
rg -n "UART_0_INST|isTXFIFOFull|transmitDataBlocking|tracking_telemetry" src
git diff -- src/car_control_support.hpp src/car_control_support.cpp `
  src/tracking_diagnostics.hpp src/app_main.cpp tests/leader_mission_logic_test.cpp
```

Expected: `transmitDataBlocking` 在 `src/` 中无匹配；差异仅包含本计划范围和工作区原有相关改动。

### Task 4: 交付实车采集说明

**Files:**
- No production changes

- [ ] **Step 1: 报告串口参数和字段**

交付说明必须包含：`115200 8N1`、每行 `TC,` 开头、20 Hz、定点缩放规则、`ready_flags` 位定义。

- [ ] **Step 2: 请求两组实车数据**

请求用户分别保存：

- 一次完整正常循迹，从按下任务键前 1 秒到停车后 1 秒。
- 一次发生无法循迹或转向失败的完整记录，保留失败前至少 3 秒和失败后至少 2 秒。

- [ ] **Step 3: 标记硬件验证边界**

若主机测试和固件构建全部通过但尚未上车运行，最终状态使用 `DONE_WITH_CONCERNS`，明确说明物理转向稳定性仍需 UART 实车数据验证，不得声称实车问题已经完全消失。
