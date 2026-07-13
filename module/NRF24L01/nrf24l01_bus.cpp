#include "nrf24l01_bus.hpp"

#include "nrf24l01_define.h"

/**
 * @file nrf24l01_bus.cpp
 * @brief NRF24L01 GPIO 模拟 SPI、寄存器/FIFO 事务和模式切换实现。
 *
 * 所有操作均为调用线程内的同步轮询式 GPIO 访问，不使用 IRQ，也没有内部
 * 互斥。CSN 的一次低到高定义一个 SPI 命令事务；CE 只按各模式函数中的顺序
 * 翻转，本文件没有插入上电等待或定时脉冲。
 */
namespace Module
{
namespace
{
/** CONFIG 中固定启用 CRC 的位。 */
constexpr uint8_t kConfigCrcEnabled = 1U << 3U;
/** CONFIG 的上电位。 */
constexpr uint8_t kConfigPowerUp = 1U << 1U;
/** CONFIG 的主接收模式位。 */
constexpr uint8_t kConfigPrimaryRx = 1U;
/** 写 1 清除 RX_DR、TX_DS、MAX_RT 三个状态标志。 */
constexpr uint8_t kAllStatusFlags = 0x70U;
}  // namespace

// 构造阶段只解析并保存 GPIO 引用；方向和空闲电平由 ConfigurePins() 建立。
Nrf24l01Bus::Nrf24l01Bus(LibXR::HardwareContainer& hw, const char* ce_name,
                         const char* csn_name, const char* sck_name,
                         const char* mosi_name, const char* miso_name)
    : ce_(*hw.FindOrExit<LibXR::GPIO>({ce_name})),
      csn_(*hw.FindOrExit<LibXR::GPIO>({csn_name})),
      sck_(*hw.FindOrExit<LibXR::GPIO>({sck_name})),
      mosi_(*hw.FindOrExit<LibXR::GPIO>({mosi_name})),
      miso_(*hw.FindOrExit<LibXR::GPIO>({miso_name}))
{
}

bool Nrf24l01Bus::ConfigurePins()
{
  // CE/CSN/SCK/MOSI 为推挽输出，MISO 为带上拉输入。
  const LibXR::GPIO::Configuration output = {
      LibXR::GPIO::Direction::OUTPUT_PUSH_PULL, LibXR::GPIO::Pull::NONE};
  const LibXR::GPIO::Configuration input = {LibXR::GPIO::Direction::INPUT,
                                             LibXR::GPIO::Pull::UP};

  const bool configured =
      ce_.SetConfig(output) == LibXR::ErrorCode::OK &&
      csn_.SetConfig(output) == LibXR::ErrorCode::OK &&
      sck_.SetConfig(output) == LibXR::ErrorCode::OK &&
      mosi_.SetConfig(output) == LibXR::ErrorCode::OK &&
      miso_.SetConfig(input) == LibXR::ErrorCode::OK;
  pins_configured_ = configured;

  // 仅在五项配置全部成功后驱动空闲状态：CE 低、CSN 高、SCK 低、MOSI 低。
  if (configured)
  {
    WriteCe(false);
    WriteCsn(true);
    WriteSck(false);
    WriteMosi(false);
  }
  return configured;
}

bool Nrf24l01Bus::Initialize(
    const std::array<uint8_t, kAddressWidth>& address, uint8_t channel,
    DataRate data_rate, OutputPower output_power, uint16_t retry_delay_us,
    uint8_t retry_count, uint8_t payload_width)
{
  // 固定宽度策略：射频频道 0..125，载荷必须为 32 字节；自动重传延迟按 250 us 编码。
  if (!pins_configured_ || channel > 125U || payload_width != kPayloadWidth ||
      retry_delay_us < 250U || retry_delay_us > 4000U ||
      (retry_delay_us % 250U) != 0U || retry_count > 15U)
  {
    return false;
  }

  WriteCe(false);
  WriteCsn(true);
  WriteSck(false);
  WriteMosi(false);

  // DataRate 通过枚举标识分支选速率位；OutputPower 的连续序值直接编码功率位。
  const uint8_t rf_setup = EncodeRfSetup(data_rate, output_power);
  const uint8_t setup_retr = EncodeSetupRetr(retry_delay_us, retry_count);

  WriteRegister(NRF24L01_CONFIG, kConfigCrcEnabled);
  WriteRegister(NRF24L01_EN_AA, 0x01U);
  WriteRegister(NRF24L01_EN_RXADDR, 0x01U);
  WriteRegister(NRF24L01_SETUP_AW, 0x03U);
  WriteRegister(NRF24L01_SETUP_RETR, setup_retr);
  WriteRegister(NRF24L01_RF_CH, channel);
  WriteRegister(NRF24L01_RF_SETUP, rf_setup);
  WriteRegister(NRF24L01_RX_PW_P0, payload_width);
  WriteRegister(NRF24L01_DYNPD, 0x00U);
  WriteRegister(NRF24L01_FEATURE, 0x00U);
  WriteRegisters(NRF24L01_RX_ADDR_P0, address.data(), address.size());
  WriteRegisters(NRF24L01_TX_ADDR, address.data(), address.size());
  FlushTx();
  FlushRx();
  WriteRegister(NRF24L01_STATUS, kAllStatusFlags);

  // 这里只回读四个局部配置项，不验证 CONFIG、地址、使能位或 FIFO 的全部状态。
  const bool verified =
      ReadRegister(NRF24L01_SETUP_RETR) == setup_retr &&
      ReadRegister(NRF24L01_RF_CH) == channel &&
      ReadRegister(NRF24L01_RF_SETUP) == rf_setup &&
      ReadRegister(NRF24L01_RX_PW_P0) == payload_width;
  if (!verified)
  {
    return false;
  }

  EnterRx(address);
  return true;
}

uint8_t Nrf24l01Bus::ReadStatus()
{
  // CSN 低电平覆盖完整 NOP 命令事务；交换命令时返回的字节即 STATUS。
  WriteCsn(false);
  const uint8_t status = SwapByte(NRF24L01_NOP);
  WriteCsn(true);
  return status;
}

uint8_t Nrf24l01Bus::ReadRegister(uint8_t reg)
{
  // 寄存器地址只保留低五位，命令字节和数据字节共享同一个 CSN 事务。
  WriteCsn(false);
  SwapByte(static_cast<uint8_t>(NRF24L01_R_REGISTER | (reg & 0x1FU)));
  const uint8_t value = SwapByte(NRF24L01_NOP);
  WriteCsn(true);
  return value;
}

void Nrf24l01Bus::ReadRegisters(uint8_t reg, uint8_t* data, size_t count)
{
  // count 完全由调用方决定；CSN 在整个连续读取期间保持低电平。
  WriteCsn(false);
  SwapByte(static_cast<uint8_t>(NRF24L01_R_REGISTER | (reg & 0x1FU)));
  for (size_t i = 0; i < count; ++i)
  {
    data[i] = SwapByte(NRF24L01_NOP);
  }
  WriteCsn(true);
}

void Nrf24l01Bus::WriteRegister(uint8_t reg, uint8_t value)
{
  // 单字节寄存器写入的事务边界为本次 CSN 低到高。
  WriteCsn(false);
  SwapByte(static_cast<uint8_t>(NRF24L01_W_REGISTER | (reg & 0x1FU)));
  SwapByte(value);
  WriteCsn(true);
}

void Nrf24l01Bus::WriteRegisters(uint8_t reg, const uint8_t* data,
                                 size_t count)
{
  // count 完全由调用方决定；CSN 在命令和全部数据字节期间保持低电平。
  WriteCsn(false);
  SwapByte(static_cast<uint8_t>(NRF24L01_W_REGISTER | (reg & 0x1FU)));
  for (size_t i = 0; i < count; ++i)
  {
    SwapByte(data[i]);
  }
  WriteCsn(true);
}

void Nrf24l01Bus::ReadRxPayload(uint8_t* data, size_t count)
{
  // 读取长度不从芯片查询；调用方负责使 count 与固定载荷策略及缓冲区相符。
  WriteCsn(false);
  SwapByte(NRF24L01_R_RX_PAYLOAD);
  for (size_t i = 0; i < count; ++i)
  {
    data[i] = SwapByte(NRF24L01_NOP);
  }
  WriteCsn(true);
}

void Nrf24l01Bus::WriteTxPayload(const uint8_t* data, size_t count)
{
  // 写入长度不在此处限制；调用方负责提供足够数据并符合芯片载荷宽度。
  WriteCsn(false);
  SwapByte(NRF24L01_W_TX_PAYLOAD);
  for (size_t i = 0; i < count; ++i)
  {
    SwapByte(data[i]);
  }
  WriteCsn(true);
}

void Nrf24l01Bus::FinishRx()
{
  // 写 1 清除 RX_DR，随后清空整个 RX FIFO；尚未读取的其他载荷会一并丢弃。
  WriteRegister(NRF24L01_STATUS, 1U << 6U);
  FlushRx();
}

void Nrf24l01Bus::FlushTx() { SendCommand(NRF24L01_FLUSH_TX); }

void Nrf24l01Bus::FlushRx() { SendCommand(NRF24L01_FLUSH_RX); }

bool Nrf24l01Bus::IsPoweredUp()
{
  const uint8_t config = ReadRegister(NRF24L01_CONFIG);
  return config != 0xFFU && (config & kConfigPowerUp) != 0U;
}

void Nrf24l01Bus::EnterRx(
    const std::array<uint8_t, kAddressWidth>& rx_address)
{
  // CE 先拉低以修改地址和模式，配置为上电 PRX 后直接拉高；此处没有等待。
  WriteCe(false);
  WriteRegisters(NRF24L01_RX_ADDR_P0, rx_address.data(), rx_address.size());
  WriteRegister(NRF24L01_CONFIG,
                kConfigCrcEnabled | kConfigPowerUp | kConfigPrimaryRx);
  WriteCe(true);
}

void Nrf24l01Bus::StartTx(
    const std::array<uint8_t, kAddressWidth>& tx_address,
    const std::array<uint8_t, kPayloadWidth>& payload)
{
  // 清除 RX_DR/TX_DS/MAX_RT 并丢弃旧 TX FIFO；RX_DR 清除但 RX FIFO 不清空。
  WriteCe(false);
  WriteRegister(NRF24L01_STATUS, kAllStatusFlags);
  FlushTx();
  WriteRegisters(NRF24L01_TX_ADDR, tx_address.data(), tx_address.size());
  WriteRegisters(NRF24L01_RX_ADDR_P0, tx_address.data(), tx_address.size());
  WriteTxPayload(payload.data(), payload.size());
  WriteRegister(NRF24L01_CONFIG, kConfigCrcEnabled | kConfigPowerUp);
  // CE 在函数末尾拉高并保持；发送完成由外部状态检查，代码不生成定时脉冲。
  WriteCe(true);
}

void Nrf24l01Bus::FinishTx(
    const std::array<uint8_t, kAddressWidth>& rx_address)
{
  // 清除 RX_DR/TX_DS/MAX_RT 并同时清空 TX/RX FIFO，再重写管道 0 并拉高 CE。
  WriteCe(false);
  WriteRegister(NRF24L01_STATUS, kAllStatusFlags);
  FlushTx();
  FlushRx();
  EnterRx(rx_address);
}

void Nrf24l01Bus::Shutdown()
{
  // 未配置引脚时不触碰 GPIO 或芯片寄存器。
  if (!pins_configured_)
  {
    return;
  }
  // CONFIG 仅保留 CRC 位即清除 PWR_UP；FIFO、状态标志和 GPIO 配置保持不变。
  WriteCe(false);
  WriteRegister(NRF24L01_CONFIG, kConfigCrcEnabled);
  WriteCsn(true);
  WriteSck(false);
  WriteMosi(false);
}

uint8_t Nrf24l01Bus::EncodeRfSetup(DataRate data_rate,
                                   OutputPower output_power)
{
  // OutputPower 的 0..3 序值左移一位，正好形成 RF_PWR[2:1]。
  constexpr uint8_t kRfSetup2Mbps0dBm = 0x0EU;
  uint8_t value = static_cast<uint8_t>(output_power) << 1U;
  if (data_rate == DataRate::KBPS_250)
  {
    value |= 1U << 5U;
  }
  else if (data_rate == DataRate::MBPS_2)
  {
    value |= 1U << 3U;
  }
  if (data_rate == DataRate::MBPS_2 && output_power == OutputPower::DBM_0)
  {
    return kRfSetup2Mbps0dBm;
  }
  return value;
}

uint8_t Nrf24l01Bus::EncodeSetupRetr(uint16_t retry_delay_us,
                                    uint8_t retry_count)
{
  // ARD 为 (delay/250)-1 的高四位，ARC 为自动重传次数的低四位。
  const uint8_t delay =
      static_cast<uint8_t>((retry_delay_us / 250U) - 1U);
  return static_cast<uint8_t>((delay << 4U) | (retry_count & 0x0FU));
}

void Nrf24l01Bus::WriteCe(bool high) { ce_.Write(high); }

void Nrf24l01Bus::WriteCsn(bool high) { csn_.Write(high); }

void Nrf24l01Bus::WriteSck(bool high) { sck_.Write(high); }

void Nrf24l01Bus::WriteMosi(bool high) { mosi_.Write(high); }

bool Nrf24l01Bus::ReadMiso() { return miso_.Read(); }

uint8_t Nrf24l01Bus::SwapByte(uint8_t value)
{
  uint8_t received = 0U;
  for (uint8_t bit = 0U; bit < 8U; ++bit)
  {
    // SPI Mode 0、MSB-first：SCK 低时设置 MOSI，拉高后采样 MISO，再拉低。
    WriteMosi((value & 0x80U) != 0U);
    value <<= 1U;
    WriteSck(true);
    received = static_cast<uint8_t>((received << 1U) |
                                    (ReadMiso() ? 1U : 0U));
    WriteSck(false);
  }
  return received;
}

void Nrf24l01Bus::SendCommand(uint8_t command)
{
  // 无数据阶段的命令也使用独立的 CSN 低到高事务。
  WriteCsn(false);
  SwapByte(command);
  WriteCsn(true);
}

}  // namespace Module
