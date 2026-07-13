# 三模块统一接入与 Ozone 调试快照设计

## 目标

在保留现有车辆控制行为的前提下，让 `XRobotModules` 统一持有 HC-SR04、
MPU6050 和 NRF24L01，并在 `src/app_main.cpp` 暴露各模块最近一次完整 Topic
输出，供 Ozone 展开观察。

## 职责边界

- `app_main.cpp` 创建板级 GPIO、I2C、PWM/Capture 资源、唯一
  `HardwareContainer`、唯一 `ApplicationManager` 和车辆控制循环。
- `xrobot_main.hpp` 持有 GreySensor、Tracking、MPU6050、Encoder、NRF24L01
  和 HC-SR04，集中管理模块配置与 Topic 名称。
- HC-SR04 的 PWM/Capture 是板级资源，必须由 `app_main.cpp` 通过
  `HC_SR04::Resources` 显式传入装配器。
- 调试快照只复制 Topic 数据，不参与驾驶模式、PID、循迹或电机输出。

## 接口设计

`XRobotModules` 新增以下契约：

```cpp
static constexpr const char* kUltrasonicTopicName = "hc_sr04";

struct Config
{
  HC_SR04::Config ultrasonic{};
};

XRobotModules(LibXR::HardwareContainer& hw,
              LibXR::ApplicationManager& app,
              const HC_SR04::Resources& ultrasonic_resources);
XRobotModules(LibXR::HardwareContainer& hw,
              LibXR::ApplicationManager& app,
              const HC_SR04::Resources& ultrasonic_resources,
              const Config& config);

HC_SR04& UltrasonicModule();
const HC_SR04& UltrasonicModule() const;
```

不保留缺少 `HC_SR04::Resources` 的旧构造入口，避免生成未绑定板级资源的装配器。

## 数据流

```text
SysConfig PWM/Capture -> HC_SR04 -----> Topic<HC_SR04::Sample> --------┐
I2C_0 ----------------> MPU6050 -----> Topic<MPU6050::Sample> ---------┤
NRF GPIO -------------> NRF24L01 ---> Topic<RxPacket/Status> ----------┤
Grey/Encoder ----------> Tracking/Encoder Topics ----------------------┤
                                                                       v
ApplicationManager::MonitorAll() -> app_main 异步订阅 -> Ozone 完整快照
```

文件作用域调试对象为：

- `hc_sr04_sample`：最近一次有效、越界或无回波测距样本。
- `mpu6050_sample`：校准完成后最近一次完整六轴与姿态样本。
- `nrf24l01_rx_packet`：最近一次收到的固定宽度无线数据包。
- `nrf24l01_status`：最近一次无线状态机、发送结果和累计计数快照。

每个订阅者在 `Available()` 后执行一次 `GetData()`，复制完成后立即调用
`StartWaiting()`。没有新数据时保留旧快照，模块异常不得阻塞主循环。

## 验证设计

- 结构测试约束统一所有权、HC-SR04 资源接线、四个完整快照和订阅重挂。
- NRF24L01 文档契约使用 ASCII `\uXXXX` 正则匹配中文语义，兼容 Windows
  PowerShell 5.1 对 UTF-8 无 BOM 脚本的解析限制。
- 主机逻辑测试覆盖 HC-SR04、NRF24L01 和控制 Topic 行为。
- ARM 完整构建验证类型、符号和链接；Flash 门禁限制 `.bin` 不超过
  `128450` 字节。
- `MSPM0_PWM_INIT` 从 Timer 当前分频配置恢复源时钟，避免 `SetConfig()` 重写
  分频后把 16 Hz Trigger 误配置为更高频率。
- 实车与 Ozone 验证需要连接调试探针及对应传感器，不作为无硬件环境下的自动测试。
