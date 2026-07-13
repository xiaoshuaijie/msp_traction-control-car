# 项目上下文

## 1. 基本信息

```yaml
名称: msp_traction-control-car
描述: 基于 MSPM0G3507 和 LibXR 的循迹控制小车固件
类型: 嵌入式固件
状态: 开发中
```

## 2. 技术上下文

```yaml
语言: C11、C++20、ARM Assembly
框架: LibXR Application、Topic 与 MSPM0 驱动适配层
硬件平台: TI MSPM0G3507
构建工具: CMake 3.20+、Ninja、arm-none-eabi GCC
配置工具: TI SysConfig 生成 ti_msp_dl_config.c/.h
```

### 主要依赖

| 依赖 | 用途 |
|------|------|
| LibXR | 应用调度、Topic、设备抽象和 MSPM0 驱动 |
| TI MSPM0 SDK DriverLib | 定时器、GPIO、I2C、DMA 等底层寄存器接口 |
| g++ | 硬件无关逻辑的主机单元测试 |

## 3. 项目概述

### 核心功能

- 灰度传感器采样、循迹计算和四轮速度闭环。
- 编码器、电机、IMU、NRF24L01 等车载模块。
- HC-SR04 PWM 触发、Echo 脉宽捕获、距离滤波和 Topic 发布。

### 项目边界

```yaml
范围内:
  - MSPM0 裸机固件、传感器与执行器模块、车辆控制算法
范围外:
  - 上位机应用、云服务和通用操作系统支持
```

## 4. 开发约定

```yaml
命名风格: C++ 类型使用 PascalCase，方法使用 PascalCase，成员变量以下划线结尾
目录组织: module/ 放业务模块，libxr/ 放框架与驱动，sysconfig/ 放生成配置，tests/ 放主机测试
错误处理: 启动期资源或配置错误使用 ASSERT，运行期测量异常通过状态字段发布
测试要求: 新增硬件无关行为先写主机测试；硬件适配至少通过 ARM 编译和目标构建
提交格式: 中文摘要 / English summary，可按任务使用路径限定提交
```

## 5. 当前约束

| 约束 | 原因 | 决策来源 |
|------|------|---------|
| HC-SR04 使用约 16 Hz、10 us Trigger | 保证 HC-SR04 测量间隔并避免长高电平误触发 | [HC-SR04 设计](../docs/superpowers/specs/2026-07-13-hc-sr04-design.md) |
| Echo 使用 1 MHz `PULSE_WIDTH_UP` Capture | 直接以微秒级计数测量 Echo 高电平 | [HC-SR04 设计](../docs/superpowers/specs/2026-07-13-hc-sr04-design.md) |
| 无效测量不更新 EMA | 防止超量程和无回波污染最近有效距离 | [HC-SR04 设计](../docs/superpowers/specs/2026-07-13-hc-sr04-design.md) |
| 控制 Topic 新鲜度上限为 50 ms | 数据链路停滞时禁止继续使用旧速度和循迹目标 | [XRobotModules](modules/xrobot_modules.md) |
| NRF24L01 采用单在途发送 | LibXR ASyncSubscriber 不提供发送队列 | [NRF24L01](modules/nrf24l01.md) |
| 固件 `.bin` 上限为 128450 字节 | 在物理 Flash 耗尽前保留最小演进空间 | [XRobotModules](modules/xrobot_modules.md) |

## 6. 已知技术债务

| 债务描述 | 优先级 | 建议处理时机 |
|---------|--------|-------------|
| HC-SR04 已接入主循环，但尚未使用真实传感器验证 Echo 波形和距离精度 | P1 | 上板联调阶段 |
| 当前完整固件 Flash 使用率为 61.62%（80,768 B / 128 KB） | P2 | 后续增加业务模块时持续运行 Flash 预算检查 |
