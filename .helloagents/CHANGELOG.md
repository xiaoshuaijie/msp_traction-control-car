# 变更日志

## [Unreleased]

### 新增

- **[Encoder]**: 改为由 ApplicationManager 周期驱动的 Topic 模块，发布四轮一致计数、角度和速度快照，并增加计数回绕与中断临界区保护。
- **[NRF24L01]**: 新增 `tx/rx/status` 强类型 Topic、非阻塞收发状态机、可配置超时及错误恢复，移除全局数组式 C API。
- **[XRobotModules]**: 统一装配 GreySensor、Tracking、MPU6050、Encoder 和 NRF24L01，增加 50 ms 控制 Topic 新鲜度保护。
- **[XRobotModules]**: 新增 HC-SR04 板级资源注入和统一生命周期管理，六个 Topic/Application 模块由同一管理器驱动。
- **[Ozone]**: 新增 HC-SR04、MPU6050、NRF24L01 接收包和状态的完整 Topic 快照，不改变车辆控制数据流。
- **[Build]**: 新增 128450 字节固件 Flash 预算门禁及边界测试。

### 重构

- **[app_main]**: 使用 Tracking/Encoder Topic 缓存驱动车辆控制，将控制配置和调试符号拆分到 `car_control_support`，保持 PID、方向表和控制公式不变。

### 修复

- **[Tests]**: NRF24L01 中文文档契约改用 ASCII Unicode 正则转义，兼容 Windows PowerShell 5.1 与 UTF-8 无 BOM 脚本。
- **[LibXR MSPM0PWM]**: 修复被截断的 `MSPM0_PWM_INIT`，从 Timer 当前分频配置恢复源时钟并返回完整 PWM 资源。

### 快速修改

- **[app_main]**: 为五个结构化全局调试快照补充数据来源、内容和 Ozone 观察用途注释 — by xiaoshuaijie
  - 类型: 快速修改（无方案包）
  - 文件: src/app_main.cpp:19-28

- **[MPU6050]**: 为公共接口、寄存器配置、采样标定、姿态融合和构建脚本补充详细中文注释 — by xiaoshuaijie
  - 类型: 快速修改（无方案包）
  - 文件: module/mpu6050/mpu6050.hpp:1-310, module/mpu6050/MPU6050.cpp:1-499, module/mpu6050/CMakeLists.txt:1-37

- **[启动与模块装配]**: 按主流程、主循环和模块装配顺序补充硬件初始化、Topic 数据流及生命周期注释 — by xiaoshuaijie
  - 类型: 快速修改（无方案包）
  - 文件: src/app_main.cpp:21-377, xrobot_main.hpp:16-114

## [0.1.3] - 2026-07-13

### 优化

- **[Encoder]**: 为正交解码、中断临界区、计数回绕、四轮运动学换算、Topic 发布和 CMake 集成补充详细中文注释 — by xiaoshuaijie
  - 方案: [202607132138_encoder-中文详细注释](archive/2026-07/202607132138_encoder-中文详细注释/)

## [0.1.2] - 2026-07-13

### 优化

- **[HC_SR04]**: 为接口、硬件资源、PWM/Capture 时序、距离换算、EMA 和构建接入补充详细中文注释，保持 Manifest 与运行逻辑不变 — by xiaoshuaijie
  - 方案: [202607132138_hc-sr04-中文注释](archive/2026-07/202607132138_hc-sr04-中文注释/)

## [0.1.1] - 2026-07-13

### 重构

- **[app_main]**: 使用四个模块完整 Sample 和一个 `CarControlSample` 替代散落的 `volatile g_*` 调试镜像，并新增 GreySensor Topic 调试订阅 — by xiaoshuaijie
  - 方案: [202607132127_结构化控制调试快照](archive/2026-07/202607132127_结构化控制调试快照/)
  - 决策: 结构化控制调试快照#D001（优先复用完整模块样本，并聚合剩余控制状态）

## [0.1.0] - 2026-07-13

### 新增

- **[HC_SR04]**: 新增基于 `MSPM0PWM` 和 Timer Capture 的超声波距离 Topic 模块，包含 EMA、无回波/超量程状态、SysConfig/CMake 接入与主机测试 — by xiaoshuaijie
  - 方案: [2026-07-13-hc-sr04](../docs/superpowers/plans/2026-07-13-hc-sr04.md)
  - 决策: [hc_sr04#D001](../docs/superpowers/specs/2026-07-13-hc-sr04-design.md#hc-sr04-d001)(采用约 16 Hz/10 us PWM Trigger，并由 OnMonitor 轮询脉宽 Capture 原始事件)
