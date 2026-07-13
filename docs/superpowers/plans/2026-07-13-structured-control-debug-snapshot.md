# 结构化控制调试快照 Implementation Plan

> **面向执行代理：** 实施时必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，按任务逐项执行。步骤使用复选框跟踪。

**目标：** 用五个结构化全局对象替代散落的控制调试镜像，并保持 Ozone 可观测性和车辆控制行为。

**架构：** 四个模块已有的完整数据结构直接作为 `app_main.cpp` 文件作用域快照；没有现成类型的控制环状态进入 `CarControlSupport::CarControlSample`。GreySensor 通过独立 Topic 订阅更新，控制逻辑继续消费原有 Tracking 和 Encoder Topic。

**技术栈：** C++20、LibXR Application/Topic、MSPM0G3507、PowerShell 结构测试、CMake/Ninja/arm-none-eabi GCC。

---

### 任务 1：锁定新的结构契约

**文件：**

- 修改：`tests/verify_xrobot_module_management.ps1`

- [ ] **步骤 1：增加失败断言**

断言 `app_main.cpp` 包含以下对象和 GreySensor 订阅：

```powershell
Assert-Contains $appMain 'GreySensor::Sample grey_sensor_sample' `
  'app_main must expose the complete GreySensor sample for Ozone.'
Assert-Contains $appMain 'CarControlSample car_control_sample' `
  'app_main must expose one aggregate control sample for Ozone.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<GreySensor::Sample>' `
  'app_main must receive complete GreySensor samples through its topic.'
Assert-NotMatches $carControlHeader '\bextern\s+volatile\s+[^;]+\bg_[A-Za-z0-9_]+' `
  'car control support must not expose field-by-field volatile debug mirrors.'
```

- [ ] **步骤 2：运行测试并确认失败**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

预期：在缺少 `grey_sensor_sample` 或 `CarControlSample` 的新断言处失败。

### 任务 2：定义控制快照并移除旧镜像

**文件：**

- 修改：`src/car_control_support.hpp`
- 修改：`src/car_control_support.cpp`

- [ ] **步骤 1：定义 `CarControlSample`**

在 `DriveMode` 后增加：

```cpp
struct CarControlSample
{
  uint32_t control_time_ms = 0;
  uint32_t elapsed_ms = 0;
  float dt_s = 0.0f;
  DriveMode drive_mode = DriveMode::kIdle;
  bool encoder_topic_ready = false;
  bool tracking_topic_ready = false;
  bool grey_sensor_active_low = false;
  bool line_lost = false;
  float forward_distance_m = 0.0f;
  std::array<float, Module::MotorGroup::kMotorCount> target_speed{};
  std::array<float, Module::MotorGroup::kMotorCount> measured_speed{};
  std::array<float, Module::MotorGroup::kMotorCount> feedforward_output{};
  std::array<float, Module::MotorGroup::kMotorCount> pid_output{};
  std::array<float, Module::MotorGroup::kMotorCount> motor_output{};
};
```

- [ ] **步骤 2：删除纯调试镜像**

从头文件和源文件删除 `g_elapsed_ms`、`g_dt_s`、`jie`、编码器镜像、循迹镜像、模式/按键/距离镜像和五组四轮控制镜像。保留 `speed_pid_config`、`speed_feedforward_config`、`speed_pid`。

### 任务 3：迁移主循环到完整快照

**文件：**

- 修改：`src/app_main.cpp`

- [ ] **步骤 1：定义五个全局对象**

```cpp
GreySensor::Sample grey_sensor_sample{};
Tracking::Output tracking_output{};
Module::Encoder::Sample encoder_sample{};
BitsButtonXR::ButtonEventResult button_event{};
CarControlSample car_control_sample{};
```

- [ ] **步骤 2：增加 GreySensor 订阅**

```cpp
LibXR::Topic::ASyncSubscriber<GreySensor::Sample> grey_sensor_subscriber(
    config.grey_topic_name);
grey_sensor_subscriber.StartWaiting();
```

在 `MonitorAll()` 后非阻塞更新并重新等待。

- [ ] **步骤 3：迁移控制状态**

保留局部 `drive_mode` 作为状态机的唯一权威值，并单向镜像到 `car_control_sample.drive_mode`；用 `car_control_sample.forward_distance_m` 替代原距离全局。在控制周期内更新 Topic 状态、综合失线状态和五组四轮数组。保留局部 `button_index`、`target`、`measured`、`feedforward`、`pid_correction` 与 `motor_output`，它们用于单次迭代计算。

- [ ] **步骤 4：运行结构测试并确认通过**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

预期：输出成功并返回退出码 0。

### 任务 4：回归验证与知识同步

**文件：**

- 修改：`.helloagents/modules/xrobot_modules.md`
- 更新：`.helloagents/CHANGELOG.md`（由方案包归档流程处理）

- [ ] **步骤 1：运行 Topic 逻辑测试**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_topic_module_logic_tests.ps1
```

预期：全部测试通过。

- [ ] **步骤 2：运行固件构建与 Flash 检查**

使用工程现有 CMake 构建目录重新构建固件，再运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_flash_budget.ps1
```

预期：固件链接成功且 `.bin` 不超过 128450 字节。

- [ ] **步骤 3：检查旧符号清理**

```powershell
rg -n "g_(line|tracking|grey_sensor|drive_mode|last_button|forward_distance|encoder|target_speed|measured_speed|feedforward_output|pid_output|motor_output|elapsed_ms|dt_s)" src tests
```

预期：没有旧调试镜像定义或使用。

- [ ] **步骤 4：同步模块文档并更新任务状态**

记录五个结构化调试对象、GreySensor 订阅和 Ozone 观察路径；完成 HelloAGENTS 方案包验证与归档。
