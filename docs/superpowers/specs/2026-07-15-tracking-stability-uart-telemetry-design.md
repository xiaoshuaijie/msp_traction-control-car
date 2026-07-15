# 循迹稳定性修复与 UART 遥测设计

## 1. 背景与目标

小车当前存在两类循迹异常：部分运行无法进入正确循迹状态，以及运行中偶发某次转向失败。代码检查和项目历史排障记录确认了两个独立但会叠加的问题：

- 灰度传感器输入使用上拉，黑线对应低电平，但当前配置为高电平有效，导致黑白判断反相。
- K2 的目标轮速约为 `15.625 rad/s`，现有四路速度前馈在约 `8.5-12.3 rad/s` 已达到 `1.0` 输出上限。中小循迹误差下，左右目标轮速不同但最终 PWM 同时饱和，实际转向差速被抹掉。

本次变更同时修复这两个根因，并通过 `UART_0_INST` 输出足以还原传感、循迹和电机闭环状态的遥测数据，供实车复测后继续调参。

## 2. 范围

### 包含

- 将灰度传感器有效极性恢复为低电平有效。
- 限制速度前馈占用的最大输出，给 PID 正负纠偏保留余量。
- 保留现有 Tracking 加权误差和丢线恢复流程，不回退已完成的入弯首帧恢复修复。
- 使用现有 `UART_0_INST`、`115200 8N1` 配置输出 CSV 遥测。
- 增加覆盖灰度极性、前馈余量和遥测编码/缓冲行为的主机回归测试。

### 不包含

- 不修改 `sysconfig/ti_msp_dl_config.c`、`sysconfig/ti_msp_dl_config.h` 等生成文件。
- 不在缺少实车数据时继续猜测 PID、轮速前馈或 Tracking 增益的最终标定值。
- 不修改任务圈数、无线通信、编码器方向和电机接线映射。

## 3. 控制修复

### 3.1 灰度有效极性

`CarControlSupport::kGreySensorActiveLow` 设置为 `true`。GPIO 继续使用上拉输入，`GreySensor` 仍统一输出 `active_mask`，其中 bit 为 1 表示该路检测到黑线。

回归测试直接断言装车配置为低电平有效，防止以后模块迁移时再次反相。

### 3.2 速度前馈余量

保留当前每个电机的静态前馈和速度增益，但在前馈函数内部增加独立的绝对值上限。推荐初始上限为 `0.65`：

```text
feedforward = sign(target) * min(static + gain * abs(target), 0.65)
motor_output = clamp(feedforward + pid_correction, -1.0, 1.0)
```

该上限不会限制最终电机输出，PID 仍可把最终输出推到 `1.0`；它只防止前馈单独占满执行器，使目标轮速降低时 PID 能立即产生负纠偏，从而恢复实际左右差速。

测试覆盖 K2 基础轮速下四路前馈均不超过 `0.65`，并确认最终输出钳位仍保持 `[-1, 1]`。

## 4. UART 遥测

### 4.1 发送策略

- 采样周期：`50 ms`，即 `20 Hz`。
- 串口：复用 SysConfig 已初始化的 `UART_0_INST`，波特率 `115200`。
- 主循环只把完整帧写入固定容量环形缓冲。
- 每轮主循环仅在 UART TX FIFO 有空间时发送待发字节，不等待 FIFO，不调用阻塞发送 API。
- 缓冲区无法容纳完整新帧时丢弃该帧，并递增 `dropped_frames`；绝不发送半帧。
- 不使用动态内存，不在中断中格式化数据。

### 4.2 CSV 帧

每行以 `TC` 开头，以 `\r\n` 结束。浮点量按定点整数发送，避免嵌入式浮点格式化开销：

```text
TC,time_ms,raw_mask,active_mask,changed_mask,position,lost_side,lost_count,
error_milli,left_target_centi,right_target_centi,
target0_centi..target3_centi,measured0_centi..measured3_centi,
output0_milli..output3_milli,source_seq,tracking_seq,ready_flags,dropped_frames
```

缩放规则：

- `error_milli = error * 1000`
- 轮速字段使用 `rad/s * 100`
- 电机输出使用 `output * 1000`
- `ready_flags`：bit0 为 Grey Topic 新鲜，bit1 为 Tracking Topic 新鲜，bit2 为 Encoder Topic 新鲜，bit3 为当前丢线

该帧能够判断：黑白极性是否正确、通道位序是否正确、丢线发生在哪一侧、Tracking 是否及时给出差速、速度环是否饱和、Topic 是否掉帧，以及 UART 自身是否丢帧。

## 5. 代码结构

- `src/car_control_support.hpp/.cpp`：灰度极性常量和速度前馈上限。
- `src/tracking_diagnostics.hpp`：可在主机测试中运行的定点 CSV 编码与固定容量发送队列。
- `src/app_main.cpp`：每 50 ms 组装遥测快照，并使用 `DL_UART_Main_isTXFIFOFull` / `DL_UART_Main_transmitData` 非阻塞排空队列。
- `tests/leader_mission_logic_test.cpp`：增加控制配置与遥测回归测试，沿用现有主机测试入口。

预计只修改上述 5 个文件，不扩大到 SysConfig 生成物或其它业务模块。

## 6. 测试与验收

按测试先行顺序执行：

- 先增加灰度低电平有效断言，确认当前代码失败。
- 增加 K2 前馈不得独占满输出的测试，确认当前代码失败。
- 增加 CSV 字段、定点缩放、完整帧入队和满缓冲整帧丢弃测试，确认实现前失败。
- 实施最小代码使测试通过。
- 运行现有 leader mission、Topic 模块和 XRobot 模块管理测试。
- 完整构建固件，并检查 Flash 预算。

实车验收分两步：

- 新固件应能稳定识别白底/黑线，并在 K2 转向时产生可见的左右轮输出差。
- 用户保存一轮正常循迹和一轮失败附近的 UART CSV；后续依据数据判断是否需要调整每轮前馈、PID 或 Tracking 增益。

## 7. 风险与控制

- 前馈上限可能改变达到目标速度的瞬态。PID 会补足所需输出，UART 数据用于确认稳态是否达到目标。
- UART 记录过多可能影响控制周期。20 Hz 限频和非阻塞队列确保控制优先，缓冲不足时主动丢日志。
- 若实车灰度板与历史记录不一致，UART 的 `raw_mask` 与 `active_mask` 可以立即识别，恢复只需修改单个配置常量。

