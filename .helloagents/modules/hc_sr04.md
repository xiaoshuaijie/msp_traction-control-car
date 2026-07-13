# HC_SR04

## 职责

`HC_SR04` 是一个 `LibXR::Application`。构造阶段通过 `MSPM0PWM` 配置 Trigger，
运行时由 `ApplicationManager::MonitorAll()` 调用 `OnMonitor()`，轮询 Timer Capture
原始事件、换算 Echo 脉宽并向 `hc_sr04` Topic 发布距离样本。

## 接口定义

### 公共 API

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `HC_SR04(app, resources)` | 构造函数 | 使用默认测量配置并注册应用 |
| `HC_SR04(app, resources, config)` | 构造函数 | 使用指定资源和配置 |
| `SampleTopic()` | `LibXR::Topic&` | 返回距离样本 Topic |
| `GetLatestSample()` | `const Sample&` | 返回最近一次已发布样本 |
| `HasValidSample()` | `bool` | 是否至少获得过一次有效距离 |
| `OnMonitor()` | `void` | 处理新 Capture 或发布无回波状态 |

### `Resources`

| 字段 | 类型 | 说明 |
|------|------|------|
| `trigger_pwm` | `LibXR::MSPM0PWM::Resources` | Trigger PWM 定时器、通道和分频前源时钟 |
| `capture_timer` | `GPTIMER_Regs*` | Echo Capture Timer |
| `capture_channel` | `DL_TIMER_CC_INDEX` | Echo Capture/Compare 通道 |
| `capture_event_mask` | `uint32_t` | 与通道精确对应的 `CCx_UP_EVENT` |
| `capture_clock_hz` | `uint32_t` | `PULSE_WIDTH_UP` Capture 实际计数频率，当前为 1 MHz |

### `Sample`

| 字段 | 类型 | 说明 |
|------|------|------|
| `pulse_width_us` | `uint32_t` | Echo 高电平宽度 |
| `raw_distance_mm` | `float` | 按 `0.17 mm/us` 换算的原始距离 |
| `filtered_distance_mm` | `float` | 最近有效距离的 EMA 结果 |
| `status` | `Status` | `VALID`、`NO_ECHO`、`BELOW_MIN` 或 `ABOVE_MAX` |
| `sequence` | `uint32_t` | Topic 发布序号 |

## 行为规范

### 初始化

**条件**: 资源指针、时钟、通道/事件映射和配置均合法。

**行为**: 停止 PWM，配置默认约 16 Hz/10 us 信号，清除旧 Capture 事件，启动
Capture Timer，再启用 PWM 并注册到 `ApplicationManager`。

**结果**: SysConfig 初始化期间不会输出旧 5 Hz/50% Trigger 波形。

### 有效回波

**条件**: 对应 `CCx_UP_EVENT` 置位且换算距离位于 20 至 4000 mm。

**行为**: 首个有效值直接初始化滤波器，后续使用
`filtered = old * 0.6 + raw * 0.4`，然后发布 `VALID` 样本。

**结果**: Topic 获得递增 `sequence` 的原始距离和滤波距离。

### 无效回波与超时

**条件**: 距离低于/高于有效范围，或超过 100 ms 没有新 Capture。

**行为**: 发布对应状态；无效帧不更新 EMA，`NO_ECHO` 按 Trigger 周期节流发布。

**结果**: `filtered_distance_mm` 保持最近有效值，订阅者通过 `status` 判定可用性。

## 依赖关系

```yaml
依赖:
  - LibXR::ApplicationManager
  - LibXR::Topic
  - LibXR::MSPM0PWM
  - TI MSPM0 DriverLib Timer API
被依赖:
  - XRobotModules 统一持有并注册
  - app_main.cpp 组装 PWM/Capture 资源并订阅测距 Topic
```

## 验证状态

- 已接入 `XRobotModules`，最近一次完整样本通过 `hc_sr04_sample` 供 Ozone 观察。
- 主机距离逻辑测试、静态集成检查、`xr` 构建和完整固件构建均已通过。
- `MSPM0_PWM_INIT` 会从 Timer 当前分频配置恢复源时钟，HC-SR04 的 16 Hz
  Trigger 不依赖 SysConfig 初始 prescale 保持不变。
- 真实 HC-SR04 传感器的 Echo 波形、距离精度和环境温度偏差尚未上板验证。

## 源码维护约定

- `sr04.h` 的 `MODULE MANIFEST V2` 由模块工具读取，键名、层级和取值必须保持机器可读。
- 公共接口与字段注释应明确生命周期、硬件资源约束、物理单位和无效样本语义。
- `sr04.cpp` 的注释重点记录 PWM 参数推导、Capture 启动顺序、事件优先级和无符号时间回绕。
- `sr04_math.hpp` 的注释与 `0.17 mm/us` 距离公式、闭区间量程判定和 EMA 行为保持一致。
