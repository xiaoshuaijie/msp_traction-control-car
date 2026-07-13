# XRobot 模块管理分层设计

> **已废弃（2026-07-13）**：本设计使用的 `NRF24L01App` 薄适配器已被完整的
> `NRF24L01` Topic/Application 状态机取代，且装配清单已加入 Encoder。
> 当前边界见 `.helloagents/modules/xrobot_modules.md`。

## 目标

将可复用模块的组合与生命周期集中到根目录 `xrobot_main.hpp`，同时保留 `src/app_main.cpp` 对车辆硬件、驾驶状态机和控制循环的所有权。

本次管理的模块为：

- `GreySensor`
- `Tracking`
- `MPU6050`
- `NRF24L01App`

不引入当前车辆未使用的 `BlinkLED` 和 `JY61P` 模块。

## 职责边界

### xrobot_main.hpp

`xrobot_main.hpp` 定义 `XRobotModules` 组合类，负责：

- 按依赖顺序构造并持有四个模块。
- 接收外部创建的 `HardwareContainer` 和 `ApplicationManager`。
- 保存模块运行所需的配置。
- 向车辆业务层暴露最小必要访问器，例如 `TrackingModule()`。

`xrobot_main.hpp` 不创建硬件对象、不创建 `ApplicationManager`、不实现无限循环，也不直接控制电机。

### app_main.cpp

`app_main.cpp` 继续负责：

- 创建时间基准、GPIO、I2C 适配器和 `HardwareContainer`。
- 创建唯一的 `ApplicationManager`。
- 创建 `XRobotModules` 并向其传入硬件、管理器和模块配置。
- 处理按键、编码器、电机、PID、驾驶模式和循迹输出。
- 在唯一主循环中调用一次 `app_manager.MonitorAll()`。

业务代码通过 `XRobotModules` 的访问器取得 `Tracking` 引用，继续执行 `Reset()` 和 `GetLatestOutput()`，避免模块管理迁移改变现有车辆行为。

## 组件设计

### XRobotModules

`XRobotModules` 以直接成员形式持有模块，成员声明与构造顺序为：

1. `GreySensor`
2. `Tracking`
3. `MPU6050`
4. `NRF24L01App`

构造函数接收：

- `LibXR::HardwareContainer&`
- `LibXR::ApplicationManager&`
- 灰度传感器硬件别名与有效电平配置
- MPU6050 配置
- NRF24L01 轮询配置

模块对象随 `app_main()` 栈帧存在。由于固件主循环不退出，其生命周期覆盖整个固件运行期。

### NRF24L01App

现有 NRF24L01 实现是 C 接口，不能直接注册到 `ApplicationManager`。新增薄适配器 `NRF24L01App`：

- 继承 `LibXR::Application`。
- 构造后注册到外部 `ApplicationManager`。
- 首次监控时调用一次 `NRF24L01_Init()`。
- 按 10 ms 周期调用 `NRF24L01_Receive()`。
- 返回 `0` 时保持等待；返回 `1` 时复制 32 字节数据并递增接收计数；返回 `2` 或 `3` 时递增错误计数。
- 只封装调度和状态，不复制或重写底层 SPI/GPIO 驱动。

底层驱动在错误返回前已有重新初始化逻辑，适配器不得再次初始化，避免重复恢复。

## 数据流

```text
app_main 硬件对象
        |
        v
HardwareContainer + ApplicationManager
        |
        v
XRobotModules
  |-- GreySensor --grey_sensor topic--> Tracking --tracking topic-->
  |                                                        app_main 车辆控制
  |-- MPU6050 ----mpu6050 topic---------------------------> 调试/业务订阅
  `-- NRF24L01App ----接收状态与数据----------------------> 调试/业务读取
```

`app_main.cpp` 保留 Tracking 订阅者和车辆控制决策。模块层只产生数据和维护模块状态，不直接驱动车轮。

## 构建调整

- `app_main.cpp` 包含 `xrobot_main.hpp` 并实例化 `XRobotModules`。
- 根 CMake 目标增加项目根目录头文件搜索路径，使 `xrobot_main.hpp` 可直接包含。
- NRF24L01 目标继续编译现有 `nrf24l01.cpp`，并加入适配器实现。
- `xrobot_main.hpp` 移除 `BlinkLED.hpp`、`JY61P.hpp` 和不存在的 `NRF24L01.hpp` 引用。
- 不恢复尚未实现的 `task/` 子目录构建入口。

## 错误处理

- MPU6050 沿用现有非阻塞初始化、校准与通信失败重试。
- NRF24L01 无数据不是错误；底层返回 `2/3` 时只记录状态，不在适配器层重复恢复。
- 模块异常不得阻断 `ApplicationManager::MonitorAll()`、按键事件、编码器采样或电机闭环。
- 模块配置或硬件别名错误应在构建验证或初始化阶段暴露，不添加静默降级路径。

## 验证策略

### 自动验证

- 新增结构验证脚本，检查 `XRobotModules` 的四个模块、访问器、唯一管理器约束以及旧生成头文件引用已移除。
- 先运行脚本确认当前结构不满足要求，再实施最小改动并确认脚本通过。
- 运行 `cmake -S . -B build`，确认生成构建图成功。
- 运行完整固件构建，确认 `.elf`、`.hex` 和 `.bin` 均生成。
- 构建输出不得包含 `BlinkLED.hpp`、`JY61P.hpp`、`NRF24L01.hpp` 缺失或模块未定义符号。

### 硬件验证

上板后通过 Ozone 观察：

- MPU6050 校准状态、样本序号和最近错误。
- NRF24L01 最近状态、接收计数、错误计数和最近数据。
- 现有灰度、循迹、按键、编码器、PID 和电机状态。

模块未连接或暂时无数据时，车辆主循环和其他控制功能必须继续运行。

## 非目标

- 不让 `xrobot_main.hpp` 接管主循环。
- 不把电机、编码器、按键或驾驶状态机迁入模块管理层。
- 不安装 `BlinkLED` 或 `JY61P`。
- 不重写 NRF24L01 底层通信协议。
- 不改变现有循迹、PID 或电机输出算法。
