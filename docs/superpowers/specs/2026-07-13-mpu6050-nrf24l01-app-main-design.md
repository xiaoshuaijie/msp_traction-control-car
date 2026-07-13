# MPU6050 与 NRF24L01 接入 app_main 设计

> **已废弃（2026-07-13）**：NRF24L01 不再通过 C API 在 `app_main` 中轮询。
> 当前实现由 `XRobotModules` 装配 Topic/Application 版本，详见
> `.helloagents/modules/nrf24l01.md` 与 `.helloagents/modules/xrobot_modules.md`。

## 目标

将 `module/MPU6050` 和 `module/NRF24L01` 接入现有 `src/app_main.cpp`，保持当前按键、循迹、编码器和电机控制行为不变。

- MPU6050 由现有 `LibXR::ApplicationManager` 周期驱动，完成初始化、校准、采样和 Topic 发布。
- NRF24L01 上电后进入接收模式，每 10 ms 轮询一次，仅接收和导出调试状态，不控制电机。
- 两个模块的运行状态均可通过全局变量在 Ozone Watch 中观察。

## 接入方案

### MPU6050

在 `app_main.cpp` 中包含 `MPU6050.hpp` 和 `mspm0_i2c.hpp`。使用 `MSPM0_I2C_INIT(I2C_0, ...)` 为 SysConfig 已配置的 I2C0 外设构造 `LibXR::MSPM0I2C` 适配器，并为其提供独立的暂存缓冲区。扩展现有 `HardwareContainer`，以 `i2c_mpu6050`、`imu` 和 `i2c2` 别名注册该适配器；其中 `i2c2` 仅用于兼容 MPU6050 模块当前的硬件查找别名，不代表实际外设编号。`ApplicationManager` 创建后构造一个 `MPU6050` 实例，使用模块默认配置。

主循环继续调用现有 `app_manager.MonitorAll()`，不增加阻塞式读取。创建 `MPU6050::Sample` 异步订阅者，在可用时复制最新样本到全局调试缓存，并重新调用 `StartWaiting()`。同时导出校准进度、最近错误和连续失败次数。

### NRF24L01

在 `app_main.cpp` 中以 C 接口包含 `nrf24l01.h`。所有 GPIO 均沿用 `ti_msp_dl_config.h` 中现有的 `NRF24L01_*` 配置，不重复创建 LibXR GPIO 对象。

完成其他模块构造后调用一次 `NRF24L01_Init()`。主循环使用独立时间戳，每 10 ms 调用一次 `NRF24L01_Receive()`：

- 返回 `0`：没有新数据，只更新最近状态。
- 返回 `1`：将 `NRF24L01_RxPacket` 的 32 字节复制到独立全局调试缓存，并递增成功接收计数。
- 返回 `2` 或 `3`：记录错误状态和错误计数。驱动内部已执行重新初始化，接入层不重复恢复。

轮询不进入 5 ms 电机控制分支，避免改变现有控制周期语义。

## 调试数据

在 `extern "C"` 全局区增加以下可观察状态：

- MPU6050 最新完整样本、样本序号、校准状态、最近错误和连续失败次数。
- NRF24L01 最近接收状态、成功接收次数、错误次数和最近 32 字节数据。

调试缓存与驱动内部缓冲分离，避免后续接收覆盖正在观察的数据。

## 构建调整

`module/NRF24L01/CMakeLists.txt` 当前只匹配 `*.c`，但实际实现文件为 `nrf24l01.cpp`。将源文件匹配扩展到 `*.cpp`，确保驱动实现被加入 `xr` 目标。MPU6050 的 CMake 已包含 C++ 源文件，无需额外调整。

## 错误处理

- MPU6050 初始化或 I2C 通信失败时沿用模块自身的定时恢复机制，不发布无效样本。
- NRF24L01 异常时沿用驱动内部重新初始化机制，接入层只记录状态，避免双重初始化。
- 任一传感器异常均不得阻止 `app_manager.MonitorAll()`、按键处理或电机闭环继续执行。

## 验收标准

- 工程可完成 CMake 配置与固件构建，不出现 MPU6050 或 NRF24L01 未定义符号。
- `app_main.cpp` 中两个模块均完成初始化并进入周期运行。
- MPU6050 有效连接时能完成校准并持续更新最新样本；未连接时主循环仍正常运行。
- NRF24L01 无数据时主循环正常运行；收到 32 字节数据时调试缓存和成功计数更新一次。
- NRF24L01 返回错误状态时错误计数更新，现有运动控制逻辑不受影响。
- 现有循迹、按键、编码器和电机控制代码不改变行为。

## 非目标

- 不用 NRF24L01 数据控制车辆。
- 不发送 MPU6050 数据。
- 不将 NRF24L01 重构为 LibXR `Application`。
- 不修改两个模块的通信协议、地址、射频通道或载荷长度。
