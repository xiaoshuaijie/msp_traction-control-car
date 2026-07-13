# XRobot 模块管理分层实施计划

> **已废弃（2026-07-13）**：本计划基于 `NRF24L01App` 和旧 C API，不能用于当前代码。
> 当前实现以 `.helloagents/modules/xrobot_modules.md` 和
> `.helloagents/modules/nrf24l01.md` 为准。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `xrobot_main.hpp` 统一管理 GreySensor、Tracking、MPU6050 和 NRF24L01，同时让 `app_main.cpp` 继续负责车辆硬件和控制逻辑。

**Architecture:** `app_main.cpp` 创建硬件、唯一 `ApplicationManager` 和主循环；`XRobotModules` 接收这些依赖并直接持有四个模块。现有 NRF24L01 C 驱动由一个薄的 `LibXR::Application` 适配器接入统一调度，不改写底层协议。

**Tech Stack:** C++20、C11、LibXR ApplicationManager、TI MSPM0 DriverLib/SysConfig、CMake、Ninja、PowerShell

---

## 文件职责

- `module/NRF24L01/NRF24L01App.hpp`：定义 NRF24L01 的 LibXR 应用适配器、配置和运行状态。
- `module/NRF24L01/NRF24L01App.cpp`：实现一次初始化、10 ms 轮询和状态统计。
- `module/NRF24L01/CMakeLists.txt`：编译 NRF24L01 的 C/C++ 驱动和适配器。
- `xrobot_main.hpp`：定义并持有四个应用模块，提供业务层所需访问器。
- `src/app_main.cpp`：创建硬件和模块组合，保留车辆控制主循环。
- `CMakeLists.txt`：向固件目标公开项目根目录头文件路径。
- `tests/verify_xrobot_module_management.ps1`：逐步验证适配器、组合类和业务接线契约。

### Task 1: 创建 NRF24L01 LibXR 适配器

**Files:**
- Create: `module/NRF24L01/NRF24L01App.hpp`
- Create: `module/NRF24L01/NRF24L01App.cpp`
- Modify: `module/NRF24L01/CMakeLists.txt:5-8`
- Create: `tests/verify_xrobot_module_management.ps1`

- [ ] **Step 1: 写入适配器结构验证并确认 RED**

创建 `tests/verify_xrobot_module_management.ps1`：

```powershell
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function Read-RequiredFile {
  param([string]$Path, [string]$MissingMessage)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw $MissingMessage
  }
  return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
}

function Assert-Contains {
  param([string]$Content, [string]$Expected, [string]$Message)

  if (-not $Content.Contains($Expected)) {
    throw $Message
  }
}

function Assert-NotContains {
  param([string]$Content, [string]$Unexpected, [string]$Message)

  if ($Content.Contains($Unexpected)) {
    throw $Message
  }
}

$adapterHeaderPath = Join-Path $root 'module/NRF24L01/NRF24L01App.hpp'
$adapterSourcePath = Join-Path $root 'module/NRF24L01/NRF24L01App.cpp'
$nrfCMakePath = Join-Path $root 'module/NRF24L01/CMakeLists.txt'

$adapterHeader = Read-RequiredFile $adapterHeaderPath `
  'Missing NRF24L01 LibXR adapter header.'
$adapterSource = Read-RequiredFile $adapterSourcePath `
  'Missing NRF24L01 LibXR adapter implementation.'
$nrfCMake = Read-RequiredFile $nrfCMakePath `
  'Missing NRF24L01 CMake configuration.'

Assert-Contains $adapterHeader 'class NRF24L01App : public LibXR::Application' `
  'NRF24L01App must be managed by ApplicationManager.'
Assert-Contains $adapterHeader 'const State& GetState() const' `
  'NRF24L01App must expose read-only runtime state.'
Assert-Contains $adapterSource 'NRF24L01_Init();' `
  'NRF24L01App must initialize the existing driver.'
Assert-Contains $adapterSource 'NRF24L01_Receive();' `
  'NRF24L01App must poll the existing driver.'
Assert-Contains $nrfCMake '"${CMAKE_CURRENT_LIST_DIR}/*.cpp"' `
  'NRF24L01 CMake must compile C++ sources.'

Write-Host 'XRobot module management structure checks passed.'
```

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

Expected: 退出码非 `0`，错误为 `Missing NRF24L01 LibXR adapter header.`。

- [ ] **Step 2: 定义 NRF24L01App 接口**

创建 `module/NRF24L01/NRF24L01App.hpp`：

```cpp
#pragma once

#include <array>
#include <cstdint>

#include "app_framework.hpp"
#include "nrf24l01.h"

class NRF24L01App : public LibXR::Application
{
 public:
  struct Config
  {
    uint32_t poll_period_ms = 10;
  };

  struct State
  {
    uint8_t receive_status = 0;
    uint32_t receive_count = 0;
    uint32_t error_count = 0;
    std::array<uint8_t, NRF24L01_RX_PACKET_WIDTH> receive_data{};
  };

  explicit NRF24L01App(LibXR::ApplicationManager& app);
  NRF24L01App(LibXR::ApplicationManager& app, const Config& config);

  void OnMonitor() override;

  [[nodiscard]] const State& GetState() const { return state_; }

 private:
  Config config_;
  State state_;
  uint32_t last_poll_ms_ = 0;
  bool initialized_ = false;
};
```

- [ ] **Step 3: 实现非阻塞初始化与轮询**

创建 `module/NRF24L01/NRF24L01App.cpp`：

```cpp
#include "NRF24L01App.hpp"

#include <algorithm>

#include "thread.hpp"

NRF24L01App::NRF24L01App(LibXR::ApplicationManager& app)
    : NRF24L01App(app, Config{})
{
}

NRF24L01App::NRF24L01App(LibXR::ApplicationManager& app,
                         const Config& config)
    : config_(config)
{
  app.Register(*this);
}

void NRF24L01App::OnMonitor()
{
  const uint32_t now = LibXR::Thread::GetTime();
  if (!initialized_)
  {
    NRF24L01_Init();
    initialized_ = true;
    last_poll_ms_ = now;
    return;
  }

  if (now - last_poll_ms_ < config_.poll_period_ms)
  {
    return;
  }
  last_poll_ms_ = now;

  const uint8_t receive_status = NRF24L01_Receive();
  state_.receive_status = receive_status;
  if (receive_status == 1U)
  {
    std::copy_n(NRF24L01_RxPacket, state_.receive_data.size(),
                state_.receive_data.begin());
    ++state_.receive_count;
  }
  else if (receive_status == 2U || receive_status == 3U)
  {
    ++state_.error_count;
  }
}
```

- [ ] **Step 4: 让 NRF24L01 CMake 编译 C++ 文件**

将 `module/NRF24L01/CMakeLists.txt` 的源文件匹配改为：

```cmake
file(GLOB MODULE_NRF24L01_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)
```

- [ ] **Step 5: 运行验证并确认 GREEN**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

Expected: 退出码 `0`，输出 `XRobot module management structure checks passed.`。

- [ ] **Step 6: 提交适配器单元**

```powershell
git add -- module/NRF24L01/NRF24L01App.hpp `
  module/NRF24L01/NRF24L01App.cpp `
  module/NRF24L01/CMakeLists.txt `
  module/NRF24L01/nrf24l01.cpp `
  module/NRF24L01/nrf24l01.h `
  module/NRF24L01/nrf24l01_define.h `
  tests/verify_xrobot_module_management.ps1
git diff --cached --check -- module/NRF24L01/NRF24L01App.hpp `
  module/NRF24L01/NRF24L01App.cpp `
  module/NRF24L01/CMakeLists.txt `
  module/NRF24L01/nrf24l01.cpp `
  module/NRF24L01/nrf24l01.h `
  module/NRF24L01/nrf24l01_define.h `
  tests/verify_xrobot_module_management.ps1
git commit --only -m "feat: 添加 NRF24L01 应用适配器" `
  -m "feat: add NRF24L01 application adapter" -- `
  module/NRF24L01/NRF24L01App.hpp `
  module/NRF24L01/NRF24L01App.cpp `
  module/NRF24L01/CMakeLists.txt `
  module/NRF24L01/nrf24l01.cpp `
  module/NRF24L01/nrf24l01.h `
  module/NRF24L01/nrf24l01_define.h `
  tests/verify_xrobot_module_management.ps1
```

Expected: 提交只包含 NRF24L01 驱动、适配器、CMake 和本任务验证脚本。

### Task 2: 将 xrobot_main.hpp 改为模块组合类

**Files:**
- Modify: `xrobot_main.hpp:1-32`
- Modify: `tests/verify_xrobot_module_management.ps1`

- [ ] **Step 1: 扩展组合类契约并确认 RED**

在验证脚本成功输出之前追加：

```powershell
$xrobotMainPath = Join-Path $root 'xrobot_main.hpp'
$xrobotMain = Read-RequiredFile $xrobotMainPath 'Missing xrobot_main.hpp.'

Assert-Contains $xrobotMain 'class XRobotModules' `
  'xrobot_main.hpp must define XRobotModules.'
Assert-Contains $xrobotMain 'GreySensor grey_sensor_;' `
  'XRobotModules must own GreySensor.'
Assert-Contains $xrobotMain 'Tracking tracking_;' `
  'XRobotModules must own Tracking.'
Assert-Contains $xrobotMain 'MPU6050 mpu6050_;' `
  'XRobotModules must own MPU6050.'
Assert-Contains $xrobotMain 'NRF24L01App nrf24l01_;' `
  'XRobotModules must own NRF24L01App.'
Assert-Contains $xrobotMain 'Tracking& TrackingModule()' `
  'XRobotModules must expose Tracking to vehicle logic.'
Assert-NotContains $xrobotMain '#include "BlinkLED.hpp"' `
  'xrobot_main.hpp must not reference BlinkLED.'
Assert-NotContains $xrobotMain '#include "JY61P.hpp"' `
  'xrobot_main.hpp must not reference JY61P.'
Assert-NotContains $xrobotMain 'while (true)' `
  'xrobot_main.hpp must not own the firmware loop.'
```

Run the script. Expected: 退出码非 `0`，错误为 `xrobot_main.hpp must define XRobotModules.`。

- [ ] **Step 2: 用 XRobotModules 替换生成式主循环**

将 `xrobot_main.hpp` 完整替换为：

```cpp
#pragma once

#include <array>

#include "GreySensor.hpp"
#include "MPU6050.hpp"
#include "NRF24L01App.hpp"
#include "Tracking.hpp"
#include "app_framework.hpp"

class XRobotModules
{
 public:
  struct Config
  {
    std::array<const char*, GreySensor::MAX_CHANNEL_COUNT>
        grey_sensor_aliases{};
    bool grey_sensor_active_low = false;
    MPU6050::Config mpu6050{};
    NRF24L01App::Config nrf24l01{};
  };

  XRobotModules(LibXR::HardwareContainer& hardware,
                LibXR::ApplicationManager& app_manager,
                const Config& config)
      : grey_sensor_(hardware, app_manager,
                     {config.grey_sensor_aliases[0],
                      config.grey_sensor_aliases[1],
                      config.grey_sensor_aliases[2],
                      config.grey_sensor_aliases[3],
                      config.grey_sensor_aliases[4],
                      config.grey_sensor_aliases[5],
                      config.grey_sensor_aliases[6],
                      config.grey_sensor_aliases[7]},
                     config.grey_sensor_active_low),
        tracking_(app_manager),
        mpu6050_(hardware, app_manager, config.mpu6050),
        nrf24l01_(app_manager, config.nrf24l01)
  {
  }

  [[nodiscard]] GreySensor& GreySensorModule() { return grey_sensor_; }
  [[nodiscard]] Tracking& TrackingModule() { return tracking_; }
  [[nodiscard]] MPU6050& MPU6050Module() { return mpu6050_; }
  [[nodiscard]] NRF24L01App& NRF24L01Module() { return nrf24l01_; }

 private:
  GreySensor grey_sensor_;
  Tracking tracking_;
  MPU6050 mpu6050_;
  NRF24L01App nrf24l01_;
};
```

- [ ] **Step 3: 运行结构验证并确认 GREEN**

Run the script. Expected: 退出码 `0`，且旧的 BlinkLED/JY61P/无限循环引用全部消失。

- [ ] **Step 4: 提交模块组合类**

```powershell
git add -- xrobot_main.hpp tests/verify_xrobot_module_management.ps1
git diff --cached --check -- xrobot_main.hpp `
  tests/verify_xrobot_module_management.ps1
git commit --only -m "refactor: 集中管理 XRobot 模块" `
  -m "refactor: centralize XRobot module management" -- `
  xrobot_main.hpp tests/verify_xrobot_module_management.ps1
```

Expected: 提交只包含 `xrobot_main.hpp` 和扩展后的结构验证。

### Task 3: 在 app_main.cpp 接入模块组合

**Files:**
- Modify: `src/app_main.cpp:3-16,268-345`
- Modify: `CMakeLists.txt:30-42`
- Modify: `tests/verify_xrobot_module_management.ps1`

- [ ] **Step 1: 扩展业务接线契约并确认 RED**

在验证脚本成功输出之前追加：

```powershell
$appMainPath = Join-Path $root 'src/app_main.cpp'
$rootCMakePath = Join-Path $root 'CMakeLists.txt'
$appMain = Read-RequiredFile $appMainPath 'Missing src/app_main.cpp.'
$rootCMake = Read-RequiredFile $rootCMakePath 'Missing root CMakeLists.txt.'

Assert-Contains $appMain '#include "xrobot_main.hpp"' `
  'app_main.cpp must include the module composition.'
Assert-Contains $appMain 'LibXR::ApplicationManager app_manager;' `
  'app_main.cpp must own the only ApplicationManager.'
Assert-Contains $appMain 'XRobotModules modules(hardware, app_manager, module_config);' `
  'app_main.cpp must create XRobotModules.'
Assert-Contains $appMain 'Tracking& tracking = modules.TrackingModule();' `
  'vehicle logic must access Tracking through XRobotModules.'
Assert-Contains $appMain 'LibXR::Topic::ASyncSubscriber<MPU6050::Sample>' `
  'app_main.cpp must subscribe to MPU6050 samples for debugging.'
Assert-Contains $appMain 'g_nrf24l01_state = modules.NRF24L01Module().GetState();' `
  'app_main.cpp must publish an Ozone-visible NRF24L01 snapshot.'
Assert-Contains $appMain 'app_manager.MonitorAll();' `
  'app_main.cpp must drive the module manager.'
Assert-NotContains $appMain 'GreySensor grey_sensor(' `
  'app_main.cpp must not own GreySensor directly.'
Assert-NotContains $appMain 'Tracking tracking(' `
  'app_main.cpp must not own Tracking directly.'
Assert-Contains $rootCMake '"${CMAKE_SOURCE_DIR}"' `
  'the firmware target must include the project root.'
```

Run the script. Expected: 退出码非 `0`，首个错误为 `app_main.cpp must include the module composition.`。

- [ ] **Step 2: 增加 I2C 与模块组合头文件**

在 `src/app_main.cpp` 的模块头文件区增加：

```cpp
#include "mspm0_i2c.hpp"
#include "xrobot_main.hpp"
```

在现有 `extern "C"` 调试变量区增加：

```cpp
MPU6050::Sample g_mpu6050_sample{};
volatile uint32_t g_mpu6050_calibration_count = 0;
volatile uint32_t g_mpu6050_consecutive_failures = 0;
volatile int32_t g_mpu6050_last_error = 0;
volatile uint8_t g_mpu6050_calibrated = 0;
NRF24L01App::State g_nrf24l01_state{};
```

- [ ] **Step 3: 创建 MPU6050 I2C 适配器并注册硬件别名**

在灰度 GPIO 创建完成后、`HardwareContainer` 之前增加：

```cpp
std::array<uint8_t, MPU6050::BUFFER_SIZE> mpu6050_i2c_stage_buffer{};
LibXR::MSPM0I2C mpu6050_i2c(MSPM0_I2C_INIT(
    I2C_0, mpu6050_i2c_stage_buffer.data(),
    mpu6050_i2c_stage_buffer.size(), 8));
```

把以下 I2C 条目放到 `HardwareContainer` 参数首位，其余 GPIO 条目保持原顺序：

```cpp
LibXR::Entry<LibXR::I2C>{mpu6050_i2c,
                         {"i2c_mpu6050", "imu", "i2c2"}},
```

- [ ] **Step 4: 用 XRobotModules 替换 GreySensor 与 Tracking 局部实例**

保留 `ApplicationManager` 和 `BitsButtonXR`，删除原来的 `GreySensor grey_sensor(...)` 与 `Tracking tracking(...)`，改为：

```cpp
const XRobotModules::Config module_config{
    .grey_sensor_aliases = kGreySensorAliases,
    .grey_sensor_active_low = kGreySensorActiveLow,
    .mpu6050 = {},
    .nrf24l01 = {.poll_period_ms = 10},
};
XRobotModules modules(hardware, app_manager, module_config);
Tracking& tracking = modules.TrackingModule();
LibXR::Topic::ASyncSubscriber<MPU6050::Sample> mpu6050_subscriber("mpu6050");
mpu6050_subscriber.StartWaiting();
```

保留现有 `tracking_subscriber`、GPIO 上拉配置、`ResetTrackingState()` 调用和车辆控制分支。

- [ ] **Step 5: 在 MonitorAll 后同步调试快照**

紧跟现有 `app_manager.MonitorAll();` 增加：

```cpp
MPU6050& mpu6050 = modules.MPU6050Module();
g_mpu6050_calibration_count = mpu6050.CalibrationCount();
g_mpu6050_consecutive_failures = mpu6050.ConsecutiveFailures();
g_mpu6050_last_error = static_cast<int32_t>(mpu6050.LastError());
g_mpu6050_calibrated = mpu6050.IsCalibrated() ? 1U : 0U;
if (mpu6050_subscriber.Available())
{
  g_mpu6050_sample = mpu6050_subscriber.GetData();
  mpu6050_subscriber.StartWaiting();
}
g_nrf24l01_state = modules.NRF24L01Module().GetState();
```

该代码只复制状态，不改变驾驶模式或电机输出。

- [ ] **Step 6: 让固件目标包含项目根目录**

在根 `CMakeLists.txt` 的固件私有头文件目录首位增加：

```cmake
"${CMAKE_SOURCE_DIR}"
```

不得恢复 `add_subdirectory("${CMAKE_SOURCE_DIR}/task")`。

- [ ] **Step 7: 运行结构验证并确认 GREEN**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
```

Expected: 退出码 `0`，所有模块管理和业务职责检查通过。

- [ ] **Step 8: 配置并构建固件**

Run:

```powershell
cmake -S . -B build
cmake --build build
```

Expected: 两个命令退出码均为 `0`；生成 `.elf`、`.hex` 和 `.bin`；输出不包含缺失头文件或未定义模块符号。

- [ ] **Step 9: 提交业务接线**

提交前检查 `CMakeLists.txt` 是否仍包含用户并行改动。若除已批准的模块入口和根目录 include 外还有无关改动，停止并保留为未提交状态；否则运行：

```powershell
git add -- src/app_main.cpp CMakeLists.txt `
  tests/verify_xrobot_module_management.ps1
git diff --cached --check -- src/app_main.cpp CMakeLists.txt `
  tests/verify_xrobot_module_management.ps1
git commit --only -m "refactor: 分离模块管理与车辆逻辑" `
  -m "refactor: separate module management from vehicle logic" -- `
  src/app_main.cpp CMakeLists.txt `
  tests/verify_xrobot_module_management.ps1
```

Expected: 提交不包含 SysConfig、编码器、电机参数或其他并行任务文件。

### Task 4: 最终回归验收

**Files:**
- Verify: `xrobot_main.hpp`
- Verify: `src/app_main.cpp`
- Verify: `module/NRF24L01/NRF24L01App.hpp`
- Verify: `module/NRF24L01/NRF24L01App.cpp`
- Verify: `tests/verify_xrobot_module_management.ps1`

- [ ] **Step 1: 运行全部本地验证**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/verify_xrobot_module_management.ps1
if (Test-Path -LiteralPath 'tests/run_topic_module_logic_tests.ps1') {
  powershell -NoProfile -ExecutionPolicy Bypass `
    -File tests/run_topic_module_logic_tests.ps1
}
cmake -S . -B build
cmake --build build
```

Expected: 所有命令退出码为 `0`；现有主题模块逻辑测试继续通过。

- [ ] **Step 2: 验证旧诊断已消失**

```powershell
rg -n 'BlinkLED.hpp|JY61P.hpp|NRF24L01.hpp|while \(true\)' xrobot_main.hpp
```

Expected: `rg` 退出码为 `1`，无匹配输出。

- [ ] **Step 3: 核对固件产物**

```powershell
Get-Item build/ti_mspm0_libxr_dev.elf, `
  build/ti_mspm0_libxr_dev.hex, `
  build/ti_mspm0_libxr_dev.bin |
  Select-Object Name,Length,LastWriteTime
```

Expected: 三个产物均存在且长度大于 `0`。

- [ ] **Step 4: 核对差异范围**

```powershell
git diff --check
git status --short
git diff HEAD -- xrobot_main.hpp src/app_main.cpp CMakeLists.txt `
  module/NRF24L01/NRF24L01App.hpp `
  module/NRF24L01/NRF24L01App.cpp `
  module/NRF24L01/CMakeLists.txt `
  tests/verify_xrobot_module_management.ps1
```

Expected: 变更只覆盖模块组合、NRF24L01 适配、业务接线、构建入口和验证脚本；`task/`、车辆控制公式和其他模块实现无意外变化。

- [ ] **Step 5: 记录硬件验收项**

上板后在 Ozone 观察：

```text
g_mpu6050_sample.sequence
g_mpu6050_calibrated
g_mpu6050_calibration_count
g_mpu6050_last_error
g_nrf24l01_state.receive_status
g_nrf24l01_state.receive_count
g_nrf24l01_state.error_count
g_nrf24l01_state.receive_data
```

Expected: MPU6050 连接时完成校准并持续采样；NRF24L01 收包时计数与数据更新；任一模块未连接时按键、循迹和电机控制主循环继续运行。
