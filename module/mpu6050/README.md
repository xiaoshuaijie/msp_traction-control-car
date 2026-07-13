# MPU6050

MPU6050 六轴 IMU 的 LibXR 应用模块。模块通过 `LibXR::I2C` 访问芯片，
不直接持有 STM32 HAL 句柄；初始化、Z 轴陀螺零漂校准、姿态解算和 Topic
发布均由 `ApplicationManager::MonitorAll()` 周期驱动。

## 硬件要求

- 默认 7-bit I2C 地址：`0x68`
- I2C 时钟：`400000 Hz`
- 硬件别名：`i2c_mpu6050`、`imu`、`i2c2` 中任意一个

当前工程将 MPU6050 与 JY61P 并行挂在 I2C2 上，二者使用不同 I2C 地址。

## 构造与调度

```cpp
HardwareContainer hardware(
    Entry<I2C>{i2c2, {"i2c_jy61p", "i2c_mpu6050", "imu", "i2c2"}});
ApplicationManager app_manager;
MPU6050 mpu6050(hardware, app_manager);

while (true) {
  app_manager.MonitorAll();
  Thread::Sleep(1);
}
```

默认配置：

- Topic 名称：`mpu6050`
- 发布周期：`10 ms`
- 采样率：`200 Hz`
- 低通滤波：`5 Hz`
- 陀螺量程：`+-250 deg/s`
- 加速度量程：`+-2 g`
- Z 轴零漂校准：`200` 个样本，默认约 `2 s`

未初始化或通信失败时，模块不会发布半成品样本；`OnMonitor()` 会按恢复周期重试。

## Topic 数据

```cpp
Topic::ASyncSubscriber<MPU6050::Sample> subscriber("mpu6050");
subscriber.StartWaiting();

if (subscriber.Available()) {
  const auto& sample = subscriber.GetData();
  // sample.angle.roll / pitch / yaw: degree
  subscriber.StartWaiting();
}
```

`MPU6050::Sample` 字段：

| 字段 | 单位/含义 |
| --- | --- |
| `acceleration` | `m/s^2`，按配置加速度量程换算 |
| `angular_velocity` | `deg/s`，Z 轴已扣除上电校准零漂 |
| `angle` | `roll/pitch/yaw`，单位 degree |
| `quaternion` | 姿态融合内部四元数 |
| `temperature` | degree Celsius，按 `raw / 340 + 36.53` 换算 |
| `sequence` | 每次成功发布后递增 |
| `calibration_count` | 当前已累计的校准样本数 |
| `calibrated` | 零漂校准是否完成 |

异步订阅者读取数据后必须再次调用 `StartWaiting()`，否则后续发布不会进入本地缓冲区。

## 状态查询

- `Probe()`：读取并校验 `WHO_AM_I`，期望值为 `0x68`。
- `LastError()`：最近一次初始化、校准或采样的 LibXR 错误码。
- `ConsecutiveFailures()`：连续失败次数，成功后清零。
- `HasValidSample()`：是否至少成功发布过一次样本。
- `IsCalibrated()`：Z 轴零漂校准是否完成。
- `CalibrationCount()`：当前校准累计样本数。

`User/app_main.cpp` 中额外导出 `mpu6050_ozone_data`，便于 Ozone Watch 或 Live
Watch 观察加速度、角速度、姿态、温度、校准进度和健康状态。LED 健康指示仍沿用
现有 JY61P 逻辑，避免 MPU6050 未连接时影响原有调试。
