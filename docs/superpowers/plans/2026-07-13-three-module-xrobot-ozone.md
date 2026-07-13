# 三模块统一接入与 Ozone 调试快照实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `22——year_car` 分支统一装配 HC-SR04、MPU6050 和 NRF24L01，
并为三个模块暴露完整 Ozone Topic 快照。

**Architecture:** `app_main.cpp` 创建板级资源和唯一主循环，`XRobotModules`
直接持有六个 Topic/Application 模块。新增调试订阅只复制模块输出，不进入车辆控制
数据流。

**Tech Stack:** C++20、LibXR Application/Topic、TI MSPM0 DriverLib/SysConfig、
CMake、Ninja、PowerShell、arm-none-eabi GCC

---

### Task 1: 建立分支与测试红灯

**Files:**
- Modify: `tests/verify_xrobot_module_management.ps1`

- [x] 记录 `base-car` 脏工作区并创建 `22——year_car`，不 stash、reset 或清理。
- [x] 增加 HC-SR04 所有权、资源接线、四个快照及订阅重挂断言。
- [x] 运行结构测试，确认因缺少 HC-SR04 装配契约而失败。

### Task 2: 实现统一装配和调试快照

**Files:**
- Modify: `xrobot_main.hpp`
- Modify: `src/app_main.cpp`

- [x] 为 `XRobotModules` 增加 HC-SR04 Topic、配置、资源构造参数、成员和访问器。
- [x] 在 `app_main.cpp` 组装 PWM/Capture 资源并传入装配器。
- [x] 新增 HC-SR04、MPU6050、NRF 接收包和 NRF 状态异步订阅与全局快照。
- [x] 运行 XRobot 结构测试并确认通过。

### Task 3: 修复中文文档契约验证

**Files:**
- Modify: `tests/verify_topic_module_structure.ps1`

- [x] 复现 NRF24L01 的四项旧英文文档契约失败。
- [x] 确认根因是 Windows PowerShell 5.1 对 UTF-8 无 BOM 中文脚本文本的解析。
- [x] 使用 ASCII `\uXXXX` 正则匹配等价中文契约并确认结构测试通过。

### Task 4: 修复 MSPM0 PWM 资源宏

**Files:**
- Modify: `tests/verify_hc_sr04_integration.ps1`
- Modify: `libxr/driver/mspm0/mspm0_pwm.hpp`

- [x] 通过 ARM 编译复现 `MSPM0_PWM_INIT` 截断导致的聚合初始化失败。
- [x] 增加宏完整性、时钟配置读取和源时钟恢复的回归断言并确认红灯。
- [x] 读取 Timer 当前 `divideRatio`/`prescale`，恢复分频前源时钟并返回完整资源。
- [x] 重跑 HC-SR04 集成检查和 ARM 构建并确认通过。

### Task 5: 同步知识库并完成验收

**Files:**
- Modify: `.helloagents/modules/xrobot_modules.md`
- Modify: `.helloagents/modules/hc_sr04.md`
- Modify: `.helloagents/modules/_index.md`
- Modify: `.helloagents/CHANGELOG.md`

- [x] 落盘设计、实施计划和模块知识库变更。
- [x] 运行全部结构、主机逻辑、Flash 预算和 ARM 固件构建。
- [x] 执行 `git diff --check`，确认当前分支、固件体积和未提交差异。
- [x] 报告需要实车、HC-SR04、MPU6050、NRF24L01 和调试探针的上板验收项。

本计划不执行 commit、push、merge，不修改现有未完成的 NRF24L01 中文注释方案包。
