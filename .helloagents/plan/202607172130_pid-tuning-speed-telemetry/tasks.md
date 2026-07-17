# 任务清单: pid-tuning-speed-telemetry

```yaml
@feature: pid-tuning-speed-telemetry
@created: 2026-07-17
@status: pending
@mode: R2
```

## 进度概览

| 完成 | 失败 | 跳过 | 总数 |
|------|------|------|------|
| 0 | 0 | 0 | 5 |

---

## 任务列表

### 1. 协议与 Console 测试

- [ ] 1.1 修改 `tests/pid_tuning_protocol_test.cpp`，以失败测试约束 12 路、52 字节和新字段顺序 | depends_on: []
- [ ] 1.2 修改 `tests/pid_tuning_console_test.cpp` 与测试 fake，以失败测试约束速度映射、20 ms 周期和忙时重试 | depends_on: []

### 2. 协议与发送实现

- [ ] 2.1 修改 `src/pid_tuning_console.hpp/.cpp`，实现 12 路快照和 50 Hz 非阻塞调度 | depends_on: [1.1, 1.2]
- [ ] 2.2 修改 `src/app_main.cpp`，将现有 `now` 传给 `Console::Poll` | depends_on: [2.1]

### 3. 集成验证与文档同步

- [ ] 3.1 更新 JustFloat 集成检查和 PidTuning 知识库，运行主机测试、集成检查及 ARM 固件构建 | depends_on: [2.2]

---

## 执行日志

| 时间 | 任务 | 状态 | 备注 |
|------|------|------|------|

---

## 执行备注

- 用户确认四轮实测速度分别发送，自动发送周期为 20 ms。
- `i_limit/out_limit` 仅从上行快照移除，下行调参能力保持不变。
- 工作区已有大量其他改动；本任务不得回退或覆盖无关文件。
