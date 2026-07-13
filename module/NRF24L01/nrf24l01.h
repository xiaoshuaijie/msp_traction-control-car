#ifndef __NRF24L01_H
#define __NRF24L01_H

#include <stdint.h>

#include "nrf24l01_define.h"

#ifdef __cplusplus
extern "C" {
#endif

/*外部可调用全局数组***********/

extern uint8_t NRF24L01_TxAddress[NRF24L01_ADDRESS_WIDTH];
extern uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH];

extern uint8_t NRF24L01_RxAddress[NRF24L01_ADDRESS_WIDTH];
extern uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH];

/***********外部可调用全局数组*/


/*函数声明*********************/

/*指令实现*/
uint8_t NRF24L01_ReadReg(uint8_t RegAddress);
void NRF24L01_ReadRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count);
void NRF24L01_WriteReg(uint8_t RegAddress, uint8_t Data);
void NRF24L01_WriteRegs(uint8_t RegAddress, uint8_t *DataArray, uint8_t Count);
void NRF24L01_ReadRxPayload(uint8_t *DataArray, uint8_t Count);
void NRF24L01_WriteTxPayload(uint8_t *DataArray, uint8_t Count);
void NRF24L01_FlushTx(void);
void NRF24L01_FlushRx(void);
uint8_t NRF24L01_ReadStatus(void);

/*功能函数*/
void NRF24L01_PowerDown(void);
void NRF24L01_StandbyI(void);
void NRF24L01_Rx(void);
void NRF24L01_Tx(void);

void NRF24L01_Init(void);
uint8_t NRF24L01_Send(void);
uint8_t NRF24L01_Receive(void);
void NRF24L01_UpdateRxAddress(void);

/*********************函数声明*/

#ifdef __cplusplus
}
#endif

#endif


/*****************江协科技|版权所有****************/
/*****************jiangxiekeji.com*****************/
