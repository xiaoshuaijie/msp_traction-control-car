# PID 调参轮速遥测设计

## 背景

现有 `PidTuning::Console` 使用 VOFA+ JustFloat 发送 8 路配置快照，但不包含
速度控制器的目标值和编码器反馈，也不会自动周期发送。这样只能确认参数配置，
无法在 VOFA+ 中连续比较目标轮速与实际轮速。

本次扩展保持现有 ASCII 下行命令和 UART 非阻塞发送链路不变，将上行快照调整为
12 路，并以 20 ms 周期连续发送。

## 目标

- 从上行快照移除 `i_limit` 和 `out_limit`。
- 保留 `pid,set,i_limit,<value>` 和 `pid,set,out_limit,<value>` 下行调参能力。
- 增加左右轮目标速度和四轮编码器实际轮速。
- 以 20 ms 周期发送 JustFloat 帧，即 50 Hz。
- 保留 `pid,get` 立即请求快照的能力。
- UART 暂时繁忙时不阻塞主控制循环，并在后续轮询中重试。

## 通道协议

每帧包含 12 个 32 位小端 IEEE-754 `float`，之后追加 JustFloat 帧尾
`00 00 80 7F`，总长度为 52 字节。

| 通道 | 字段 | 数据来源 | 单位 |
|------|------|----------|------|
| I0 | `velocity_gain` | `speed_feedforward_config[0]` | 输出/(rad/s) |
| I1 | `static_output` | `speed_feedforward_config[0]` | 归一化输出 |
| I2 | `p` | `speed_pid_config[0]` | — |
| I3 | `i` | `speed_pid_config[0]` | — |
| I4 | `turn_kp` | `Tracking::Config` | — |
| I5 | `turn_kd` | `Tracking::Config` | — |
| I6 | `left_target` | `car_control_sample.target_speed[0]` | rad/s |
| I7 | `right_target` | `car_control_sample.target_speed[1]` | rad/s |
| I8 | `front_left_measured` | `car_control_sample.measured_speed[0]` | rad/s |
| I9 | `front_right_measured` | `car_control_sample.measured_speed[1]` | rad/s |
| I10 | `back_left_measured` | `car_control_sample.measured_speed[2]` | rad/s |
| I11 | `back_right_measured` | `car_control_sample.measured_speed[3]` | rad/s |

`car_control_sample.measured_speed` 已应用 `kEncoderDirection`，因此发送给 VOFA+
的是与目标速度符号约定一致的实际轮速，不重复进行方向修正。

## 架构与数据流

主循环已经取得统一的毫秒时间 `now`，因此将 `Console::Poll()` 改为
`Console::Poll(uint32_t now_ms)`，避免 Console 再依赖全局时钟，也便于主机测试
精确控制 19 ms、20 ms 等时间边界。

数据流如下：

```text
Tracking / DriveMode ──> car_control_sample.target_speed
Encoder ───────────────> car_control_sample.measured_speed
PID/前馈配置 ──────────> SnapshotValues[0..5]
目标/反馈速度 ─────────> SnapshotValues[6..11]
SnapshotValues ────────> 52-byte JustFloat frame
                       └─> LibXR::UART::Write
```

Console 继续直接读取 `car_control_sample` 全局快照。该实现与其当前直接读取
`speed_pid_config` 和 `speed_feedforward_config` 的方式一致，避免为了单一读取者
扩大构造接口和主控制模块改动范围。

## 调度与错误处理

- 自动发送周期固定为 `20U` ms。
- 使用 `static_cast<uint32_t>(now_ms - last_snapshot_time_ms_)` 判断周期，
  保持毫秒计数器回绕安全。
- `pid,get` 将 `snapshot_pending_` 置位，可在周期尚未到期时请求一帧。
- 周期到期同样置位 `snapshot_pending_`。
- WritePort 空间不足，或 `uart_.Write()` 返回 `BUSY`、`FULL` 或其他错误时，
  保留 `snapshot_pending_`，下一次 `Poll()` 继续尝试。
- 只有完整 52 字节帧成功入队后，才清除 pending 并更新
  `last_snapshot_time_ms_`。
- 调参命令的参数白名单、范围校验和热更新行为保持不变。

## 测试设计

先修改测试并确认失败，再实现生产代码。

### 协议测试

- `kSnapshotValueCount == 12`。
- `kJustFloatFrameSize == 52`。
- 12 个浮点数按固定顺序无损编码。
- 帧尾位于字节偏移 48，内容为 `00 00 80 7F`。

### Console 测试

- GET 快照不再包含 `i_limit/out_limit`。
- I0～I11 与配置、左右目标和四轮实测值一一对应。
- 19 ms 时不自动发送，20 ms 时发送第一帧，后续每 20 ms 发送。
- WritePort 满、`BUSY` 和 `FULL` 时保留待发状态，恢复后只成功发送一次。
- 现有 SET 命令仍可修改 `i_limit/out_limit`。

### 集成验证

- 更新 JustFloat 源码契约检查中的通道数、帧长和字段顺序。
- 运行协议测试、Console 测试和集成检查。
- 使用启用 `MSP_ENABLE_PID_TUNING_CONSOLE` 的配置完成 ARM 固件构建。

## 非目标

- 不删除 `i_limit/out_limit` 参数或对应 SET 命令。
- 不修改 `libxr/driver/mspm0/mspm0_uart.cpp`。
- 不改变编码器采样、速度 PID 或 Tracking 算法。
- 不新增另一种二进制下行调参协议。
