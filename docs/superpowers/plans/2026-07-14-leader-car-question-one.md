# H 题领头车第 1 问实施计划

> **面向执行代理：** 实施时必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，按任务逐项执行。步骤使用复选框跟踪。

**目标：** K1 按下后，领头车以 0.3 m/s 沿外圈循迹一圈，在再次检测到 A 点横向黑色标志时主动制动，随后蜂鸣 1 秒。

**架构：** 保留 GreySensor、Tracking、Encoder、TB6612 和现有四轮 PI。新增可主机测试的连续线簇选线、横线事件和第 1 问状态机；`app_main.cpp` 只负责 Topic 快照、按键和 5 ms 控制编排。GreySensor 以 2 ms 采样横线，事件锁存后由 5 ms 控制周期消费。

**技术栈：** C++20、LibXR Application/Topic、MSPM0G3507、TB6612、PowerShell + g++ 主机测试、CMake/Ninja、arm-none-eabi GCC。

**规格依据：** `docs/superpowers/specs/2026-07-14-leader-car-h-problem-control-system-design.md` 的“当前实施里程碑”。

---

## 文件职责

- 新建 `module/tracking/tracking_logic.hpp`：8 bit 连续线簇提取和外圈选线。
- 修改 `module/tracking/Tracking.hpp/.cpp`：动态接收普通/外圈选线模式，继续输出四轮目标速度。
- 新建 `src/leader_track_event_logic.hpp`：A 点横线去抖、锁存和距离再武装。
- 新建 `src/leader_question_one_logic.hpp`：K1 任务状态、B 点窗口、最小圈长、停止和蜂鸣时序。
- 修改 `src/car_control_support.hpp/.cpp`：删除旧 `DriveMode` 演示逻辑，记录第 1 问调试快照。
- 修改 `src/app_main.cpp`、`xrobot_main.hpp`：K1/K3、2 ms 事件、5 ms 控制、BrakeAll 和蜂鸣器接线。
- 新建 2 组主机测试并更新现有结构测试和 HelloAGENTS 知识库。

### 任务 1：锁定第 1 问应用契约

**文件：**

- 修改：`tests/verify_xrobot_module_management.ps1`

- [ ] **步骤 1：写失败断言**

在现有应用断言区增加：

```powershell
$questionOneHeader = Join-Path $root 'src/leader_question_one_logic.hpp'
$trackEventHeader = Join-Path $root 'src/leader_track_event_logic.hpp'
if (-not (Test-Path -LiteralPath $questionOneHeader)) {
  throw 'Missing src/leader_question_one_logic.hpp.'
}
if (-not (Test-Path -LiteralPath $trackEventHeader)) {
  throw 'Missing src/leader_track_event_logic.hpp.'
}
Assert-Contains $xrobotHeader 'grey_publish_period_ms = 2U;' `
  'GreySensor must publish every 2 ms.'
Assert-Contains $appMain 'question_one.Start(now' `
  'K1 must directly start question one.'
Assert-Contains $appMain 'motors.BrakeAll();' `
  'finish and K3 must use active braking.'
Assert-Contains $appMain 'buzzer.Write(question_one_output.buzzer_on);' `
  'the buzzer must follow the non-blocking mission output.'
Assert-NotContains $appMain 'kForwardDistanceTargetM' `
  'the old 50 m demo must be removed.'
Assert-NotContains $appMain 'kSpinInPlace' `
  'the old spin demo must be removed.'
Assert-NotMatches $appMain 'now\s*-\s*last_blink_time[\s\S]*buzzer\.Write\(true\)' `
  'the heartbeat must not drive the buzzer.'
```

- [ ] **步骤 2：运行并确认红灯**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

预期：首先因缺少 `leader_question_one_logic.hpp` 失败。

- [ ] **步骤 3：提交契约测试**

```powershell
git add -- tests/verify_xrobot_module_management.ps1
git commit --only -m 'test: 锁定领头车第一问接线 / lock leader question-one wiring' -- tests/verify_xrobot_module_management.ps1
```

### 任务 2：实现外圈连续线簇选线

**文件：**

- 新建：`module/tracking/tracking_logic.hpp`
- 新建：`tests/tracking_logic_test.cpp`
- 新建：`tests/run_tracking_logic_tests.ps1`
- 修改：`module/tracking/Tracking.hpp`
- 修改：`module/tracking/Tracking.cpp`
- 修改：`tests/verify_topic_module_structure.ps1`

- [ ] **步骤 1：写线簇失败测试**

```cpp
#include <cstdlib>
#include <iostream>
#include "module/tracking/tracking_logic.hpp"

void Expect(bool value, const char* message)
{
  if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}

int main()
{
  using namespace Module::TrackingLogic;
  const auto split = Analyze(0xC3U, -2.8f, SelectionMode::kContinuity, 6U);
  Expect(split.cluster_count == 2U, "0xC3 must form two clusters");
  Expect(split.selected_center < -2.0f, "continuity must keep the prior cluster");
  Expect(Analyze(0xC3U, 0.0f, SelectionMode::kOuterBranch, 6U).selected_center > 2.0f,
         "outer mode must choose the straight/right cluster");
  Expect(Analyze(0xFFU, 0.0f, SelectionMode::kContinuity, 6U).transverse_candidate,
         "A marker must be a transverse candidate");
  Expect(!Analyze(0x00U, 0.0f, SelectionMode::kContinuity, 6U).line_detected,
         "empty mask must report no line");
  std::cout << "tracking logic tests passed.\n";
}
```

PowerShell 脚本使用 `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I $root` 编译并运行。

- [ ] **步骤 2：运行并确认红灯**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_tracking_logic_tests.ps1
```

预期：缺少 `tracking_logic.hpp`，编译失败。

- [ ] **步骤 3：实现线簇 API**

```cpp
namespace Module::TrackingLogic
{
enum class SelectionMode : uint8_t { kContinuity, kOuterBranch };
struct Cluster { uint8_t mask{}; uint8_t first{}; uint8_t last{}; uint8_t count{}; float center{}; };
struct Analysis
{
  std::array<Cluster, 4> clusters{}; uint8_t cluster_count{};
  int8_t selected_index{-1}; uint8_t selected_mask{}; float selected_center{};
  bool line_detected{}; bool transverse_candidate{};
};
inline Analysis Analyze(uint8_t mask, float previous_center,
                        SelectionMode mode, uint8_t transverse_min_active);
}
```

按 bit0 到 bit7 提取连续置位段，权重保持 `{3.5,2.5,1.5,0.5,-0.5,-1.5,-2.5,-3.5}`。普通模式最小化 `abs(center-previous_center)`；外圈模式选择最大 `center`；总有效通道数达到阈值时设置横线候选。

- [ ] **步骤 4：让 Tracking 使用选中线簇**

增加 `SetSelectionMode()` 和 `SetBaseSpeedRadS()`；`Output` 增加 `selected_mask`、`cluster_count` 和 `transverse_candidate`。`Calculate()` 对选中线簇计算误差，保留现有 PD、丢线和四轮目标输出。比赛配置把 `base_speed_rad_s` 设为 9.375、`max_speed_rad_s` 设为 14.0。

同步结构测试，要求 Tracking 调用 `TrackingLogic::Analyze()`，并保留 `wheel_speed_rad_s` 输出供现有速度环消费。

- [ ] **步骤 5：运行绿灯并提交**

运行新测试、`verify_topic_module_structure.ps1` 和 `run_topic_module_logic_tests.ps1`；全部通过后只提交本任务列出的文件，使用双语 `feat:` 信息。

### 任务 3：实现 A 点事件和第 1 问状态机

**文件：**

- 新建：`src/leader_track_event_logic.hpp`
- 新建：`src/leader_question_one_logic.hpp`
- 新建：`tests/leader_question_one_logic_test.cpp`
- 新建：`tests/run_leader_question_one_logic_tests.ps1`

- [ ] **步骤 1：写失败测试**

测试覆盖：横线连续 2 帧才触发；保持黑线不重复；0.15 m 内不重新武装；K1 后目标为 9.375 rad/s；起点横线和小于 4.8 m 的横线不停车；大于 4.8 m 的横线进入制动；完成后蜂鸣恰好 1000 ms；持续丢线 30 ms、灰度或编码器超时进入故障；K3 立即制动。

- [ ] **步骤 2：运行并确认红灯**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_leader_question_one_logic_tests.ps1
```

- [ ] **步骤 3：实现横线检测器**

```cpp
namespace LeaderTrackEvent
{
struct Config { uint8_t min_active{6U}; uint8_t confirm{2U}; uint8_t release{2U}; float rearm_m{0.15f}; };
class CrossLineDetector
{
 public:
  explicit CrossLineDetector(Config config = {}); bool Update(uint8_t mask, float distance_m); void Reset();
 private:
  Config config_{}; uint8_t confirm_count_{}; uint8_t release_count_{};
  float event_distance_m_{}; bool latched_{};
};
}
```

只在确认计数首次达到阈值时返回 `true`；锁存后必须满足释放帧数和再武装距离。

- [ ] **步骤 4：实现第 1 问控制器**

```cpp
namespace LeaderQuestionOne
{
enum class State : uint8_t { kIdle, kLaunch, kRunning, kFinished, kFaultStop };
enum class DriveAction : uint8_t { kCoast, kDrive, kBrake };
struct Input
{
  uint32_t now_ms{}; float distance_m{}; bool cross_line{}; bool line_lost{};
  bool grey_fresh{}; bool encoder_fresh{};
};
struct Output
{
  State state{}; DriveAction drive{}; Module::TrackingLogic::SelectionMode selection{};
  float target_wheel_rad_s{}; bool buzzer_on{};
};
class Controller
{
 public:
  void Start(uint32_t now_ms, float distance_m); void EmergencyStop(); void Reset();
  Output Update(const Input& input); const Output& Latest() const;
};
}
```

固定参数：目标 9.375 rad/s、起步屏蔽 0.15 m、B 点外圈窗口 0.15-0.55 m、最小圈长 4.8 m、丢线故障 30 ms、蜂鸣 1000 ms。K1 只在非运行状态调用 `Start()`；运行中重复 K1 不重置里程。

- [ ] **步骤 5：运行绿灯并提交**

预期输出 `leader question-one logic tests passed.`；提交两个生产头、测试和脚本。

### 任务 4：接入 K1、主动制动和蜂鸣器

**文件：**

- 修改：`xrobot_main.hpp`
- 修改：`src/car_control_support.hpp`
- 修改：`src/car_control_support.cpp`
- 修改：`src/control_topic_logic.hpp`
- 修改：`src/app_main.cpp`
- 修改：`tests/topic_module_logic_test.cpp`
- 修改：`tests/verify_xrobot_module_management.ps1`

- [ ] **步骤 1：更新采样和控制配置**

把 `grey_publish_period_ms` 改为 2U，Encoder 保持 5U；Tracking 基础轮速设为 9.375 rad/s、上限 14.0 rad/s。`1024 counts/rev` 暂作台架值，任务 6 必须以实测值替换后才能正式验收。

- [ ] **步骤 2：删除原按键和 DriveMode 逻辑**

删除 K1 直行 50 m、K2 普通循迹、K4 原地旋转、`DriveMode`、`ResolveTargetSpeed()`、`GateTarget()`、`ShouldRunSpeedController()` 及对应旧测试。新映射：K1 在非运行状态复位 Encoder、Tracking、横线检测器和 PID 后启动第 1 问；K3 立即 `EmergencyStop()`、复位 PID 并 `BrakeAll()`；K2/K4 的 `PRESSED` 事件只消费，不执行动作。

- [ ] **步骤 3：在 2 ms 数据路径锁存横线事件**

在每次取得新的 GreySensor sample 时更新时间戳，并立即执行：

```cpp
pending_cross_line |= cross_line_detector.Update(
    grey_sensor_sample.active_mask, CalculateForwardDistanceM(encoder_sample));
```

5 ms 控制周期复制 `pending_cross_line` 后立即清零，确保不会因为控制周期较慢漏掉 2 cm 横线。

- [ ] **步骤 4：接入任务输出和四轮 PI**

```cpp
question_one_output = question_one.Update({
    now, distance_m, cross_line, tracking_output.lost_line,
    grey_topic_fresh, encoder_topic_fresh});
tracking.SetSelectionMode(question_one_output.selection);
tracking.SetBaseSpeedRadS(question_one_output.target_wheel_rad_s);
```

`kDrive` 使用 `tracking_output.wheel_speed_rad_s` 执行现有四轮前馈 + PI；`kBrake` 复位 PID 并 `BrakeAll()`；`kCoast` 使用 `CoastAll()`。灰度和编码器新鲜度阈值均改为 20 ms。

- [ ] **步骤 5：接入一次蜂鸣并保持 LED 心跳独立**

每轮主循环执行 `buzzer.Write(question_one_output.buzzer_on)`；删除 LED 心跳块中的 `buzzer.Write(true)`。LED 可继续作为主循环心跳，但不能影响蜂鸣器。

- [ ] **步骤 6：更新 Ozone 快照和结构断言**

新增 `LeaderQuestionOne::Output question_one_output{}` 全局快照；`CarControlSample` 增加任务状态、选线模式和横线事件，删除旧 `DriveMode` 字段。更新结构测试中的旧模式断言，并确保 `app_main.cpp` 尽量保持在 500 行以内，必要时允许小幅超过但不得复制纯逻辑算法。

- [ ] **步骤 7：运行结构测试、逻辑回归和 ARM 构建**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_topic_module_logic_tests.ps1
cmake --build build --target ti_mspm0_libxr_dev.elf
```

预期：全部退出 0；随后只提交本任务文件，使用双语 `feat:` 信息。

### 任务 5：建立聚合回归和知识同步

**文件：**

- 新建：`tests/run_leader_question_one_tests.ps1`
- 修改：`.helloagents/modules/xrobot_modules.md`
- 修改：`.helloagents/CHANGELOG.md`

- [ ] **步骤 1：创建聚合脚本**

依次运行 Tracking 测试、第 1 问逻辑测试、Topic 回归、两个结构测试、ARM 构建和 `verify_flash_budget.ps1`。任一步失败立即返回相同退出码；全部通过输出 `leader question-one automated checks passed.`。

- [ ] **步骤 2：运行完整自动验证**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_leader_question_one_tests.ps1
```

预期：所有逻辑、结构、构建和 Flash 检查通过。

- [ ] **步骤 3：同步知识库并提交**

记录 K1/K3 映射、删除的演示模式、2 ms 横线事件、5 ms 控制、外圈 B 点选线、A 点最小圈长、BrakeAll 和 1 秒蜂鸣；用双语 `docs:` 提交聚合脚本与知识库变更。

### 任务 6：完成实车标定和第 1 问验收

**文件：**

- 修改：`xrobot_main.hpp`
- 修改：`src/car_control_support.cpp`
- 新建：`docs/leader-car-question-one-results.md`

- [ ] **步骤 1：标定编码器和轮径**

每轮手动旋转 10 圈并重复 3 次；重复误差小于 0.5%、四轮均值差不超过 1% 后，把共同均值写入 `config.encoder.counts_per_rev`。再以低速直行 2 m，按 `new_radius=old_radius*measured_distance/encoder_distance` 修正有效轮径，复测误差必须小于 1%。

- [ ] **步骤 2：标定 0.3 m/s 前馈和 PI**

目标轮速固定为 9.375 rad/s，分别记录四轮稳态占空比并更新现有前馈参数；调 PI 后每轮稳态误差小于 5%，整车实测平均速度保持 0.27-0.33 m/s。

- [ ] **步骤 3：标定 B 点和 A 点识别**

记录经过普通 1 cm 引导线、B 点分岔和 A 点横线时的 `active_mask`。确认外圈连续通过 B 点 20 次；调整 `min_active` 和 4.8 m 最小圈长窗口，使起点和途中宽线零误停、返回 A 点可靠触发。

- [ ] **步骤 4：执行连续验收**

连续运行 10 次：K1 启动、外圈一圈、A 点 `BrakeAll()`、蜂鸣器仅响一次且持续约 1 秒。任何丢线、提前停车、漏停或重复蜂鸣都使计数清零重测。

- [ ] **步骤 5：记录、验证并提交**

结果文档记录三次原始编码器数据、轮径、前馈、PI、10 次圈速和失败原因。重新运行聚合脚本；自动验证通过后，用双语 `test:` 提交实测参数和验收记录。
