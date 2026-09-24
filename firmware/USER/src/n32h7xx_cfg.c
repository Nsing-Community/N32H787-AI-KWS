/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file n32h7xx_cfg.c
 * @author N32cube
 */

#include "n32h7xx_cfg.h"
/* NTFx CODE START */
__IO uint32_t mwTick;
void SysTick_Delayms(uint32_t Delayms)
{
    uint32_t tickstart = mwTick;
    uint32_t wait=Delayms;
    /* Add 1 to guarantee minimum wait */
    if (wait < 0xFFFFFFFFU)
    {
        wait +=1;
    }
    while ((mwTick - tickstart) < wait)
    {
    }
}
 /**
 *@name  DMA_SetSrcDstAddr.
 *@brief Set peripher address and memory address of DMA
 *param   DMAy (The input parameters must be the following values):
 *          - DMA1
 *          - DMA2
 *          - DMA3
 *param   ChNum (The input parameters must be the following values):
 *          - DMA_CHANNEL_0
 *          - DMA_CHANNEL_1
 *          - DMA_CHANNEL_2
 *          - DMA_CHANNEL_3
 *          - DMA_CHANNEL_4
 *          - DMA_CHANNEL_5
 *          - DMA_CHANNEL_6
 *          - DMA_CHANNEL_7
 *@param SrcAddr   Source address
 *@param DstAddr   Destination address
 *@return status
 */
 void DMA_SetSrcDstAddr(DMA_Module *const DMAy,DMA_ChNumType ChNum, uint32_t SrcAddr,uint32_t DstAddr )
 {
    /* Sets channel n source address register */
    WRITE_REG(DMAy->CH[ChNum].SA, SrcAddr);
    /* Sets channel n destination address register */
    WRITE_REG(DMAy->CH[ChNum].DA, DstAddr);
 }
  /**
 *@name  MDMA_SetSrcDstAddr.
 *@brief Set peripher address and memory address of MDMA
 *param   MDMAy (The input parameters must be the following values):
 *          - MDMA
 *param   ChNum (The input parameters must be the following values):
 *          - MDMA_CHANNEL_0
 *          - MDMA_CHANNEL_1
 *          - MDMA_CHANNEL_2
 *          - MDMA_CHANNEL_3
 *          - MDMA_CHANNEL_4
 *          - MDMA_CHANNEL_5
 *          - MDMA_CHANNEL_6
 *          - MDMA_CHANNEL_7
 *          - MDMA_CHANNEL_8
 *          - MDMA_CHANNEL_9
 *          - MDMA_CHANNEL_10
 *          - MDMA_CHANNEL_11
 *          - MDMA_CHANNEL_12
 *          - MDMA_CHANNEL_13
 *          - MDMA_CHANNEL_14
 *          - MDMA_CHANNEL_15
 *@param SrcAddr   Source address
 *@param DstAddr   Destination address
 *@return status
 */
 void MDMA_SetSrcDstAddr(MDMA_Module *const MDMAy,MDMA_ChNumType ChNum, uint32_t SrcAddr,uint32_t DstAddr )
 {
    /* Sets channel n source address register */
    WRITE_REG(MDMAy->CH[ChNum].SA, SrcAddr);
    /* Sets channel n destination address register */
    WRITE_REG(MDMAy->CH[ChNum].DA, DstAddr);
 }
/* NTFx CODE END */

/* NTFx CODE START */
/* Bounded spin count for the power-domain ready flags.  SysTick is not
   running yet at this point, so a plain counter is used instead. */
#define PWR_READY_TIMEOUT  1000000U

/**
 *@brief Bring up the GRAPHICS sub power domain
 *@param null
 *@return status
 */
bool PWR_Configuration(void)
{
    uint32_t timeout;

    /* DVP1/DVP2 live in the VDDDMAIN GRAPHICS sub power domain together with
       GPU, LCDC, JPEG and MIPI-DSI.  The datasheet lists that domain's power
       switch as software controlled and off out of reset, so before this runs
       every DVP2 register reads back as zero and every write is dropped no
       matter how many RCC clock and reset bits are asserted -- which is
       indistinguishable from a dead DVP.

       This performs the same sequence as the driver's
       PWR_MoudlePowerEnable(GRAPHIC_DVP_PWRCTRL, ENABLE), but bounds the two
       ready waits.  The vendor version spins forever, and here that would
       hang the core before USART is up, leaving nothing to report. */
    PWR->IPMEMCTRL &= ~PWR_IPMEMCTRL_DVP_PWREN;
    for (timeout = PWR_READY_TIMEOUT; timeout > 0U; --timeout) {
        if ((PWR->IPMEMCTRLSTS & PWR_IPMEMCTRLSTS_DVP_PWRRDY) != 0U) {
            break;
        }
    }
    if (timeout == 0U) {
        return false;
    }

    if ((PWR->SYSCTRL3 & PWR_SYSCTRL3_GRC_PWRRDY) == 0U) {
        /* Select the switcher mode, then enable the domain power. */
        PWR->SYSCTRL3 |= PWR_SYSCTRL3_GRC_PSWACK1;
        PWR->SYSCTRL3 |= PWR_SYSCTRL3_GRC_PWREN;
        for (timeout = PWR_READY_TIMEOUT; timeout > 0U; --timeout) {
            if ((PWR->SYSCTRL3 & PWR_SYSCTRL3_GRC_PWRRDY) != 0U) {
                break;
            }
        }
        if (timeout == 0U) {
            return false;
        }
        /* Set the domain out of reset and release its output isolation. */
        PWR->SYSCTRL3 |= PWR_SYSCTRL3_GRC_FUCEN;
        PWR->SYSCTRL3 |= PWR_SYSCTRL3_GRC_ISNEN;
    }

    return true;
}

/**
 *@brief Initializes the clock tree
 *@param null
 *@return status
 */
bool RCC_Configuration(void)
{
    ErrorStatus ClockStatus;
     
    RCC_ConfigHSIclkDivider(RCC_HSICLK_DIV1);
    RCC_EnableHsi(ENABLE);
    /* Wait till HSI is ready */
    ClockStatus = RCC_WaitHsiStable();
    if (ClockStatus != SUCCESS) return false;
    RCC_ConfigSysclkDivider(RCC_SYSCLK_DIV1);
    RCC_ConfigSysbusDivider(RCC_BUSCLK_DIV2);
    /*Configures the Periph clock source as HSI*/
    RCC_ConfigPeriphClk(RCC_PERIPHCLK_SRC_HSI);
     
    /* Configure APB1 clock is AHB1/2 = 150.000M */
    /* Configure APB2 clock is AHB2/2 = 150.000M */
    /* Configure APB5 clock is AHB5/2 = 150.000M */
    /* Configure APB6 clock is AHB6/2 = 150.000M */
    RCC_ConfigAPBclkDivider(RCC_APB1CLK_DIV2, RCC_APB2CLK_DIV2, RCC_APB5CLK_DIV2, RCC_APB6CLK_DIV2);
    /*Configures the PLL1 clock source and multiplication factor,Fin=64M,Fout=600M*/
    //RCC_ConfigPll1(RCC_PLL_SRC_HSI,64000000,600000000,ENABLE);
    RCC_ConfigPll1_NoCalculate(RCC_PLL_SRC_HSI,0,153600,3,ENABLE);
    /*Configure PLL1 divider value to Pll1A*/
    RCC_ConfigPLL1ADivider(RCC_PLLA_DIV1);
    /*Configure PLL1 divider value to Pll1B*/
    RCC_ConfigPLL1BDivider(RCC_PLLB_DIV2);
    /*Configure PLL1 divider value to Pll1C*/
    RCC_ConfigPLL1CDivider(RCC_PLLC_DIV2);
    /*Configure AXI clock source and divider*/
    RCC_ConfigAXIClk(RCC_AXIHYPERCLK_SRC_PLL1A);
    RCC_ConfigAXIclkDivider(RCC_AXICLK_DIV2);
    /*Configure M7 clock source and divider*/
    RCC_ConfigM7SystickClkDivider(RCC_STCLK_DIV1);
    RCC_ConfigM7Clk(RCC_M7HYPERCLK_SRC_PLL1A);
    /* configure sys_clk source is PLL1A */
    RCC_ConfigSysclk(RCC_SYSCLK_SRC_PLL1A);
    /* Check if sys_clk source is PLL1A */
    while(RCC_GetSysclkSrc() != RCC_SYSCLK_STS_PLL1A);

    /* I2C4 carries the WM8978 codec control bus. */
    RCC_ConfigI2C4_6_KerSysDivider(RCC_I2CKERCLK_SYSBUSDIV8);
    RCC_ConfigI2C4KerClkSource(RCC_I2CKERCLK_SRC_SYSBUSDIV);
    RCC_ConfigUSARTPClk(RCC_USARTPCLK_AHB1_DIV1);

    /*Config the TRNG clock*/
    RCC_ConfigTRNGClk(RCC_TRNGCLK_SRC_SYSBUSDIV,RCC_TRNGCLK_SYSBUSDIV2);

    RCC_EnableAPB2PeriphClk2(RCC_APB2_PERIPHEN_M7_I2C4, ENABLE);
    RCC_EnableAPB1PeriphClk3(RCC_APB1_PERIPHEN_M7_USART1, ENABLE);
    
     
/* NTFx CODE END */

    return true;
}
/* NTFx CODE START */
/**
 *@brief Initializes the NVIC
 *@param null
 *@return status
 */
bool NVIC_Configuration(void)
{
     
/* NTFx CODE END */

    return true;
}
/* NTFx CODE START */
/**
 *@brief Initializes the DMA
 *@param null
 *@return status
 */
bool DMA_Configuration(void)
{
     
/* NTFx CODE END */

    return true;
}
/* NTFx CODE START */
/**
 *@brief Initializes the GPIO
 *@param null
 *@return status
 */
bool GPIO_Configuration(void)
{
    GPIO_InitType GPIO_InitStructure;
    GPIO_InitStruct(&GPIO_InitStructure);

    /* Indicator LEDs, I2C4, USART1 and the I2S pins' GPIO port clocks. */
    RCC_EnableAHB5PeriphClk1(RCC_AHB5_PERIPHEN_M7_GPIOB |
                              RCC_AHB5_PERIPHEN_M7_GPIOA |
                              RCC_AHB5_PERIPHEN_M7_GPIOC |
                              RCC_AHB5_PERIPHEN_M7_GPIOD |
                              RCC_AHB5_PERIPHEN_M7_GPIOE |
                              RCC_AHB5_PERIPHEN_M7_GPIOF |
                              RCC_AHB5_PERIPHEN_M7_GPIOG |
                              RCC_AHB5_PERIPHEN_M7_GPIOH, ENABLE);
    RCC_EnableAHB5PeriphClk2(RCC_AHB5_PERIPHEN_M7_GPIOI |
                              RCC_AHB5_PERIPHEN_M7_GPIOK, ENABLE);
    RCC_EnableAHB5PeriphClk2(RCC_AHB5_PERIPHEN_M7_AFIO,ENABLE);

    /* PB3/PF10/PI8 report classifier state and are active-low.  Drive all
       high (off) before selecting output mode. */
    GPIO_SetBits(GPIOB, GPIO_PIN_3);
    GPIO_SetBits(GPIOF, GPIO_PIN_10);
    GPIO_SetBits(GPIOI, GPIO_PIN_8);
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructure.GPIO_Pull      = GPIO_PULL_UP;
    GPIO_InitStructure.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    GPIO_InitStructure.GPIO_Current   = GPIO_DC_2mA;
    GPIO_InitStructure.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitStructure.Pin            = GPIO_PIN_3;
    GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.Pin            = GPIO_PIN_10;
    GPIO_InitPeripheral(GPIOF, &GPIO_InitStructure);

    /* PI8 is a 5 V tolerant I/O; retain the Cube-generated drive setting. */
    GPIO_InitStructure.GPIO_Current   = GPIO_5VTOL_DC_1mA;
    GPIO_InitStructure.Pin            = GPIO_PIN_8;
    GPIO_InitPeripheral(GPIOI, &GPIO_InitStructure);

    /* I2S1's CK/WS pins (PA5, PG10) and I2S4's MCLK output are configured by
       wm8978_n32_init(), which owns the codec's clock tree. */

    /* I2C4 SCCB: PD12=SCL (AF10), PD13=SDA (AF8).  Both pins must be
       alternate-function open-drain.  Driving SDA low for the ACK and data
       bits is a peripheral output job, so an input-mode pin can never work
       here even though GPIO_InitPeripheral still writes the AF field. */
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStructure.GPIO_Pull      = GPIO_PULL_UP;
    GPIO_InitStructure.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    GPIO_InitStructure.GPIO_Current   = GPIO_DC_2mA;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF10;
    GPIO_InitStructure.Pin            = GPIO_PIN_12;
    GPIO_InitPeripheral(GPIOD, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF8;
    GPIO_InitStructure.Pin            = GPIO_PIN_13;
    GPIO_InitPeripheral(GPIOD, &GPIO_InitStructure);

    /* NSLink virtual COM port: USART1 TX=PA9, RX=PA10. */
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Pull      = GPIO_PULL_UP;
    GPIO_InitStructure.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    GPIO_InitStructure.GPIO_Current   = GPIO_DC_2mA;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF7;
    GPIO_InitStructure.Pin            = GPIO_PIN_9;
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

    /* USART1 RX.  SDRAM pins are owned by board_sdram.c. */
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_INPUT;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF5;
    GPIO_InitStructure.Pin            = GPIO_PIN_10;
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

     
/* NTFx CODE END */

    return true;
}

bool I2C_Configuration(void)
{
    I2C_InitType I2C_InitStructure;
    I2C_InitStruct(&I2C_InitStructure);
    I2C_InitStructure.Timing           = 0xD093DA;
    I2C_InitStructure.HSTiming         = 0x0;
    I2C_InitStructure.OwnAddress1      = 0x0;
    I2C_InitStructure.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    I2C_InitStructure.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    I2C_InitStructure.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    I2C_InitStructure.NoStretchMode    = I2C_NOSTRCH_DISABLE;
    I2C_Init(I2C4, &I2C_InitStructure);
    I2C_Enable(I2C4, ENABLE);
    return true;
}

#define USART1_BAUD 921600U

bool USART_Configuration(void)
{
    USART_InitType usart;
    USART_StructInit(&usart);
    USART_DeInit(USART1);
    /* The NSLink CDC bridge takes its UART rate from the host's line coding,
       so this must match what the host-side reader opens the port with --
       kws_uart_putc() is polled, so nothing is buffered here.  The keyword
       log is a few bytes per detection, so the rate is not a constraint. */
    usart.BaudRate = USART1_BAUD;
    usart.WordLength = USART_WL_8B;
    usart.StopBits = USART_STPB_1;
    usart.Parity = USART_PE_NO;
    usart.HardwareFlowControl = USART_HFCTRL_NONE;
    usart.Mode = USART_MODE_RX | USART_MODE_TX;
    usart.OverSampling = USART_16OVER;
    USART_Init(USART1, &usart);
    USART_Enable(USART1, ENABLE);
    return true;
}
