# 结构化控制调试快照设计

## 目标

用 `GreySensor::Sample`、`Tracking::Output`、`Module::Encoder::Sample`、`BitsButtonXR::ButtonEventResult` 和一个新的 `CarControlSample` 替代 `car_control_support` 中散落的调试镜像，使 Ozone 能按控制链路展开完整状态。

## 边界

- 保留 `speed_pid_config`、`speed_feedforward_config` 和 `speed_pid`，因为它们是可调控制配置及运行状态。
- 删除所有只由 `app_main.cpp` 写入、没有业务消费者的 `volatile g_*` 调试变量。
- 不改变 PID 参数、Topic 新鲜度阈值、电机方向、循迹参数或按键行为。
- 不把聚合对象声明为 `volatile`；完整结构赋值需要普通对象限定符。

## 结构

`app_main.cpp` 文件作用域定义：

```cpp
GreySensor::Sample grey_sensor_sample{};
Tracking::Output tracking_output{};
Module::Encoder::Sample encoder_sample{};
BitsButtonXR::ButtonEventResult button_event{};
CarControlSupport::CarControlSample car_control_sample{};
```

`CarControlSample` 聚合控制周期、驾驶模式、Topic 新鲜度、灰度极性、综合失线状态、前进距离，以及四路目标速度、测量速度、前馈、PID 修正和最终电机输出。

## 数据流

- GreySensor 新增独立异步订阅器，只更新 `grey_sensor_sample`，不直接调用会改变传感器内部记忆的 `Read()`。
- Tracking 和 Encoder 继续通过原有异步订阅器更新 `tracking_output`、`encoder_sample`，并继续作为控制输入。
- Button 队列直接写入全局 `button_event`；控制内部仍临时解析按键索引。
- 主控制周期一次性更新 `car_control_sample` 的时间、状态和四轮数组。

## 失败保护

Topic 新鲜度仍由 `ControlTopicLogic::IsFresh()` 判定。Encoder 陈旧时 PID 不运行，Tracking 陈旧时循迹目标归零；`car_control_sample.line_lost` 继续记录“Topic 陈旧或模块报告丢线”的综合结果。

## 验证

- 结构脚本先失败，证明旧约束尚未迁移；源码修改后通过。
- Topic 逻辑主机测试继续覆盖时间回绕、新鲜度、缓存失效和门控。
- ARM 固件构建验证 C++ 类型、链接和嵌入式编译器兼容性。
- Flash 预算检查保证新增订阅器和结构没有突破固件上限。
