# app_main 初始化迁移实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `CarApplication` 构造函数和 `g_control_config` 定义移入 `src/app_main.cpp`，且不改变初始化顺序、参数值或运行行为。

**Architecture:** 本次只改变定义所属的翻译单元，公开声明和所有使用方保持不变。先更新结构测试，使新的定义归属成为可执行契约，再逐字迁移两项定义并重新构建固件，以发现编译或链接回归。

**Tech Stack:** C++20、C11、CMake 3.20+、Ninja、ARM GNU Toolchain、PowerShell 结构测试、LibXR/MSPM0。

---

## 文件映射

- 修改 `task/tests/verify_refactor.ps1`：用定义位置和重复定义断言替换已过时的极薄入口断言。
- 修改 `src/app_main.cpp`：承载 `g_control_config` 和 `CarApplication::CarApplication()`，并保留 `app_main()`。
- 修改 `task/application/car_application.cpp`：保留运行期方法，不再定义构造函数。
- 修改 `task/config/car_config.cpp`：保留 `MakeTrackingConfig()`，不再定义 `g_control_config`。
- 参考 `task/application/car_application.hpp`：保持构造函数声明和成员顺序不变。
- 参考 `task/config/car_config.hpp`：保持 `extern "C"` 声明不变。

## 工作区约束

目标 `task/` 源文件和 `src/app_main.cpp` 已包含未提交工作，在干净 worktree 中并不存在。应在当前工作区执行本计划，不得从 `HEAD` 重置或替换这些文件，并且只修改任务中列出的四个目标路径。

### 任务 1：编码并实现定义归属变更

**文件：**
- 修改：`task/tests/verify_refactor.ps1:31`
- 修改：`src/app_main.cpp:1`
- 修改：`task/application/car_application.cpp:12`
- 修改：`task/config/car_config.cpp:3`
- 参考：`task/application/car_application.hpp:19`
- 参考：`task/config/car_config.hpp:101`

- [ ] **步骤 1：用新的结构契约替换极薄入口断言**

在 `task/tests/verify_refactor.ps1` 中，将当前 `$appMainPath` 开始至旧辅助函数断言结束的代码替换为以下完整代码块：

```powershell
$appMainPath = Join-Path $repoRoot 'src/app_main.cpp'
$carApplicationPath =
    Join-Path $repoRoot 'task/application/car_application.cpp'
$carConfigPath = Join-Path $repoRoot 'task/config/car_config.cpp'
$appMain = Get-Content -Raw -Encoding UTF8 $appMainPath
$carApplication = Get-Content -Raw -Encoding UTF8 $carApplicationPath
$carConfig = Get-Content -Raw -Encoding UTF8 $carConfigPath

if ($appMain -notmatch 'App::CarApplication application') {
  $errors.Add('src/app_main.cpp does not construct App::CarApplication')
}
if ($appMain -notmatch 'CarApplication::CarApplication\(\)') {
  $errors.Add('src/app_main.cpp does not define CarApplication constructor')
}
if ($appMain -notmatch 'App::ControlConfig\s+g_control_config\s*=') {
  $errors.Add('src/app_main.cpp does not define g_control_config')
}
if ($carApplication -match 'CarApplication::CarApplication\(\)') {
  $errors.Add(
      'task/application/car_application.cpp still defines CarApplication constructor')
}
if ($carConfig -match 'App::ControlConfig\s+g_control_config\s*=') {
  $errors.Add('task/config/car_config.cpp still defines g_control_config')
}
if ($appMain -match 'SyncMpu6050Sample|SyncMpu6050Health') {
  $errors.Add('legacy MPU6050 sync helper remains in src/app_main.cpp')
}
```

这里有意删除 `$appMainLines` 和 15 行限制，因为 `app_main.cpp` 将成为已批准的组合与初始化归属文件。

- [ ] **步骤 2：运行结构测试，确认实现前新契约会失败**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'task/tests/verify_refactor.ps1'
```

预期：退出码为 `1`；错误信息指出 `src/app_main.cpp` 缺少两项定义，并且两个原源文件仍包含这些定义。不应出现文件缺失或 CMake 子目录错误。

- [ ] **步骤 3：用已批准的组合根内容替换 `src/app_main.cpp`**

使用以下完整文件内容。配置值、成员初始化列表顺序和构造函数体调用顺序均从当前定义原样复制。

```cpp
#include "app_main.h"

#include "application/car_application.hpp"
#include "config/car_config.hpp"
#include "diagnostics/debug_state.hpp"
#include "thread.hpp"

extern "C" {
// 通过 C 链接导出，便于调试器或外部工具观察；前馈参数按引用读取，
// PID 参数在控制器构造时复制，运行期修改后需重建控制器才会生效。
App::ControlConfig g_control_config = {
    // 每个元素依次对应 FL、FR、BL、BR 的速度环 PID；输出上限为归一化驱动量。
    .speed_pid = {{
        {.k = 1.0f, .p = 0.25f, .i = 0.12f, .d = 0.0f,
         .i_limit = 0.4f, .out_limit = 0.6f, .cycle = false},
        {.k = 1.0f, .p = 0.24f, .i = 0.12f, .d = 0.0f,
         .i_limit = 0.4f, .out_limit = 0.6f, .cycle = false},
        {.k = 1.0f, .p = 0.24f, .i = 0.12f, .d = 0.0f,
         .i_limit = 0.4f, .out_limit = 0.6f, .cycle = false},
        {.k = 1.0f, .p = 0.24f, .i = 0.12f, .d = 0.0f,
         .i_limit = 0.4f, .out_limit = 0.6f, .cycle = false},
    }},
    .speed_feedforward = {{
        {.static_output = 0.18f, .velocity_gain = 0.075f},
        {.static_output = 0.18f, .velocity_gain = 0.075f},
        {.static_output = 0.18f, .velocity_gain = 0.075f},
        {.static_output = 0.18f, .velocity_gain = 0.075f},
    }},
    .straight_yaw_pid = {
        .k = 1.0f,
        .p = 0.12f,
        .i = 0.01f,
        .d = 0.02f,
        .i_limit = 20.0f,
        .out_limit = 5.0f,
        .cycle = false,
    },
};
}

namespace App
{

CarApplication::CarApplication()
    // 先注入板级硬件别名，再按按钮、灰度、IMU、编码器、电机和控制器依赖构造。
    : buttons_(
          board_.Hardware(), app_manager_,
          {{Config::kButtonAliases[0], false, Config::kButtonConstraints},
           {Config::kButtonAliases[1], false, Config::kButtonConstraints},
           {Config::kButtonAliases[2], false, Config::kButtonConstraints},
           {Config::kButtonAliases[3], false, Config::kButtonConstraints}},
          {}),
      grey_sensor_(
          board_.Hardware(), app_manager_,
          {Config::kGreySensorAliases[0], Config::kGreySensorAliases[1],
           Config::kGreySensorAliases[2], Config::kGreySensorAliases[3],
           Config::kGreySensorAliases[4], Config::kGreySensorAliases[5],
           Config::kGreySensorAliases[6], Config::kGreySensorAliases[7]},
          Config::kGreySensorActiveLow),
      tracking_(app_manager_, "grey_sensor", "tracking",
                Config::MakeTrackingConfig()),
      mpu6050_(board_.Hardware(), app_manager_),
      tracking_subscriber_("tracking"),
      mpu6050_subscriber_("mpu6050"),
      speed_controller_(encoder_, motors_),
      route_controller_(encoder_, motors_, tracking_, tracking_subscriber_,
                        speed_controller_)
{
  // GreySensor 可能覆盖 GPIO 默认配置，构造完成后恢复板级上拉输入。
  board_.ConfigureGreySensorInputs();
  g_app_debug.tracking.active_low = Config::kGreySensorActiveLow;
  // 异步订阅必须先进入等待；编码器先初始化，电机 PWM 再以 10000 为周期启动。
  tracking_subscriber_.StartWaiting();
  mpu6050_subscriber_.StartWaiting();
  encoder_.Init();
  motors_.Init(10000);
  // 无线初始化会立即发送首包，随后主循环按独立周期轮询。
  last_control_time_ = LibXR::Thread::GetTime();
  nrf24_service_.Initialize(last_control_time_);
  g_app_debug.radio = nrf24_service_.Snapshot();
}

}  // namespace App

extern "C" void app_main()
{
  App::CarApplication application;
  application.Run();
}
```

- [ ] **步骤 4：仅从 `car_application.cpp` 删除构造函数定义**

删除从 `CarApplication::CarApplication()` 开始、到 `[[noreturn]] void CarApplication::Run()` 之前结束的完整代码块。修改后文件开头必须准确为：

```cpp
#include "application/car_application.hpp"

#include <cstring>

#include "config/car_config.hpp"
#include "diagnostics/debug_state.hpp"
#include "thread.hpp"

namespace App
{

[[noreturn]] void CarApplication::Run()
```

保持从 `Run()` 到 `ResolveButtonIndex()` 的所有方法体不变。这三个非标准库头文件仍被这些方法使用，必须保留。

- [ ] **步骤 5：仅从 `car_config.cpp` 删除控制配置定义**

删除 `extern "C"` 定义块后，完整文件必须为：

```cpp
#include "config/car_config.hpp"

namespace App::Config
{

Tracking::Config MakeTrackingConfig()
{
  // 这些参数只供 Tracking 使用，与路线层的直线/圆弧速度常量分开维护。
  Tracking::Config config;
  config.base_speed_rad_s = 13.0f;
  config.max_speed_rad_s = 18.0f;
  config.search_speed_rad_s = 5.0f;
  config.turn_kp = 1.8f;
  config.turn_kd = 0.02f;
  config.lost_confirm_samples = 2;
  return config;
}

}  // namespace App::Config
```

- [ ] **步骤 6：运行结构测试，确认定义归属契约通过**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'task/tests/verify_refactor.ps1'
```

预期：退出码为 `0`，并包含以下输出：

```text
Refactor structure contract passed.
```

- [ ] **步骤 7：构建固件并验证编译、链接行为**

运行：

```powershell
cmake --build 'build'
```

预期：退出码为 `0`；Ninja 重新编译受影响的翻译单元并链接 `ti_mspm0_libxr_dev.elf`，不出现重复符号或未定义引用错误；构建后重新生成 `.hex` 和 `.bin`。

- [ ] **步骤 8：确认每个符号仅剩一个定义且参数未漂移**

运行：

```powershell
rg -n 'CarApplication::CarApplication\(\)|App::ControlConfig g_control_config' `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp'
git diff --check -- `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp' `
  'task/tests/verify_refactor.ps1'
```

预期：`rg` 恰好输出两项匹配，且均来自 `src/app_main.cpp`；`git diff --check` 退出码为 `0`。将迁移后的初始化器和构造函数代码块与步骤 3 至步骤 5 对照，每个数值和初始化调用都必须一致。

- [ ] **步骤 9：检查迁移改动边界并保留当前工作区状态**

目标 `task/` 文件属于尚未提交的更大范围重构，单独提交四个文件会产生无法从该提交独立构建的历史。因此本任务只检查改动边界，不创建代码提交：

```powershell
git diff -- `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp' `
  'task/tests/verify_refactor.ps1'
git diff --check -- `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp' `
  'task/tests/verify_refactor.ps1'
git status --short -- `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp' `
  'task/tests/verify_refactor.ps1'
```

预期：`git diff --check` 退出码为 `0`；状态输出只反映这四个目标文件原有的已修改或未跟踪状态。不得执行 `git add .`、重置或提交其他工作区内容。代码提交应在更大范围的 `task/` 重构具备完整提交边界后统一处理。

### 任务 2：最终验收审计

**文件：**
- 验证：`src/app_main.cpp`
- 验证：`task/application/car_application.cpp`
- 验证：`task/config/car_config.cpp`
- 验证：`task/tests/verify_refactor.ps1`
- 验证：`task/application/car_application.hpp`
- 验证：`task/config/car_config.hpp`

- [ ] **步骤 1：确认声明和成员顺序仍符合既有接口**

运行：

```powershell
rg -n 'CarApplication\(\);|BoardContext board_|ApplicationManager app_manager_|BitsButtonXR buttons_|GreySensor grey_sensor_|Tracking tracking_|MPU6050 mpu6050_|extern "C" App::ControlConfig g_control_config' `
  'task/application/car_application.hpp' `
  'task/config/car_config.hpp'
```

预期：输出仍包含构造函数声明、从 `board_` 到 `mpu6050_` 的既有成员声明顺序，以及 `extern "C" App::ControlConfig g_control_config` 声明。实现步骤不得编辑这两个头文件。

- [ ] **步骤 2：从最终工作区状态重新运行两项验收命令**

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'task/tests/verify_refactor.ps1'
cmake --build 'build'
```

预期：两条命令均以 `0` 退出；结构测试输出 `Refactor structure contract passed.`，Ninja 报告构建成功或 `no work to do`。

- [ ] **步骤 3：检查最终工作区边界**

运行：

```powershell
git status --short -- `
  'src/app_main.cpp' `
  'task/application/car_application.cpp' `
  'task/config/car_config.cpp' `
  'task/tests/verify_refactor.ps1'
```

预期：只列出四个迁移目标文件的已修改或未跟踪状态；任务没有改动头文件、CMake、模块代码或其他用户文件。
