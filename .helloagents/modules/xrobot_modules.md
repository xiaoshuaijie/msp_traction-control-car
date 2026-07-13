# XRobotModules 装配器

## 职责边界

`xrobot_main.hpp` 定义 `XRobotModules`，按以下依赖顺序持有 Topic 模块：

```text
GreySensor -> Tracking -> MPU6050 -> Encoder -> NRF24L01 -> HC_SR04
```

装配器集中管理 Topic 名、灰度 GPIO 别名、MPU6050 I2C 别名、NRF24L01 GPIO
别名和 HC-SR04 运行配置，但不创建 `ApplicationManager`、不调用 `MonitorAll()`、
不拥有主循环。HC-SR04 的 PWM/Capture 属于板级资源，由 `app_main.cpp` 通过
`HC_SR04::Resources` 显式传入构造函数。

`src/app_main.cpp` 创建底层 GPIO/I2C/PWM/Capture、唯一 `HardwareContainer`、唯一
`ApplicationManager` 和车辆控制循环。GreySensor、Tracking 与 Encoder 结果通过各自的
Topic 订阅；
任一控制输入超过 50 ms 未更新时，相关目标速度归零，Encoder 过期还会阻止速度环
输出并重置 PID。

## 调试快照

`src/app_main.cpp` 在文件作用域提供以下结构化全局对象，供 Ozone 展开查看：

- `grey_sensor_sample`：完整 `GreySensor::Sample`，包含原始/有效掩码、位置、丢线记忆和序号。
- `tracking_output`：完整 `Tracking::Output`，包含循迹误差、丢线状态和左右/四轮目标速度。
- `encoder_sample`：完整 `Module::Encoder::Sample`，包含四轮计数、角度、速度、时间和序号。
- `hc_sr04_sample`：完整 `HC_SR04::Sample`，包含脉宽、原始/滤波距离、状态和序号。
- `mpu6050_sample`：完整 `MPU6050::Sample`，包含六轴物理量、姿态和序号。
- `nrf24l01_rx_packet`：完整 `NRF24L01::RxPacket`，包含载荷、管道、时间和序号。
- `nrf24l01_status`：完整 `NRF24L01::Status`，包含状态机、发送结果和累计计数。
- `button_event`：完整 `BitsButtonXR::ButtonEventResult`，保存最近一次按键事件。
- `car_control_sample`：`CarControlSample`，补充控制周期、驾驶模式、Topic 新鲜度、定距结果和四轮闭环中间量。

这些对象替代逐字段 `volatile g_*` 镜像。PID 参数、前馈参数和 PID 实例仍保留为
独立全局控制配置，便于在线调参。

## 固件预算

POST_BUILD 使用 `cmake/CheckFirmwareSize.cmake` 检查 `.bin`，当前硬门限为
128450 字节（128 KiB 的约 98%）。本次接入后固件为 80768 字节（61.62%），
超过门限时构建失败。

## 验证

- `tests/verify_xrobot_module_management.ps1`
- `tests/verify_flash_budget.ps1`
- ARM 固件完整构建

## 设计与计划

- [统一接入设计](../../docs/superpowers/specs/2026-07-13-three-module-xrobot-ozone-design.md)
- [统一接入实施计划](../../docs/superpowers/plans/2026-07-13-three-module-xrobot-ozone.md)
