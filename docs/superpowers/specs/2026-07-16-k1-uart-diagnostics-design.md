# K1 速度闭环串口诊断设计

## 1. 背景

K1 的名义车体速度为 `0.3 m/s`，实测约为 `0.38 m/s`；K2 的名义速度为
`0.5 m/s`，实测约为 `0.52 m/s`。当前 UART 遥测能输出四轮目标速度、实测
速度和最终电机输出，但缺少前馈、PI、编码器累计计数和任务生命周期信息，无法
判断偏差主要来自编码器/轮径标定、前馈饱和还是 PI 参数。

## 2. 目标

- 按下 K1 后自动开始采集，无需额外调试命令。
- 以 `50 ms` 周期输出可直接保存为文本的 CSV 数据。
- 覆盖启动、循迹、完成制动以及完成后的 `2 s` 静止观察窗口。
- 输出足以分离编码器标定、轮径、前馈、PI 和单轮差异。
- 复用 UART0 `115200 8N1` 和现有非阻塞发送机制，不阻塞 `5 ms` 控制环。
- 数据编码和生命周期判断可通过主机测试验证。

## 3. 非目标

- 本次不修改 `counts_per_rev`、轮径、前馈或 PI 参数。
- 本次不实现串口接收命令、二进制协议或上位机程序。
- 本次不改变 K1/K2/K3/K4 的任务控制行为。
- 本次不为 K2、K3、K4 增加对应诊断会话。

## 4. 总体方案

新增独立的 K1 诊断帧，前缀为 `KD`。K1 诊断会话启用期间暂停周期性的
`TC` 帧，复用同一发送队列和 UART 排空逻辑；会话结束后恢复原 `TC` 帧。
这样既不改变 `TC` 字段格式，也避免两种高频帧同时发送挤占 115200 波特率。

诊断编码逻辑放在可由主机测试编译的头文件中，`app_main.cpp` 只负责：

- 响应 K1 按键并启动诊断会话；
- 从现有快照组装诊断帧；
- 每 `50 ms` 将帧压入非阻塞队列；
- 按任务状态推进结束后的 `2 s` 窗口；
- 在 UART TX FIFO 可写时逐字节排空队列。

## 5. 会话生命周期

### 5.1 启动

收到 K1 的 `PRESSED` 事件并完成任务状态复位后：

- 设置诊断会话为活动状态；
- 记录 `start_time_ms = now`；
- 清除此前的结束窗口状态；
- 将一行 `KD_HEADER` 压入发送队列；
- 下一次 `50 ms` 采样周期开始输出 `KD`。

如果诊断会话期间再次按下 K1，则重新开始一个会话并再次发送表头。按下 K2、K3
或 K4 会立即结束当前 K1 诊断会话，避免把其他任务的数据混入 K1 数据集。

### 5.2 运行

诊断会话活动期间持续输出：任务为 `kRunning` 时记录正常行驶数据，进入终态后
按 5.3 节继续记录结束窗口。采样使用无符号时间差，保证 `uint32_t` 毫秒计时器
回绕时仍然正确。

### 5.3 结束窗口

任务首次进入以下任一终态时记录 `terminal_time_ms`：

- `LeaderMission::State::kFinished`；
- `LeaderMission::State::kFaultStop`。

终态后继续输出 `2000 ms`。达到时间后停止 `KD`，恢复 `TC`。最后一帧必须包含
终态和接近零的目标/输出信息，以便判断制动后编码器是否仍有运动。

## 6. CSV 协议

### 6.1 表头

每次 K1 会话发送一次：

```text
KD_HEADER,relative_ms,mission_state,drive_action,completed_laps,target_laps,distance_mm,cross_line,line_error_milli,active_mask,ready_flags,enc_fl,enc_fr,enc_bl,enc_br,target_fl_centi,target_fr_centi,target_bl_centi,target_br_centi,measured_fl_centi,measured_fr_centi,measured_bl_centi,measured_br_centi,ff_fl_milli,ff_fr_milli,ff_bl_milli,ff_br_milli,pi_fl_milli,pi_fr_milli,pi_bl_milli,pi_br_milli,output_fl_milli,output_fr_milli,output_bl_milli,output_br_milli,encoder_sequence,dropped_frames
```

### 6.2 数据行

```text
KD,<relative_ms>,<mission_state>,<drive_action>,...,<dropped_frames>\r\n
```

字段顺序固定，电机顺序沿用 `Module::Encoder::MotorId`：

1. `front_left`
2. `front_right`
3. `back_left`
4. `back_right`

### 6.3 缩放与单位

| 字段 | 编码规则 | 还原方式 |
|---|---:|---:|
| `relative_ms` | 原始毫秒 | 无需换算 |
| `distance_mm` | `distance_m * 1000` | 除以 `1000` 得米 |
| `line_error_milli` | `tracking_error * 1000` | 除以 `1000` |
| `target_*_centi` | `rad/s * 100` | 除以 `100` 得 `rad/s` |
| `measured_*_centi` | `rad/s * 100` | 除以 `100` 得 `rad/s` |
| `ff_*_milli` | 前馈输出 `* 1000` | 除以 `1000` |
| `pi_*_milli` | PI 输出 `* 1000` | 除以 `1000` |
| `output_*_milli` | 最终电机输出 `* 1000` | 除以 `1000` |
| `enc_*` | 四倍频累计计数 | 无需换算 |

所有字段使用十进制整数编码，避免在 MCU 上引入浮点字符串格式化。

`ready_flags` 沿用现有位定义：

- bit0：Grey Topic 新鲜；
- bit1：Tracking Topic 新鲜；
- bit2：Encoder Topic 新鲜；
- bit3：当前报告丢线。

## 7. 队列、带宽与错误处理

- UART 发送仍使用固定容量字节队列和 TX FIFO 非阻塞排空。
- 单帧编码使用固定数组，不进行堆分配。
- 编码失败或队列空间不足时丢弃整帧，不发送半行 CSV。
- `dropped_frames` 单调递增并写入后续成功帧。
- `KD_HEADER` 也必须作为完整行入队；空间不足时延后重试，表头成功入队前不发送
  `KD` 数据，确保采集文件可解析。
- K1 会话期间暂停 `TC`，预期 `KD` 每秒 20 帧，115200 波特率保留足够余量。
- UART 堵塞不得改变控制周期、任务状态或电机输出。

## 8. 代码边界

预计修改范围：

- `src/tracking_diagnostics.hpp`：增加 K1 诊断帧、会话状态和 CSV 编码；复用现有
  固定容量队列基础能力。
- `src/app_main.cpp`：连接 K1 按键、任务终态、控制快照与诊断帧。
- `tests/leader_mission_logic_test.cpp`：增加会话生命周期和 CSV 编码测试。
- 不修改硬件生成文件、UART 配置或其他任务模块。

## 9. 测试设计

主机测试至少覆盖：

- K1 启动时只发送一次 `KD_HEADER`；
- 表头成功前不发送数据行；
- `KD` 字段顺序、四轮顺序、符号和缩放正确；
- 运行期间按 `50 ms` 周期采样；
- `kFinished` 和 `kFaultStop` 后均延续 `2000 ms`；
- 毫秒计时器跨 `UINT32_MAX` 回绕时结束窗口正确；
- K1 再次按下会重启会话；K2/K3/K4 会取消 K1 会话；
- 队列不足时只增加丢帧计数，不输出半帧；
- K1 诊断活动时不产生 `TC`，结束后恢复 `TC`；
- 现有 leader mission、tracking 和固件体积检查继续通过。

## 10. 验收标准

- 串口工具以 `115200 8N1` 保存一次完整 K1 运行，文件首行是 `KD_HEADER`。
- 从 K1 按下到任务完成后约 `2 s` 均有连续 `KD` 数据。
- 正常情况下采样间隔约 `50 ms`，`dropped_frames` 保持为 `0`。
- 文件能直接用于比较四轮目标、实测、前馈、PI 和最终输出。
- 诊断启用前后 K1 的控制周期、循迹行为和停车行为没有可观察变化。
