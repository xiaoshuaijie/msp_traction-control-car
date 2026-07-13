# msp_traction-control-car 知识库

> 本文件是项目知识库的入口点。

## 快速导航

| 需要了解 | 读取文件 |
|---------|---------|
| 项目概况、技术栈、开发约定 | [context.md](context.md) |
| 模块索引 | [modules/_index.md](modules/_index.md) |
| HC-SR04 模块职责和接口 | [modules/hc_sr04.md](modules/hc_sr04.md) |
| Encoder Topic 接口 | [modules/encoder.md](modules/encoder.md) |
| NRF24L01 Topic 协议 | [modules/nrf24l01.md](modules/nrf24l01.md) |
| Topic 模块装配关系 | [modules/xrobot_modules.md](modules/xrobot_modules.md) |
| 项目变更历史 | [CHANGELOG.md](CHANGELOG.md) |
| 历史方案索引 | [archive/_index.md](archive/_index.md) |

## 模块关键词索引

| 模块 | 关键词 | 摘要 |
|------|--------|------|
| HC_SR04 | ultrasonic, PWM, Capture, Topic, EMA | 使用 PWM Trigger 与脉宽 Capture 发布超声波距离样本 |
| Encoder | quadrature, GPIO IRQ, Topic, speed | 周期发布四轮一致运动学快照 |
| NRF24L01 | radio, software SPI, Topic, state machine | 非阻塞收发与错误恢复 |
| XRobotModules | ApplicationManager, composition, freshness | 集中装配 Topic 模块并保护控制输入新鲜度 |

## 知识库状态

```yaml
kb_version: 2.3.7
最后更新: 2026-07-13
模块数量: 4
待执行方案: 0
```

## 读取指引

- 涉及 HC-SR04、超声波测距或避障输入时，读取 `modules/hc_sr04.md`。
- 涉及编码器、无线通信或 Topic 模块装配时，读取对应模块文档。
- 需要实现决策和测试步骤时，读取 `../docs/superpowers/specs/2026-07-13-hc-sr04-design.md` 与对应实施计划。
- 代码与文档不一致时，以代码为准并同步更新本知识库。
