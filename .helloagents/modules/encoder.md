# Encoder 模块

## 职责

`Module::Encoder` 聚合四路正交编码器，以 GPIO 双边沿中断进行四倍频计数，
并由 `ApplicationManager::MonitorAll()` 周期调用 `OnMonitor()` 发布一致的四轮快照。

## 接口

- Topic：`encoder`
- 消息：`Module::Encoder::Sample`
- 默认周期：5 ms
- 默认分辨率：1024 counts/rev
- 数据：四轮 `count`、`angle_rad`、`speed_rad_s`、采样时间和序号

四轮速度使用同一个微秒时间差计算。首帧、`ResetAll()` 后首帧以及无效时间参数
均输出零速度。计数使用无符号模 2^32 累加，主循环在中断临界区读取一致快照。

## 生命周期

构造函数完成 GPIO 初始化、Topic 创建以及向 `HardwareContainer` 和
`ApplicationManager` 注册。对象必须覆盖所有 `MonitorAll()` 调用；析构时关闭八路
编码器 GPIO 中断。

## 源文件导航

- `module/encoder/encoder.hpp`：定义正交解码器、四轮聚合应用、配置项和 Topic 样本契约。
- `module/encoder/encoder.cpp`：实现 GPIO 中断解码、临界区快照、周期采样和 Topic 发布。
- `module/encoder/encoder_math.hpp`：提供硬件无关的角度/角速度换算及计数回绕处理。
- `module/encoder/CMakeLists.txt`：把公开头文件路径和 `encoder.cpp` 接入 `xr` 目标。

上述文件使用中文 Doxygen 与关键行内注释说明数据单位、对象生命周期、中断安全、
模 `2^32` 计数和首帧速度语义；维护时仍应以实际代码行为为准。

## 验证

- `tests/run_topic_module_logic_tests.ps1`
- `tests/verify_topic_module_structure.ps1`
- ARM 固件完整构建
