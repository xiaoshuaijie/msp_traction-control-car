#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "app_framework.hpp"
#include "gpio.hpp"

/**
 * @file nrf24l01_bus.hpp
 * @brief NRF24L01 的 GPIO 模拟 SPI 总线及寄存器/FIFO 访问接口。
 *
 * 本类直接同步翻转 GPIO 完成 SPI Mode 0、MSB-first 传输，不使用硬件 SPI、
 * IRQ 或内部锁。一次公开调用会在调用线程中完成全部位传输；同一实例若可能被
 * 多个执行上下文访问，调用方必须在外部串行化，避免 CSN/CE/SCK 时序交叉。
 */
namespace Module
{

/**
 * @brief 单个 NRF24L01 的同步、无锁 GPIO 模拟 SPI 传输与工作模式控制器。
 *
 * 地址宽度固定为 5 字节，静态载荷宽度固定为 32 字节；Initialize() 会拒绝
 * 其他载荷宽度。寄存器和 FIFO 原语仍按调用方提供的 count 逐字节传输，不在
 * 接口内部推断动态载荷长度或检查缓冲区容量。
 *
 * OutputPower 的连续枚举序值直接参与 RF_SETUP 位编码，其声明顺序对应
 * RF_PWR[2:1]。DataRate 通过显式枚举标识比较选择 RF_DR_LOW/RF_DR_HIGH，
 * 不依赖枚举声明顺序；调整 OutputPower 的顺序或数值必须同步修改编码逻辑。
 */
class Nrf24l01Bus
{
 public:
  /** NRF24L01 在本实现中使用的固定地址宽度（字节）。 */
  static constexpr size_t kAddressWidth = 5;
  /** NRF24L01 在本实现中使用的固定静态载荷宽度（字节）。 */
  static constexpr size_t kPayloadWidth = 32;

  /** RF_SETUP 数据率选择；实际位组合由 EncodeRfSetup() 生成。 */
  enum class DataRate : uint8_t
  {
    /** 250 kbps：设置 RF_DR_LOW，清除 RF_DR_HIGH。 */
    KBPS_250,
    /** 1 Mbps：同时清除 RF_DR_LOW 与 RF_DR_HIGH。 */
    MBPS_1,
    /** 2 Mbps：清除 RF_DR_LOW，设置 RF_DR_HIGH。 */
    MBPS_2,
  };

  /**
   * RF_SETUP 发射功率选择；从 DBM_NEG_18 到 DBM_0 的序值依次编码为
   * RF_PWR[2:1] 的 00、01、10、11。
   */
  enum class OutputPower : uint8_t
  {
    DBM_NEG_18,
    DBM_NEG_12,
    DBM_NEG_6,
    DBM_0,
  };

  /**
   * @brief 从硬件容器解析 CE、CSN、SCK、MOSI、MISO 五个 GPIO。
   * @param hw 保存具名 GPIO 的硬件容器；查找失败由 FindOrExit() 处理。
   * @param ce_name CE GPIO 名称。
   * @param csn_name 低有效片选 GPIO 名称。
   * @param sck_name SPI 时钟 GPIO 名称。
   * @param mosi_name 主机输出 GPIO 名称。
   * @param miso_name 主机输入 GPIO 名称。
   *
   * 构造仅保存 GPIO 引用，不配置方向或初始电平。访问总线前必须先成功调用
   * ConfigurePins()；除 Initialize() 和 Shutdown() 的局部检查外，寄存器/FIFO
   * 原语不会重复验证此前置条件。
   */
  Nrf24l01Bus(LibXR::HardwareContainer& hw, const char* ce_name,
              const char* csn_name, const char* sck_name,
              const char* mosi_name, const char* miso_name);

  Nrf24l01Bus(const Nrf24l01Bus&) = delete;
  Nrf24l01Bus(Nrf24l01Bus&&) = delete;
  Nrf24l01Bus& operator=(const Nrf24l01Bus&) = delete;
  Nrf24l01Bus& operator=(Nrf24l01Bus&&) = delete;

  /**
   * @brief 配置 GPIO 方向，并在全部配置成功后建立空闲电平。
   * @return 五个 GPIO 均配置成功时返回 true。
   *
   * 输出使用推挽且无上下拉，MISO 使用上拉输入；成功后的空闲状态为 CE=0、
   * CSN=1、SCK=0、MOSI=0。配置采用短路求值，失败时可能已有前序 GPIO 完成
   * 配置，但不会驱动统一空闲电平，且 pins_configured_ 保持 false。
   */
  bool ConfigurePins();

  /**
   * @brief 写入固定宽度、管道 0、自动应答配置，并做局部寄存器回读。
   * @param address 5 字节本地地址，同时写入 RX_ADDR_P0 与 TX_ADDR。
   * @param channel 射频频道 RF_CH，合法范围为 0..125。
   * @param data_rate RF_SETUP 数据率。
   * @param output_power RF_SETUP 发射功率。
   * @param retry_delay_us 自动重传间隔，须为 250..4000 us 且为 250 us 的倍数。
   * @param retry_count 自动重传次数，合法范围为 0..15。
   * @param payload_width 须等于 kPayloadWidth（32 字节）。
   * @return 参数和引脚前置条件满足，且局部回读一致时返回 true。
   *
   * 初始化会清空 TX/RX FIFO、清除状态标志，最后调用 EnterRx()。回读只覆盖
   * SETUP_RETR、RF_CH、RF_SETUP、RX_PW_P0；它不是对全部配置、地址或 FIFO
   * 状态的完整验证，也不包含显式上电等待。
   */
  bool Initialize(const std::array<uint8_t, kAddressWidth>& address,
                  uint8_t channel, DataRate data_rate,
                  OutputPower output_power, uint16_t retry_delay_us,
                  uint8_t retry_count, uint8_t payload_width);

  /** 以一个 CSN 低电平事务发送 NOP，并返回命令阶段采样到的 STATUS。 */
  uint8_t ReadStatus();
  /** 读取 reg&0x1F 指定的单字节寄存器；CSN 包围命令和数据字节。 */
  uint8_t ReadRegister(uint8_t reg);
  /**
   * 从 reg&0x1F 起在同一 CSN 低电平事务中读取 count 字节。
   * data 必须指向至少 count 字节的可写缓冲区；本函数不做空指针或容量检查。
   */
  void ReadRegisters(uint8_t reg, uint8_t* data, size_t count);
  /** 写入 reg&0x1F 指定的单字节寄存器；CSN 包围命令和数据字节。 */
  void WriteRegister(uint8_t reg, uint8_t value);
  /**
   * 从 reg&0x1F 起在同一 CSN 低电平事务中写入 count 字节。
   * data 必须提供至少 count 字节；本函数不做空指针或容量检查。
   */
  void WriteRegisters(uint8_t reg, const uint8_t* data, size_t count);
  /**
   * 在一个 R_RX_PAYLOAD 事务中读取 count 字节；count 和目标容量由调用方保证。
   */
  void ReadRxPayload(uint8_t* data, size_t count);
  /**
   * 在一个 W_TX_PAYLOAD 事务中写入 count 字节；count 和源容量由调用方保证。
   */
  void WriteTxPayload(const uint8_t* data, size_t count);
  /** 清除 RX_DR，并清空整个 RX FIFO，未读取的其余载荷也会被丢弃。 */
  void FinishRx();
  /** 发送独立的 FLUSH_TX 命令事务，丢弃整个 TX FIFO。 */
  void FlushTx();
  /** 发送独立的 FLUSH_RX 命令事务，丢弃整个 RX FIFO。 */
  void FlushRx();
  /** 回读 CONFIG；0xFF 视为总线无效，否则返回其中 PWR_UP 位。 */
  bool IsPoweredUp();

  /**
   * @brief 以给定管道 0 地址进入上电接收模式。
   *
   * 依次拉低 CE、写 RX_ADDR_P0、设置 CRC/PWR_UP/PRIM_RX，再拉高 CE。
   * 不清除状态标志、不清空 RX FIFO，也不执行显式上电等待。
   */
  void EnterRx(const std::array<uint8_t, kAddressWidth>& rx_address);

  /**
   * @brief 装载一个固定 32 字节载荷，并切换到发送模式。
   *
   * 拉低 CE 后向 STATUS 写 1 清除 RX_DR、TX_DS、MAX_RT，并清空 TX FIFO；
   * 将目标地址同时写入 TX_ADDR 与 RX_ADDR_P0（供自动应答使用），装载载荷，
   * 写入上电 PTX 配置，最后拉高 CE。清除 RX_DR 是 RX 侧事件标志副作用，但
   * 函数不清空 RX FIFO；也不等待发送完成或在内部生成带延时的 CE 脉冲。
   */
  void StartTx(const std::array<uint8_t, kAddressWidth>& tx_address,
               const std::array<uint8_t, kPayloadWidth>& payload);

  /**
   * @brief 结束发送并用本地地址恢复接收模式。
   *
   * 拉低 CE，向 STATUS 写 1 清除 RX_DR、TX_DS、MAX_RT，再清空 TX FIFO 和
   * RX FIFO，然后通过 EnterRx() 重写 RX_ADDR_P0 并将 CE 拉高；RX FIFO 中
   * 任何残留载荷都会丢失。
   */
  void FinishTx(const std::array<uint8_t, kAddressWidth>& rx_address);

  /**
   * @brief 在引脚已配置时关闭无线电电源位并恢复总线空闲输出。
   *
   * 拉低 CE，将 CONFIG 写为仅启用 CRC（PWR_UP 清零），并设置 CSN=1、
   * SCK=0、MOSI=0。不清除状态标志、不清空 FIFO、不反配置 GPIO，且
   * pins_configured_ 仍保持原值。
   */
  void Shutdown();

 private:
  /**
   * 编码 RF_SETUP；OutputPower 序值与 RF_PWR 位布局直接耦合，DataRate 通过
   * 枚举标识分支选择 RF_DR_LOW/RF_DR_HIGH，2 Mbps + 0 dBm 返回 0x0E。
   */
  static uint8_t EncodeRfSetup(DataRate data_rate, OutputPower output_power);
  /** 将 250 us 步进延迟和四位自动重传次数编码到 SETUP_RETR。 */
  static uint8_t EncodeSetupRetr(uint16_t retry_delay_us,
                                 uint8_t retry_count);

  /** 直接驱动模式控制引脚 CE；高低电平含义由当前收发流程决定。 */
  void WriteCe(bool high);
  /** 驱动低有效片选 CSN；低电平开始事务，高电平结束事务。 */
  void WriteCsn(bool high);
  /** 驱动模拟 SPI 时钟 SCK；Mode 0 空闲电平为低。 */
  void WriteSck(bool high);
  /** 驱动 MOSI，为下一次 SCK 上升沿准备主机发送位。 */
  void WriteMosi(bool high);
  /** 读取 MISO 当前电平，供 SCK 拉高后的输入位采样使用。 */
  bool ReadMiso();
  /**
   * 同步交换一个 MSB-first 字节：SCK 空闲为低，先设置 MOSI 当前最高位，
   * 再拉高 SCK 并采样 MISO，随后拉低 SCK进入下一位。函数不加锁、不延时。
   */
  uint8_t SwapByte(uint8_t value);
  /** 以 CSN=0 包围一个命令字节，结束后恢复 CSN=1。 */
  void SendCommand(uint8_t command);

  /** CE 模式控制 GPIO 引用，由构造函数按名称解析。 */
  LibXR::GPIO& ce_;
  /** 低有效 SPI 片选 GPIO 引用，由构造函数按名称解析。 */
  LibXR::GPIO& csn_;
  /** 模拟 SPI 时钟 GPIO 引用，由构造函数按名称解析。 */
  LibXR::GPIO& sck_;
  /** 模拟 SPI 主机输出 GPIO 引用，由构造函数按名称解析。 */
  LibXR::GPIO& mosi_;
  /** 模拟 SPI 主机输入 GPIO 引用，由构造函数按名称解析。 */
  LibXR::GPIO& miso_;
  /** 仅表示 ConfigurePins() 的五项配置是否全部成功，不表示芯片已初始化。 */
  bool pins_configured_ = false;
};

}  // namespace Module
