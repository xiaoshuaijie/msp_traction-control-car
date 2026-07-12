# app_main 初始化迁移设计

## 背景

当前 `src/app_main.cpp` 只负责构造并运行 `App::CarApplication`。应用对象的构造函数定义位于 `task/application/car_application.cpp`，可在线调节的全局控制参数 `g_control_config` 定义位于 `task/config/car_config.cpp`。

本次调整将这两处初始化定义集中到 `src/app_main.cpp`，方便在入口文件查看和调整应用组装及控制参数，同时保留现有模块边界和运行行为。

## 范围

- 将 `App::CarApplication::CarApplication()` 的完整定义迁移到 `src/app_main.cpp`。
- 将具有 C 链接的 `g_control_config` 完整定义迁移到 `src/app_main.cpp`。
- 保留 `CarApplication` 类声明、`Run()` 及其辅助方法的位置。
- 保留 `App::Config::MakeTrackingConfig()` 的现有位置。
- 更新结构验证脚本，使其验证新的定义归属。

不包含以下变更：

- 不修改成员声明顺序或构造顺序。
- 不修改 PID、前馈或循迹参数。
- 不修改硬件初始化调用及其顺序。
- 不重构控制循环、路线控制、调试状态或无线服务。

## 文件设计

### `src/app_main.cpp`

文件按以下顺序组织：

- 包含 `app_main.h`、`application/car_application.hpp` 以及构造函数和控制配置定义所需的配置、调试与线程头文件。
- 在 `extern "C"` 块内定义 `App::ControlConfig g_control_config`，保持现有字段值和数组顺序不变。
- 在 `namespace App` 中定义 `CarApplication::CarApplication()`，保持初始化列表和构造函数体不变。
- 保留现有 `extern "C" void app_main()`，继续创建局部 `App::CarApplication` 并调用 `Run()`。

### `task/application/car_application.cpp`

删除 `CarApplication::CarApplication()` 定义及仅由该定义使用的头文件依赖。保留 `Run()`、`UpdateImu()`、`HandleButtons()`、`RunControlTick()`、`SyncCompatibilityState()` 和 `ResolveButtonIndex()`。

### `task/config/car_config.cpp`

删除 `g_control_config` 定义，保留 `App::Config::MakeTrackingConfig()`。头文件中的 `extern "C" App::ControlConfig g_control_config` 声明保持不变，因此控制器调用方无需修改。

### `task/tests/verify_refactor.ps1`

移除 `src/app_main.cpp` 不超过 15 行的旧约束。新增结构断言：

- `src/app_main.cpp` 包含 `CarApplication::CarApplication()` 定义。
- `src/app_main.cpp` 包含 `g_control_config` 定义。
- 原来的两个实现文件不再包含对应定义，防止重复符号。
- `src/app_main.cpp` 仍构造 `App::CarApplication`。

## 链接与运行行为

迁移只改变两个外部定义所属的翻译单元。声明、符号名称、C 链接约定和所有调用方保持不变。CMake 已同时编译 `src/app_main.cpp` 与 `task` 目录源文件，因此不需要调整构建目标或源文件列表。

对象成员仍按 `car_application.hpp` 中的声明顺序构造。迁移后的初始化列表和构造函数体逐项保持原样，确保硬件配置、订阅启动、编码器与电机初始化、控制时基以及 NRF24 服务启动顺序不变。

## 验证

- 运行 `task/tests/verify_refactor.ps1`，验证定义位置及无重复定义。
- 使用现有 CMake 构建命令编译固件，验证头文件依赖、符号链接和指定初始化语法。
- 检查迁移前后控制配置字段值、构造初始化列表和构造函数体无语义差异。

## 完成标准

- 两项初始化定义均位于 `src/app_main.cpp`。
- 原实现文件不再重复定义对应符号。
- 结构验证脚本通过。
- 固件构建通过。
- 初始化顺序、参数值和运行行为保持不变。
