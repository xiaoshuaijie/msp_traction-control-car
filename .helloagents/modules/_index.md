# 模块索引

> 通过此文件快速定位已有模块文档。

## 模块清单

| 模块 | 职责 | 状态 | 文档 |
|------|------|------|------|
| HC_SR04 | 生成 Trigger、捕获 Echo 并发布距离样本 | 🚧 | [hc_sr04.md](./hc_sr04.md) |
| Encoder | 发布四轮计数、角度和速度一致快照 | 🚧 | [encoder.md](./encoder.md) |
| NRF24L01 | 通过三个 Topic 非阻塞收发固定载荷 | 🚧 | [nrf24l01.md](./nrf24l01.md) |
| XRobotModules | 统一装配六个 Topic Application | 🚧 | [xrobot_modules.md](./xrobot_modules.md) |

## 模块依赖关系

```text
TI DriverLib Timer Capture
           ↓
LibXR MSPM0PWM → HC_SR04 → Topic<HC_SR04::Sample>

GPIO IRQ → Encoder → Topic<Encoder::Sample>
NRF24L01 Tx Topic → NRF24L01 → Rx/Status Topic
GreySensor → Tracking ┐
MPU6050 ──────────────┼→ XRobotModules → ApplicationManager::MonitorAll()
Encoder + NRF24L01 ───┤
HC_SR04 ──────────────┘
```

## 源文件入口

| 模块 | 主要入口 |
|------|---------|
| HC_SR04 | `module/HC_SR04/sr04.h`、`sr04.cpp`、`sr04_math.hpp`、`CMakeLists.txt` |
| Encoder | `module/encoder/encoder.hpp`、`encoder.cpp`、`encoder_math.hpp`、`CMakeLists.txt` |

## 状态说明

- ✅ 稳定
- 🚧 开发中或待硬件验证
- 📝 规划中
