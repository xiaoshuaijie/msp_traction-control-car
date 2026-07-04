/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the LP_MSPM0G3507
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_LP_MSPM0G3507
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA1
#define PWM_0_INST_IRQHandler                                   TIMA1_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA1_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                             32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_0_C0_PORT                                                 GPIOA
#define GPIO_PWM_0_C0_PIN                                         DL_GPIO_PIN_17
#define GPIO_PWM_0_C0_IOMUX                                      (IOMUX_PINCM39)
#define GPIO_PWM_0_C0_IOMUX_FUNC                     IOMUX_PINCM39_PF_TIMA1_CCP0
#define GPIO_PWM_0_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_0_C1_PORT                                                 GPIOA
#define GPIO_PWM_0_C1_PIN                                         DL_GPIO_PIN_16
#define GPIO_PWM_0_C1_IOMUX                                      (IOMUX_PINCM38)
#define GPIO_PWM_0_C1_IOMUX_FUNC                     IOMUX_PINCM38_PF_TIMA1_CCP1
#define GPIO_PWM_0_C1_IDX                                    DL_TIMER_CC_1_INDEX

/* Defines for PWM_1 */
#define PWM_1_INST                                                         TIMG6
#define PWM_1_INST_IRQHandler                                   TIMG6_IRQHandler
#define PWM_1_INST_INT_IRQN                                     (TIMG6_INT_IRQn)
#define PWM_1_INST_CLK_FREQ                                             32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_1_C0_PORT                                                 GPIOB
#define GPIO_PWM_1_C0_PIN                                          DL_GPIO_PIN_6
#define GPIO_PWM_1_C0_IOMUX                                      (IOMUX_PINCM23)
#define GPIO_PWM_1_C0_IOMUX_FUNC                     IOMUX_PINCM23_PF_TIMG6_CCP0
#define GPIO_PWM_1_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_1_C1_PORT                                                 GPIOB
#define GPIO_PWM_1_C1_PIN                                          DL_GPIO_PIN_7
#define GPIO_PWM_1_C1_IOMUX                                      (IOMUX_PINCM24)
#define GPIO_PWM_1_C1_IOMUX_FUNC                     IOMUX_PINCM24_PF_TIMG6_CCP1
#define GPIO_PWM_1_C1_IDX                                    DL_TIMER_CC_1_INDEX



/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           32000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOA
#define GPIO_UART_0_TX_PORT                                                GPIOA
#define GPIO_UART_0_RX_PIN                                        DL_GPIO_PIN_11
#define GPIO_UART_0_TX_PIN                                        DL_GPIO_PIN_10
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM22)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM21)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM21_PF_UART0_TX
#define UART_0_BAUD_RATE                                                (115200)
#define UART_0_IBRD_32_MHZ_115200_BAUD                                      (17)
#define UART_0_FBRD_32_MHZ_115200_BAUD                                      (23)





/* Port definition for Pin Group LED */
#define LED_PORT                                                         (GPIOB)

/* Defines for LED0: GPIOB.22 with pinCMx 50 on package pin 21 */
#define LED_LED0_PIN                                            (DL_GPIO_PIN_22)
#define LED_LED0_IOMUX                                           (IOMUX_PINCM50)
/* Port definition for Pin Group KEY */
#define KEY_PORT                                                         (GPIOB)

/* Defines for k4: GPIOB.21 with pinCMx 49 on package pin 20 */
#define KEY_k4_PIN                                              (DL_GPIO_PIN_21)
#define KEY_k4_IOMUX                                             (IOMUX_PINCM49)
/* Defines for k1: GPIOB.14 with pinCMx 31 on package pin 2 */
#define KEY_k1_PIN                                              (DL_GPIO_PIN_14)
#define KEY_k1_IOMUX                                             (IOMUX_PINCM31)
/* Defines for k2: GPIOB.19 with pinCMx 45 on package pin 16 */
#define KEY_k2_PIN                                              (DL_GPIO_PIN_19)
#define KEY_k2_IOMUX                                             (IOMUX_PINCM45)
/* Defines for k3: GPIOB.20 with pinCMx 48 on package pin 19 */
#define KEY_k3_PIN                                              (DL_GPIO_PIN_20)
#define KEY_k3_IOMUX                                             (IOMUX_PINCM48)
/* Defines for OUT1: GPIOA.7 with pinCMx 14 on package pin 49 */
#define Line_OUT1_PORT                                                   (GPIOA)
#define Line_OUT1_PIN                                            (DL_GPIO_PIN_7)
#define Line_OUT1_IOMUX                                          (IOMUX_PINCM14)
/* Defines for OUT2: GPIOB.12 with pinCMx 29 on package pin 64 */
#define Line_OUT2_PORT                                                   (GPIOB)
#define Line_OUT2_PIN                                           (DL_GPIO_PIN_12)
#define Line_OUT2_IOMUX                                          (IOMUX_PINCM29)
/* Defines for OUT3: GPIOB.5 with pinCMx 18 on package pin 53 */
#define Line_OUT3_PORT                                                   (GPIOB)
#define Line_OUT3_PIN                                            (DL_GPIO_PIN_5)
#define Line_OUT3_IOMUX                                          (IOMUX_PINCM18)
/* Defines for OUT4: GPIOB.4 with pinCMx 17 on package pin 52 */
#define Line_OUT4_PORT                                                   (GPIOB)
#define Line_OUT4_PIN                                            (DL_GPIO_PIN_4)
#define Line_OUT4_IOMUX                                          (IOMUX_PINCM17)
/* Defines for OUT5: GPIOA.31 with pinCMx 6 on package pin 39 */
#define Line_OUT5_PORT                                                   (GPIOA)
#define Line_OUT5_PIN                                           (DL_GPIO_PIN_31)
#define Line_OUT5_IOMUX                                           (IOMUX_PINCM6)
/* Defines for OUT6: GPIOA.28 with pinCMx 3 on package pin 35 */
#define Line_OUT6_PORT                                                   (GPIOA)
#define Line_OUT6_PIN                                           (DL_GPIO_PIN_28)
#define Line_OUT6_IOMUX                                           (IOMUX_PINCM3)
/* Defines for OUT7: GPIOA.8 with pinCMx 19 on package pin 54 */
#define Line_OUT7_PORT                                                   (GPIOA)
#define Line_OUT7_PIN                                            (DL_GPIO_PIN_8)
#define Line_OUT7_IOMUX                                          (IOMUX_PINCM19)
/* Defines for OUT8: GPIOA.9 with pinCMx 20 on package pin 55 */
#define Line_OUT8_PORT                                                   (GPIOA)
#define Line_OUT8_PIN                                            (DL_GPIO_PIN_9)
#define Line_OUT8_IOMUX                                          (IOMUX_PINCM20)
/* Defines for AIN1_F: GPIOA.14 with pinCMx 36 on package pin 7 */
#define tb6612_AIN1_F_PORT                                               (GPIOA)
#define tb6612_AIN1_F_PIN                                       (DL_GPIO_PIN_14)
#define tb6612_AIN1_F_IOMUX                                      (IOMUX_PINCM36)
/* Defines for BIN1_F: GPIOA.12 with pinCMx 34 on package pin 5 */
#define tb6612_BIN1_F_PORT                                               (GPIOA)
#define tb6612_BIN1_F_PIN                                       (DL_GPIO_PIN_12)
#define tb6612_BIN1_F_IOMUX                                      (IOMUX_PINCM34)
/* Defines for BIN2_F: GPIOA.13 with pinCMx 35 on package pin 6 */
#define tb6612_BIN2_F_PORT                                               (GPIOA)
#define tb6612_BIN2_F_PIN                                       (DL_GPIO_PIN_13)
#define tb6612_BIN2_F_IOMUX                                      (IOMUX_PINCM35)
/* Defines for AIN1_B: GPIOB.8 with pinCMx 25 on package pin 60 */
#define tb6612_AIN1_B_PORT                                               (GPIOB)
#define tb6612_AIN1_B_PIN                                        (DL_GPIO_PIN_8)
#define tb6612_AIN1_B_IOMUX                                      (IOMUX_PINCM25)
/* Defines for AIN2_B: GPIOB.9 with pinCMx 26 on package pin 61 */
#define tb6612_AIN2_B_PORT                                               (GPIOB)
#define tb6612_AIN2_B_PIN                                        (DL_GPIO_PIN_9)
#define tb6612_AIN2_B_IOMUX                                      (IOMUX_PINCM26)
/* Defines for BIN1_B: GPIOB.23 with pinCMx 51 on package pin 22 */
#define tb6612_BIN1_B_PORT                                               (GPIOB)
#define tb6612_BIN1_B_PIN                                       (DL_GPIO_PIN_23)
#define tb6612_BIN1_B_IOMUX                                      (IOMUX_PINCM51)
/* Defines for BIN2_B: GPIOB.26 with pinCMx 57 on package pin 28 */
#define tb6612_BIN2_B_PORT                                               (GPIOB)
#define tb6612_BIN2_B_PIN                                       (DL_GPIO_PIN_26)
#define tb6612_BIN2_B_IOMUX                                      (IOMUX_PINCM57)
/* Defines for AIN2_F: GPIOA.15 with pinCMx 37 on package pin 8 */
#define tb6612_AIN2_F_PORT                                               (GPIOA)
#define tb6612_AIN2_F_PIN                                       (DL_GPIO_PIN_15)
#define tb6612_AIN2_F_IOMUX                                      (IOMUX_PINCM37)
/* Port definition for Pin Group SPI */
#define SPI_PORT                                                         (GPIOA)

/* Defines for CE: GPIOA.22 with pinCMx 47 on package pin 18 */
#define SPI_CE_PIN                                              (DL_GPIO_PIN_22)
#define SPI_CE_IOMUX                                             (IOMUX_PINCM47)
/* Defines for CSN: GPIOA.24 with pinCMx 54 on package pin 25 */
#define SPI_CSN_PIN                                             (DL_GPIO_PIN_24)
#define SPI_CSN_IOMUX                                            (IOMUX_PINCM54)
/* Defines for SCK: GPIOA.25 with pinCMx 55 on package pin 26 */
#define SPI_SCK_PIN                                             (DL_GPIO_PIN_25)
#define SPI_SCK_IOMUX                                            (IOMUX_PINCM55)
/* Defines for MOSI: GPIOA.27 with pinCMx 60 on package pin 31 */
#define SPI_MOSI_PIN                                            (DL_GPIO_PIN_27)
#define SPI_MOSI_IOMUX                                           (IOMUX_PINCM60)
/* Defines for MISO: GPIOA.30 with pinCMx 5 on package pin 37 */
#define SPI_MISO_PIN                                            (DL_GPIO_PIN_30)
#define SPI_MISO_IOMUX                                            (IOMUX_PINCM5)
/* Port definition for Pin Group encoder_jie */
#define encoder_jie_PORT                                                 (GPIOB)

/* Defines for FLA: GPIOB.0 with pinCMx 12 on package pin 47 */
// pins affected by this interrupt request:["FLA","FLB","FRA","FRB","BLA","BLB","BRA","BRB"]
#define encoder_jie_INT_IRQN                                    (GPIOB_INT_IRQn)
#define encoder_jie_INT_IIDX                    (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define encoder_jie_FLA_IIDX                                 (DL_GPIO_IIDX_DIO0)
#define encoder_jie_FLA_PIN                                      (DL_GPIO_PIN_0)
#define encoder_jie_FLA_IOMUX                                    (IOMUX_PINCM12)
/* Defines for FLB: GPIOB.1 with pinCMx 13 on package pin 48 */
#define encoder_jie_FLB_IIDX                                 (DL_GPIO_IIDX_DIO1)
#define encoder_jie_FLB_PIN                                      (DL_GPIO_PIN_1)
#define encoder_jie_FLB_IOMUX                                    (IOMUX_PINCM13)
/* Defines for FRA: GPIOB.10 with pinCMx 27 on package pin 62 */
#define encoder_jie_FRA_IIDX                                (DL_GPIO_IIDX_DIO10)
#define encoder_jie_FRA_PIN                                     (DL_GPIO_PIN_10)
#define encoder_jie_FRA_IOMUX                                    (IOMUX_PINCM27)
/* Defines for FRB: GPIOB.11 with pinCMx 28 on package pin 63 */
#define encoder_jie_FRB_IIDX                                (DL_GPIO_IIDX_DIO11)
#define encoder_jie_FRB_PIN                                     (DL_GPIO_PIN_11)
#define encoder_jie_FRB_IOMUX                                    (IOMUX_PINCM28)
/* Defines for BLA: GPIOB.13 with pinCMx 30 on package pin 1 */
#define encoder_jie_BLA_IIDX                                (DL_GPIO_IIDX_DIO13)
#define encoder_jie_BLA_PIN                                     (DL_GPIO_PIN_13)
#define encoder_jie_BLA_IOMUX                                    (IOMUX_PINCM30)
/* Defines for BLB: GPIOB.16 with pinCMx 33 on package pin 4 */
#define encoder_jie_BLB_IIDX                                (DL_GPIO_IIDX_DIO16)
#define encoder_jie_BLB_PIN                                     (DL_GPIO_PIN_16)
#define encoder_jie_BLB_IOMUX                                    (IOMUX_PINCM33)
/* Defines for BRA: GPIOB.24 with pinCMx 52 on package pin 23 */
#define encoder_jie_BRA_IIDX                                (DL_GPIO_IIDX_DIO24)
#define encoder_jie_BRA_PIN                                     (DL_GPIO_PIN_24)
#define encoder_jie_BRA_IOMUX                                    (IOMUX_PINCM52)
/* Defines for BRB: GPIOB.27 with pinCMx 58 on package pin 29 */
#define encoder_jie_BRB_IIDX                                (DL_GPIO_IIDX_DIO27)
#define encoder_jie_BRB_PIN                                     (DL_GPIO_PIN_27)
#define encoder_jie_BRB_IOMUX                                    (IOMUX_PINCM58)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_PWM_1_init(void);
void SYSCFG_DL_UART_0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
