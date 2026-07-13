/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the LP_MSPM0G3507
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gPWM_0Backup;
DL_TimerG_backupConfig gPWM_1Backup;
DL_TimerG_backupConfig gPWM_ULTRASONICBackup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_PWM_0_init();
    SYSCFG_DL_PWM_1_init();
    SYSCFG_DL_PWM_ULTRASONIC_init();
    SYSCFG_DL_CAPTURE_ULTRASONIC_init();
    SYSCFG_DL_I2C_0_init();
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_DMA_init();
    /* Ensure backup structures have no valid state */
	gPWM_0Backup.backupRdy 	= false;
	gPWM_1Backup.backupRdy 	= false;
	gPWM_ULTRASONICBackup.backupRdy 	= false;



}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(PWM_0_INST, &gPWM_0Backup);
	retStatus &= DL_TimerG_saveConfiguration(PWM_1_INST, &gPWM_1Backup);
	retStatus &= DL_TimerG_saveConfiguration(PWM_ULTRASONIC_INST, &gPWM_ULTRASONICBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(PWM_0_INST, &gPWM_0Backup, false);
	retStatus &= DL_TimerG_restoreConfiguration(PWM_1_INST, &gPWM_1Backup, false);
	retStatus &= DL_TimerG_restoreConfiguration(PWM_ULTRASONIC_INST, &gPWM_ULTRASONICBackup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(PWM_0_INST);
    DL_TimerG_reset(PWM_1_INST);
    DL_TimerG_reset(PWM_ULTRASONIC_INST);
    DL_TimerG_reset(CAPTURE_ULTRASONIC_INST);
    DL_I2C_reset(I2C_0_INST);
    DL_UART_Main_reset(UART_0_INST);


    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(PWM_0_INST);
    DL_TimerG_enablePower(PWM_1_INST);
    DL_TimerG_enablePower(PWM_ULTRASONIC_INST);
    DL_TimerG_enablePower(CAPTURE_ULTRASONIC_INST);
    DL_I2C_enablePower(I2C_0_INST);
    DL_UART_Main_enablePower(UART_0_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_0_C0_IOMUX,GPIO_PWM_0_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_0_C0_PORT, GPIO_PWM_0_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_0_C1_IOMUX,GPIO_PWM_0_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_0_C1_PORT, GPIO_PWM_0_C1_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_1_C0_IOMUX,GPIO_PWM_1_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_1_C0_PORT, GPIO_PWM_1_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_1_C1_IOMUX,GPIO_PWM_1_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_1_C1_PORT, GPIO_PWM_1_C1_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWM_ULTRASONIC_C0_IOMUX,GPIO_PWM_ULTRASONIC_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWM_ULTRASONIC_C0_PORT, GPIO_PWM_ULTRASONIC_C0_PIN);

    DL_GPIO_initPeripheralInputFunction(GPIO_CAPTURE_ULTRASONIC_C0_IOMUX,GPIO_CAPTURE_ULTRASONIC_C0_IOMUX_FUNC);

    
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C_0_IOMUX_SDA, GPIO_I2C_0_IOMUX_SDA_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C_0_IOMUX_SCL, GPIO_I2C_0_IOMUX_SCL_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_0_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART_0_IOMUX_TX, GPIO_UART_0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART_0_IOMUX_RX, GPIO_UART_0_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(LED_LED0_IOMUX);

    DL_GPIO_initDigitalInputFeatures(KEY_k4_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY_k1_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY_k2_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(KEY_k3_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT1_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT2_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT3_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT4_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT5_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT6_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT7_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(Line_OUT8_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(tb6612_AIN1_F_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_BIN1_F_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_BIN2_F_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_AIN1_B_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_AIN2_B_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_BIN1_B_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_BIN2_B_IOMUX);

    DL_GPIO_initDigitalOutput(tb6612_AIN2_F_IOMUX);

    DL_GPIO_initDigitalOutput(NRF24L01_CE_IOMUX);

    DL_GPIO_initDigitalOutput(NRF24L01_CSN_IOMUX);

    DL_GPIO_initDigitalOutput(NRF24L01_SCK_IOMUX);

    DL_GPIO_initDigitalOutput(NRF24L01_MOSI_IOMUX);

    DL_GPIO_initDigitalInputFeatures(NRF24L01_MISO_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_FLA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_FLB_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_FRA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_FRB_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_BLA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_BLB_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_BRA_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(encoder_jie_BRB_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(BEEp_PIN_0_IOMUX);

    DL_GPIO_clearPins(GPIOA, tb6612_AIN1_F_PIN |
		tb6612_BIN1_F_PIN |
		tb6612_BIN2_F_PIN |
		tb6612_AIN2_F_PIN |
		NRF24L01_CE_PIN |
		NRF24L01_SCK_PIN |
		NRF24L01_MOSI_PIN |
		BEEp_PIN_0_PIN);
    DL_GPIO_setPins(GPIOA, NRF24L01_CSN_PIN);
    DL_GPIO_enableOutput(GPIOA, tb6612_AIN1_F_PIN |
		tb6612_BIN1_F_PIN |
		tb6612_BIN2_F_PIN |
		tb6612_AIN2_F_PIN |
		NRF24L01_CE_PIN |
		NRF24L01_CSN_PIN |
		NRF24L01_SCK_PIN |
		NRF24L01_MOSI_PIN |
		BEEp_PIN_0_PIN);
    DL_GPIO_clearPins(GPIOB, LED_LED0_PIN |
		tb6612_AIN1_B_PIN |
		tb6612_AIN2_B_PIN |
		tb6612_BIN1_B_PIN |
		tb6612_BIN2_B_PIN);
    DL_GPIO_enableOutput(GPIOB, LED_LED0_PIN |
		tb6612_AIN1_B_PIN |
		tb6612_AIN2_B_PIN |
		tb6612_BIN1_B_PIN |
		tb6612_BIN2_B_PIN);
    DL_GPIO_setLowerPinsPolarity(GPIOB, DL_GPIO_PIN_0_EDGE_FALL |
		DL_GPIO_PIN_1_EDGE_FALL |
		DL_GPIO_PIN_10_EDGE_FALL |
		DL_GPIO_PIN_11_EDGE_FALL |
		DL_GPIO_PIN_13_EDGE_FALL);
    DL_GPIO_setUpperPinsPolarity(GPIOB, DL_GPIO_PIN_21_EDGE_RISE |
		DL_GPIO_PIN_16_EDGE_FALL |
		DL_GPIO_PIN_24_EDGE_FALL |
		DL_GPIO_PIN_27_EDGE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, encoder_jie_FLA_PIN |
		encoder_jie_FLB_PIN |
		encoder_jie_FRA_PIN |
		encoder_jie_FRB_PIN |
		encoder_jie_BLA_PIN |
		encoder_jie_BLB_PIN |
		encoder_jie_BRA_PIN |
		encoder_jie_BRB_PIN);
    DL_GPIO_enableInterrupt(GPIOB, encoder_jie_FLA_PIN |
		encoder_jie_FLB_PIN |
		encoder_jie_FRA_PIN |
		encoder_jie_FRB_PIN |
		encoder_jie_BLA_PIN |
		encoder_jie_BLB_PIN |
		encoder_jie_BRA_PIN |
		encoder_jie_BRB_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
	.rDivClk2x              = 3,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 9,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_2
};

SYSCONFIG_WEAK bool SYSCFG_DL_SYSCTL_SYSPLL_init(void)
{
    bool fFCCRatioStatus = false;
    uint32_t fFCCSysoscCount;
    uint32_t fFCCPllCount;
    uint32_t fFCCRatio;

    DL_SYSCTL_setFCCPeriods( DL_SYSCTL_FCC_TRIG_CNT_01 );

    /* Measuring PLL. */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSPLLCLK2X);
    /* Get SYSPLL frequency using FCC */
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0);

    /* get measA= SYSPLLCLK2X freq wrt LFOSC*/
    fFCCPllCount = DL_SYSCTL_readFCC();

    /* Measuring SYSPLL Source */
    DL_SYSCTL_configFCC(DL_SYSCTL_FCC_TRIG_TYPE_RISE_RISE,
                        DL_SYSCTL_FCC_TRIG_SOURCE_LFCLK,
                        DL_SYSCTL_FCC_CLOCK_SOURCE_SYSOSC);
    /* Get SYSPLL frequency using FCC */
    DL_SYSCTL_startFCC();
    while (DL_SYSCTL_isFCCDone() == 0 );

    /* get measB= SYSOSC freq wrt LFOSC*/
    fFCCSysoscCount = DL_SYSCTL_readFCC();

    /* Get ratio of both measurements*/
    fFCCRatio = (fFCCPllCount * FLOAT_TO_INT_SCALE) / fFCCSysoscCount;
    /* Check ratio is within bounds*/
    if ((FCC_LOWER_BOUND <  fFCCRatio) && (fFCCRatio < FCC_UPPER_BOUND))
    {
        /* ratio is good for proceeding into application code. */
        fFCCRatioStatus = true;
    }

    return fFCCRatioStatus;
}
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);

    /*
     * [SYSPLL_ERR_01]
     * PLL Incorrect locking WA start.
     * Insert after every PLL enable.
     * This can lead an infinite loop if the condition persists
     * and can block entry to the application code.
     */

    while (SYSCFG_DL_SYSCTL_SYSPLL_init() == false)
    {
        /* Toggle SYSPLL enable to re-enable SYSPLL and re-check incorrect locking */
        DL_SYSCTL_disableSYSPLL();
        DL_SYSCTL_enableSYSPLL();

        /* Wait until SYSPLL startup is stabilized*/
        while ((DL_SYSCTL_getClockStatus() & SYSCTL_CLKSTATUS_SYSPLLGOOD_MASK) != DL_SYSCTL_CLK_STATUS_SYSPLL_GOOD){}
    }
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}


/*
 * Timer clock configuration to be sourced by  / 1 (80000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   80000000 Hz = 80000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gPWM_0ClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gPWM_0Config = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 3200,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_0_init(void) {

    DL_TimerA_setClockConfig(
        PWM_0_INST, (DL_TimerA_ClockConfig *) &gPWM_0ClockConfig);

    DL_TimerA_initPWMMode(
        PWM_0_INST, (DL_TimerA_PWMConfig *) &gPWM_0Config);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(PWM_0_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(PWM_0_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, 3200, DL_TIMER_CC_0_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(PWM_0_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(PWM_0_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_0_INST, 3200, DL_TIMER_CC_1_INDEX);

    DL_TimerA_enableClock(PWM_0_INST);


    
    DL_TimerA_setCCPDirection(PWM_0_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT );


}
/*
 * Timer clock configuration to be sourced by  / 1 (80000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   80000000 Hz = 80000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerG_ClockConfig gPWM_1ClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerG_PWMConfig gPWM_1Config = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 3200,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_1_init(void) {

    DL_TimerG_setClockConfig(
        PWM_1_INST, (DL_TimerG_ClockConfig *) &gPWM_1ClockConfig);

    DL_TimerG_initPWMMode(
        PWM_1_INST, (DL_TimerG_PWMConfig *) &gPWM_1Config);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(PWM_1_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(PWM_1_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(PWM_1_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_1_INST, 3200, DL_TIMER_CC_0_INDEX);

    DL_TimerG_setCaptureCompareOutCtl(PWM_1_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_1_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(PWM_1_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_1_INST, 3200, DL_TIMER_CC_1_INDEX);

    DL_TimerG_enableClock(PWM_1_INST);


    
    DL_TimerG_setCCPDirection(PWM_1_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT );


}
/*
 * Timer clock configuration to be sourced by  / 1 (80000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   320000 Hz = 80000000 Hz / (1 * (249 + 1))
 */
static const DL_TimerG_ClockConfig gPWM_ULTRASONICClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 249U
};

static const DL_TimerG_PWMConfig gPWM_ULTRASONICConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 64000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWM_ULTRASONIC_init(void) {

    DL_TimerG_setClockConfig(
        PWM_ULTRASONIC_INST, (DL_TimerG_ClockConfig *) &gPWM_ULTRASONICClockConfig);

    DL_TimerG_initPWMMode(
        PWM_ULTRASONIC_INST, (DL_TimerG_PWMConfig *) &gPWM_ULTRASONICConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(PWM_ULTRASONIC_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(PWM_ULTRASONIC_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(PWM_ULTRASONIC_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWM_ULTRASONIC_INST, 32000, DL_TIMER_CC_0_INDEX);

    DL_TimerG_enableClock(PWM_ULTRASONIC_INST);


    
    DL_TimerG_setCCPDirection(PWM_ULTRASONIC_INST , DL_TIMER_CC0_OUTPUT );


}



/*
 * Timer clock configuration to be sourced by BUSCLK /  (40000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   1000000 Hz = 40000000 Hz / (1 * (39 + 1))
 */
static const DL_TimerG_ClockConfig gCAPTURE_ULTRASONICClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 39U
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * CAPTURE_ULTRASONIC_INST_LOAD_VALUE = (50ms * 1000000 Hz) - 1
 */
static const DL_TimerG_CaptureConfig gCAPTURE_ULTRASONICCaptureConfig = {
    .captureMode    = DL_TIMER_CAPTURE_MODE_PULSE_WIDTH_UP,
    .period         = CAPTURE_ULTRASONIC_INST_LOAD_VALUE,
    .startTimer     = DL_TIMER_STOP,
    .edgeCaptMode   = DL_TIMER_CAPTURE_EDGE_DETECTION_MODE_RISING,
    .inputChan      = DL_TIMER_INPUT_CHAN_0,
    .inputInvMode   = DL_TIMER_CC_INPUT_INV_NOINVERT,
};

SYSCONFIG_WEAK void SYSCFG_DL_CAPTURE_ULTRASONIC_init(void) {

    DL_TimerG_setClockConfig(CAPTURE_ULTRASONIC_INST,
        (DL_TimerG_ClockConfig *) &gCAPTURE_ULTRASONICClockConfig);

    DL_TimerG_initCaptureMode(CAPTURE_ULTRASONIC_INST,
        (DL_TimerG_CaptureConfig *) &gCAPTURE_ULTRASONICCaptureConfig);
    DL_TimerG_enableClock(CAPTURE_ULTRASONIC_INST);

}

static const DL_I2C_ClockConfig gI2C_0ClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C_0_init(void) {

    DL_I2C_setClockConfig(I2C_0_INST,
        (DL_I2C_ClockConfig *) &gI2C_0ClockConfig);
    DL_I2C_disableAnalogGlitchFilter(I2C_0_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(I2C_0_INST);
    /* Set frequency to 400000 Hz*/
    DL_I2C_setTimerPeriod(I2C_0_INST, 9);
    DL_I2C_setControllerTXFIFOThreshold(I2C_0_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(I2C_0_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(I2C_0_INST);

    /* Configure DMA Event 1 */
    DL_I2C_enableDMAEvent(I2C_0_INST, DL_I2C_EVENT_ROUTE_1,
                          DL_I2C_DMA_INTERRUPT_CONTROLLER_TXFIFO_TRIGGER);
    /* Configure DMA Event 2 */
    DL_I2C_enableDMAEvent(I2C_0_INST, DL_I2C_EVENT_ROUTE_2,
                          DL_I2C_DMA_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER);

    /* Enable module */
    DL_I2C_enableController(I2C_0_INST);


}

static const DL_UART_Main_ClockConfig gUART_0ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART_0Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART_0_init(void)
{
    DL_UART_Main_setClockConfig(UART_0_INST, (DL_UART_Main_ClockConfig *) &gUART_0ClockConfig);

    DL_UART_Main_init(UART_0_INST, (DL_UART_Main_Config *) &gUART_0Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(UART_0_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_0_INST, UART_0_IBRD_40_MHZ_115200_BAUD, UART_0_FBRD_40_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(UART_0_INST,
                                 DL_UART_MAIN_INTERRUPT_RX |
                                 DL_UART_MAIN_INTERRUPT_TX);


    DL_UART_Main_enable(UART_0_INST);
}

static const DL_DMA_Config gDMA_CH_TXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_UNCHANGED,
    .srcIncrement   = DL_DMA_ADDR_INCREMENT,
    .destWidth      = DL_DMA_WIDTH_WORD,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = I2C_0_INST_DMA_TRIGGER_0,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH_TX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_CH_TX_CHAN_ID , (DL_DMA_Config *) &gDMA_CH_TXConfig);
}
static const DL_DMA_Config gDMA_CH_RXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_INCREMENT,
    .srcIncrement   = DL_DMA_ADDR_UNCHANGED,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_WORD,
    .trigger        = I2C_0_INST_DMA_TRIGGER_1,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH_RX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_CH_RX_CHAN_ID , (DL_DMA_Config *) &gDMA_CH_RXConfig);
}
SYSCONFIG_WEAK void SYSCFG_DL_DMA_init(void){
    SYSCFG_DL_DMA_CH_TX_init();
    SYSCFG_DL_DMA_CH_RX_init();
}


