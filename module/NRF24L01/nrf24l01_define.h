/**
 * @file nrf24l01_define.h
 * @brief NRF24L01 驱动的固定宽度策略、SPI 指令码和寄存器地址定义。
 *
 * 地址固定为 5 字节、收发载荷固定为 32 字节，是当前驱动采用的通信策略，
 * 不是器件能力的通用限制；收发双方必须使用一致的地址宽度和载荷配置。
 */
#ifndef __NRF24L01_DEFINE_H
#define __NRF24L01_DEFINE_H

#include <stdint.h>

#include "ti_msp_dl_config.h"

/** @brief 当前驱动固定使用的地址宽度，单位为字节。 */
#define NRF24L01_ADDRESS_WIDTH      (5U)

/** @brief 当前驱动固定使用的 TX 有效载荷宽度，单位为字节。 */
#define NRF24L01_TX_PACKET_WIDTH    (32U)

/** @brief 当前驱动固定使用的 RX 有效载荷宽度，单位为字节。 */
#define NRF24L01_RX_PACKET_WIDTH    (32U)

/**
 * @name NRF24L01 SPI 指令
 *
 * 每次 SPI 事务中，器件在主机发送首个指令字节的同时返回 STATUS 寄存器值；
 * 因此任意指令的首字节返回值都可用于检查 RX_DR、TX_DS、MAX_RT 等状态。
 * 寄存器读写指令由“指令码 | (寄存器地址 & 0x1F)”组成，低 5 位承载寄存器地址。
 * @{
 */

/**
 * 读取寄存器，后续时钟读出 1~5 字节。
 * 前置：将目标寄存器地址组合到指令低 5 位，多字节寄存器按其实际宽度读取。
 * 副作用：只读出寄存器内容；读取 STATUS 本身不会清除其中的中断标志。
 */
#define NRF24L01_R_REGISTER			0x00

/**
 * 写寄存器，后续发送 1~5 字节。
 * 前置：将可写目标寄存器地址组合到指令低 5 位，并遵守该寄存器的位域和宽度约束。
 * 副作用：更新目标寄存器；写 STATUS 时 RX_DR、TX_DS、MAX_RT 位采用“写 1 清除”。
 */
#define NRF24L01_W_REGISTER			0x20

/**
 * 从 RX FIFO 顶部读取 1~32 字节有效载荷。
 * 前置：RX FIFO 非空，并按静态包长配置或 R_RX_PL_WID 的结果提供足够读取时钟。
 * 副作用：完成读取后，该有效载荷从 RX FIFO 出队，后续数据包成为 FIFO 顶部。
 */
#define NRF24L01_R_RX_PAYLOAD		0x61

/**
 * 向 TX FIFO 写入 1~32 字节有效载荷。
 * 前置：TX FIFO 尚有空间，载荷长度符合当前静态或动态包长配置。
 * 副作用：载荷进入 TX FIFO；后续由 CE 触发发送，若 FIFO 已满则不会装入新载荷。
 */
#define NRF24L01_W_TX_PAYLOAD		0xA0

/**
 * 清空 TX FIFO，指令后不带数据字节。
 * 前置：应在 CE 为低且器件未进行发射时使用，避免破坏正在处理的发送或应答。
 * 副作用：丢弃 TX FIFO 中所有待发送载荷，并清除有效载荷重用状态。
 */
#define NRF24L01_FLUSH_TX			0xE1

/**
 * 清空 RX FIFO，指令后不带数据字节。
 * 前置：应在 CE 为低且器件未接收数据包时使用。
 * 副作用：丢弃 RX FIFO 中所有未读取载荷；STATUS 中的 RX_DR 仍需通过写 1 单独清除。
 */
#define NRF24L01_FLUSH_RX			0xE2

/**
 * 重用最后一次发送的有效载荷，指令后不带数据字节。
 * 前置：TX FIFO 中存在可重用的最后发送载荷，并在下一次发射触发前下发该指令。
 * 副作用：置位 FIFO_STATUS.TX_REUSE；重用持续到 W_TX_PAYLOAD 或 FLUSH_TX 为止。
 */
#define NRF24L01_REUSE_TX_PL		0xE3

/**
 * 读取 RX FIFO 顶部数据包的动态宽度，后续读出 1 字节，合法结果为 1~32。
 * 前置：已启用动态包长且 RX FIFO 非空。
 * 副作用：不弹出载荷；若结果大于 32，接收内容无效，调用方应清空 RX FIFO。
 */
#define NRF24L01_R_RX_PL_WID		0x60

/**
 * 为指定接收数据管道写入 1~32 字节 ACK Payload；低 3 位是接收数据管道号 0~5。
 * 前置：FEATURE.EN_ACK_PAY 与 FEATURE.EN_DPL 已启用，目标接收数据管道允许动态包长，
 *       且 TX FIFO 尚有空间。
 * 副作用：载荷排入 TX FIFO，供该接收数据管道后续自动应答携带；FIFO 满时不会装入。
 */
#define NRF24L01_W_ACK_PAYLOAD		0xA8

/**
 * 向 TX FIFO 写入 1~32 字节无 ACK 有效载荷。
 * 前置：FEATURE.EN_DYN_ACK 已启用且 TX FIFO 尚有空间。
 * 副作用：该载荷入队并仅对本次载荷禁止自动应答；FIFO 满时不会装入。
 */
#define NRF24L01_W_TX_PAYLOAD_NOACK	0xB0

/**
 * 空操作，指令后不带数据字节。
 * 前置：无额外功能配置要求。
 * 副作用：不修改寄存器或 FIFO；利用首字节返回值可无副作用地读取 STATUS。
 */
#define NRF24L01_NOP				0xFF

/** @} */

/**
 * @name NRF24L01 寄存器地址
 *
 * 以下值仅为 5 位寄存器地址，访问时需与 R_REGISTER 或 W_REGISTER 指令码组合。
 * @{
 */

/**
 * CONFIG（1 字节）：[6] MASK_RX_DR、[5] MASK_TX_DS、[4] MASK_MAX_RT 仅屏蔽 IRQ 引脚，
 * [3] EN_CRC、[2] CRCO（0=1 字节 CRC，1=2 字节 CRC）、[1] PWR_UP、[0] PRIM_RX。
 */
#define NRF24L01_CONFIG				0x00

/** EN_AA（1 字节）：[5:0] ENAA_P5~ENAA_P0，分别控制接收数据管道 5~0 的自动应答。 */
#define NRF24L01_EN_AA				0x01

/** EN_RXADDR（1 字节）：[5:0] ERX_P5~ERX_P0，分别使能接收数据管道 5~0。 */
#define NRF24L01_EN_RXADDR			0x02

/**
 * SETUP_AW（1 字节）：[1:0] 地址宽度，01=3 字节、10=4 字节、11=5 字节，00 为非法值；
 * 当前驱动的 NRF24L01_ADDRESS_WIDTH 为 5，因此收发双方应配置为 11。
 */
#define NRF24L01_SETUP_AW			0x03

/**
 * SETUP_RETR（1 字节）：[7:4] ARD，自动重传间隔为 (ARD+1)*250 us；
 * [3:0] ARC，自动重传次数范围 0~15，0 表示禁止自动重传。
 */
#define NRF24L01_SETUP_RETR			0x04

/** RF_CH（1 字节）：[6:0] 射频频道范围 0~125，对应中心频率 2400+RF_CH MHz。 */
#define NRF24L01_RF_CH				0x05

/**
 * RF_SETUP（1 字节）：[7] CONT_WAVE、[5] RF_DR_LOW、[4] PLL_LOCK、[3] RF_DR_HIGH、
 * [2:1] RF_PWR；数据率组合 (RF_DR_LOW, RF_DR_HIGH) 为 00=1 Mbps、01=2 Mbps、
 * 10=250 kbps、11=保留，
 * RF_PWR 依次为 00=-18 dBm、01=-12 dBm、10=-6 dBm、11=0 dBm。
 */
#define NRF24L01_RF_SETUP			0x06

/**
 * STATUS（1 字节）：[6] RX_DR、[5] TX_DS、[4] MAX_RT 是中断标志，均为写 1 清除；
 * [3:1] RX_P_NO 为 RX FIFO 顶部数据包的接收数据管道号（0~5，110 未使用，111 表示 FIFO 为空），
 * [0] TX_FULL 表示 TX FIFO 已满。每条 SPI 指令的首字节都会同时返回此寄存器值。
 */
#define NRF24L01_STATUS				0x07

/**
 * OBSERVE_TX（1 字节，只读）：[7:4] PLOS_CNT 为累计丢包计数，最大值 15，写射频频道寄存器 RF_CH 后清零；
 * [3:0] ARC_CNT 为最近一次发送所用的自动重传次数，范围 0~15。
 */
#define NRF24L01_OBSERVE_TX			0x08

/** RPD（1 字节，只读）：[0] RPD，RX 模式检测到高于 -64 dBm 的接收功率时置 1。 */
#define NRF24L01_RPD				0x09

/**
 * RX_ADDR_P0（3~5 字节）：接收数据管道 0 的完整地址，宽度由 SETUP_AW 决定；
 * 当前驱动使用 5 字节。启用 PTX 自动应答时，该地址应与 TX_ADDR 一致以接收 ACK。
 */
#define NRF24L01_RX_ADDR_P0			0x0A

/**
 * RX_ADDR_P1（3~5 字节）：接收数据管道 1 的完整地址，宽度由 SETUP_AW 决定；
 * 当前驱动使用 5 字节，同时其高位地址由接收数据管道 2~5 共享。
 */
#define NRF24L01_RX_ADDR_P1			0x0B

/** RX_ADDR_P2（1 字节）：仅配置接收数据管道 2 的最低字节，高位地址与 RX_ADDR_P1 相同。 */
#define NRF24L01_RX_ADDR_P2			0x0C

/** RX_ADDR_P3（1 字节）：仅配置接收数据管道 3 的最低字节，高位地址与 RX_ADDR_P1 相同。 */
#define NRF24L01_RX_ADDR_P3			0x0D

/** RX_ADDR_P4（1 字节）：仅配置接收数据管道 4 的最低字节，高位地址与 RX_ADDR_P1 相同。 */
#define NRF24L01_RX_ADDR_P4			0x0E

/** RX_ADDR_P5（1 字节）：仅配置接收数据管道 5 的最低字节，高位地址与 RX_ADDR_P1 相同。 */
#define NRF24L01_RX_ADDR_P5			0x0F

/**
 * TX_ADDR（3~5 字节）：发送地址，宽度由 SETUP_AW 决定，当前驱动使用 5 字节；
 * 启用 PTX 自动应答时应与 RX_ADDR_P0 保持一致。
 */
#define NRF24L01_TX_ADDR			0x10

/** RX_PW_P0（1 字节）：[5:0] 数据管道 0 的静态载荷宽度，范围 0~32；当前驱动使用 32。 */
#define NRF24L01_RX_PW_P0			0x11

/** RX_PW_P1（1 字节）：[5:0] 可配置数据管道 1 的静态载荷宽度为 0~32 字节；当前实现未启用、也未配置该数据管道。 */
#define NRF24L01_RX_PW_P1			0x12

/** RX_PW_P2（1 字节）：[5:0] 可配置数据管道 2 的静态载荷宽度为 0~32 字节；当前实现未启用、也未配置该数据管道。 */
#define NRF24L01_RX_PW_P2			0x13

/** RX_PW_P3（1 字节）：[5:0] 可配置数据管道 3 的静态载荷宽度为 0~32 字节；当前实现未启用、也未配置该数据管道。 */
#define NRF24L01_RX_PW_P3			0x14

/** RX_PW_P4（1 字节）：[5:0] 可配置数据管道 4 的静态载荷宽度为 0~32 字节；当前实现未启用、也未配置该数据管道。 */
#define NRF24L01_RX_PW_P4			0x15

/** RX_PW_P5（1 字节）：[5:0] 可配置数据管道 5 的静态载荷宽度为 0~32 字节；当前实现未启用、也未配置该数据管道。 */
#define NRF24L01_RX_PW_P5			0x16

/**
 * FIFO_STATUS（1 字节，只读）：[6] TX_REUSE、[5] TX_FULL、[4] TX_EMPTY、
 * [1] RX_FULL、[0] RX_EMPTY，用于判断收发 FIFO 状态和有效载荷重用状态。
 */
#define NRF24L01_FIFO_STATUS		0x17

/**
 * DYNPD（1 字节）：[5:0] DPL_P5~DPL_P0，分别允许接收数据管道 5~0 使用动态包长；
 * 仅在 FEATURE.EN_DPL=1 时生效，启用后对应数据管道不再使用 RX_PW_Pn 的静态宽度。
 */
#define NRF24L01_DYNPD				0x1C

/**
 * FEATURE（1 字节）：[2] EN_DPL 启用动态包长，[1] EN_ACK_PAY 启用 ACK Payload，
 * [0] EN_DYN_ACK 允许单个发送载荷通过 W_TX_PAYLOAD_NOACK 禁止自动应答。
 */
#define NRF24L01_FEATURE			0x1D

/** @} */

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
