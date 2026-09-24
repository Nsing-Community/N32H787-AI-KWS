/**
*     Copyright (c) 2025, Nations Technologies Inc.
* 
*     All rights reserved.
*
*     This software is the exclusive property of Nations Technologies Inc. (Hereinafter 
* referred to as NATIONS). This software, and the product of NATIONS described herein 
* (Hereinafter referred to as the Product) are owned by NATIONS under the laws and treaties
* of the People's Republic of China and other applicable jurisdictions worldwide.
*
*     NATIONS does not grant any license under its patents, copyrights, trademarks, or other 
* intellectual property rights. Names and brands of third party may be mentioned or referred 
* thereto (if any) for identification purposes only.
*
*     NATIONS reserves the right to make changes, corrections, enhancements, modifications, and 
* improvements to this software at any time without notice. Please contact NATIONS and obtain 
* the latest version of this software before placing orders.

*     Although NATIONS has attempted to provide accurate and reliable information, NATIONS assumes 
* no responsibility for the accuracy and reliability of this software.
* 
*     It is the responsibility of the user of this software to properly design, program, and test 
* the functionality and safety of any application made of this information and any resulting product. 
* In no event shall NATIONS be liable for any direct, indirect, incidental, special,exemplary, or 
* consequential damages arising in any way out of the use of this software or the Product.
*
*     NATIONS Products are neither intended nor warranted for usage in systems or equipment, any
* malfunction or failure of which may cause loss of human life, bodily injury or severe property 
* damage. Such applications are deemed, "Insecure Usage".
*
*     All Insecure Usage shall be made at user's risk. User shall indemnify NATIONS and hold NATIONS 
* harmless from and against all claims, costs, damages, and other liabilities, arising from or related 
* to any customer's Insecure Usage.

*     Any express or implied warranty with regard to this software or the Product, including,but not 
* limited to, the warranties of merchantability, fitness for a particular purpose and non-infringement
* are disclaimed to the fullest extent permitted by law.

*     Unless otherwise explicitly permitted by NATIONS, anyone may not duplicate, modify, transcribe
* or otherwise distribute this software for any purposes, in whole or in part.
*
*     NATIONS products and technologies shall not be used for or incorporated into any products or systems
* whose manufacture, use, or sale is prohibited under any applicable domestic or foreign laws or regulations. 
* User shall comply with any applicable export control laws and regulations promulgated and administered by 
* the governments of any countries asserting jurisdiction over the parties or transactions.
**/

/**
 * @file n32h7xx_dsi.c
 * @author Nations
 * @version From N32H7xx_Library.1.2.0
 *
 * @copyright Copyright (c) 2025, Nations Technologies Inc. All rights reserved.
 */ 
#include "n32h7xx_dsi.h"
#include "stdio.h"


static DSI_StateTypeDef DSI_ShortWrite(DSI_HandleTypeDef *hdsi, DSI_PKTCtrlTypeDef *PacketCtrl);
static DSI_StateTypeDef DSI_LongWrite(DSI_HandleTypeDef *hdsi, DSI_PKTCtrlTypeDef *PacketCtrl, uint8_t *ParametersTable);
static DSI_StateTypeDef DSI_Read(DSI_HandleTypeDef *hdsi, uint8_t *RxArray, uint32_t RxSize);

/**
*\*\name    DSI_EnableWrapper.
*\*\fun     Enable DSI Wrapper.
*\*\param   hdsi: dsi host instance.
*\*\param  cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_EnableWrapper(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        hdsi->InstanceWrap->DSI_WRPCTRL  &= ~DSI_WRPCTRL_TRIGREQ;
    }
    else
    {
        hdsi->InstanceWrap->DSI_WRPCTRL  |= DSI_WRPCTRL_TRIGREQ;
    }
}

/**
*\*\name    DSI_Disable.
*\*\fun     Disable DSI.
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
void DSI_Disable(DSI_HandleTypeDef *hdsi)
{
    hdsi->Instance->DSI_NUMLANES  = DSI_REGISTER_MASK;
}

/**
*\*\name    DSI_EnableClockLane.
*\*\fun     Enable or Disable DSI clock lane.
*\*\param   hdsi: dsi host instance.
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_EnableClockLane(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        hdsi->Instance->DSI_CLKLANEN  = DSI_CLKLANEN_CLKLANEN;
    }
    else
    {
        hdsi->Instance->DSI_CLKLANEN  &= ~DSI_CLKLANEN_CLKLANEN;
    }
}

/**
*\*\name    DSI_EnableDataLane.
*\*\fun     Configure DSI data lane number.
*\*\param   hdsi: dsi host instance.
*\*\param   datalinenum : 
*\*\          - DSI_NO_DATA_LENE
*\*\          - DSI_ONE_DATA_LANE
*\*\          - DSI_TWO_DATA_LANES
*\*\          - DSI_THREE_DATA_LANES
*\*\          - DSI_FOUR_DATA_LANES
*\*\return  None.
**/
void DSI_ConfigDataLaneNum(DSI_HandleTypeDef *hdsi, uint32_t datalinenum)
{
    hdsi->Instance->DSI_DATLANEN  = datalinenum;
}

/**
*\*\name    DSI_SetVCID.
*\*\fun     Configure DSI virtual channel ID.
*\*\param   hdsi: dsi host instance.
*\*\param   channel : 
*\*\return  None.
**/
void DSI_SetVCID(DSI_HandleTypeDef *hdsi, uint32_t channel)
{
    hdsi->Instance->VID_VC  = channel;
}


/**
*\*\name    DSI_DisableSkewCal.
*\*\fun     Disable DSI skew calibration .
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
void DSI_DisableSkewCal(DSI_HandleTypeDef *hdsi)
{
    hdsi->Instance->DSI_SKEWCALTIMI = DSI_REGISTER_MASK;
    hdsi->Instance->DSI_SKEWCALTIMP = DSI_REGISTER_MASK;
    hdsi->Instance->DSI_SKEWCALINE  = DSI_REGISTER_MASK;
}

/**
*\*\name    DSI_DeInit.
*\*\fun     De-initializes the DSI peripheral registers to their default reset
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
DSI_StateTypeDef DSI_DeInit(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef state = DSI_OK;

    if (hdsi != NULL)
    {
        /* Disable the DSI wrapper: write bit9 = 0 */
        DSI_EnableWrapper(hdsi, DISABLE);

        /* Disable the DSI host, no active lanes */
        DSI_Disable(hdsi);

        /* D-PHY clock lane and data lane disable */
        DSI_EnableClockLane(hdsi, DISABLE);
        DSI_ConfigDataLaneNum(hdsi, DSI_NO_DATA_LENE);

        /* Disable the skewcal*/
        DSI_DisableSkewCal(hdsi);

    }
    else
    {
        state = DSI_ERROR;
    }

    return state;
}

/**
*\*\name    DSI_InitHostWrap.
*\*\fun     Initialize DSI host and wrapper .
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
void DSI_InitHostWrap(DSI_HandleTypeDef *hdsi)
{
   /* Base address of DSI Host/Wrapper registers to be set before calling De-Init */
    hdsi->Instance     = DSI_HOST;
    hdsi->InstanceWrap = DSI_HOST_WRAPPER;
}

/**
*\*\name    DSI_GetPHYStatus.
*\*\fun     Get DSI PHY status.
*\*\param   hdsi: dsi host instance.
*\*\return  SET or RESET.
**/
FlagStatus DSI_GetPHYStatus(DSI_HandleTypeDef *hdsi)
{
    FlagStatus bitstatus = RESET;

    /* Clear PLL unlock state */
    hdsi->InstanceWrap->DSIPHY_PLLSTS = DSIPHY_PLLSTS_PLLUNLOCK;

    if ((hdsi->InstanceWrap->DSIPHY_PLLSTS & DSIPHY_PLLSTS_PHYREADY) != DSIPHY_PLLSTS_PHYREADY)
    {
        bitstatus = RESET;
    }
    else
    {
        bitstatus = SET;
    }
    
    /* Return the flag status */
    return bitstatus;
}

/**
*\*\name    DSI_InitHostWrap.
*\*\fun     Initialize DSI PHY .
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
void DSI_InitPHY(DSI_HandleTypeDef *hdsi, DSI_PHY_InitTypeDef *PHY_InitParams)
{
    uint32_t tmpReg;
    
    /*************************** PHY wrapper control register1(DSIPHY_CTRL1) ***************************/
    tmpReg = hdsi->InstanceWrap->DSIPHY_CTRL1;
    tmpReg &= ~((uint32_t)(DSIPHY_CTRL1_REFCKSEL | DSIPHY_CTRL1_EXTDCYCEL | DSIPHY_CTRL1_L4SEL | 
                           DSIPHY_CTRL1_L3SEL | DSIPHY_CTRL1_L2SEL | DSIPHY_CTRL1_L1SEL | DSIPHY_CTRL1_L0SEL));

    /* Configures Physical Lane control source */
    tmpReg |= ((uint32_t)(((uint32_t)(PHY_InitParams->L4_Swap_Sel_Value & 0x00000007U)) << 6U  | 
                          ((uint32_t)(PHY_InitParams->L3_Swap_Sel_Value & 0x00000007U)) << 9U  |
                          ((uint32_t)(PHY_InitParams->L2_Swap_Sel_Value & 0x00000007U)) << 12U |
                          ((uint32_t)(PHY_InitParams->L1_Swap_Sel_Value & 0x00000007U)) << 15U |
                          ((uint32_t)(PHY_InitParams->L0_Swap_Sel_Value & 0x00000007U)) << 18U));
                         
    /* Reference Clock Selection */
    tmpReg |= ((uint32_t)(((uint32_t)(PHY_InitParams->RefCLK_In_Sel & 0x00000007U)) << 26U));
    /* High speed transmitter timing parameter: EXTDCYCEL[2:0] */
    tmpReg |= ((uint32_t)(((uint32_t)(PHY_InitParams->EXTD_CYCLE_SEL & 0x00000007U)) << 29U));
    hdsi->InstanceWrap->DSIPHY_CTRL1 = tmpReg;
    
    
    /*********************** PHY wrapper control register2(DSIPHY_CTRL2), DLANE Timing ***********************/
    tmpReg = ((uint32_t)(((uint32_t)(PHY_InitParams->DLANE_HS_PER_TIME   & 0x000000FFU)) << 0U  | 
                         ((uint32_t)(PHY_InitParams->DLANE_HS_ZERO_TIME  & 0x000000FFU)) << 8U  |
                         ((uint32_t)(PHY_InitParams->DLANE_HS_TRAIL_TIME & 0x000000FFU)) << 16U |
                         ((uint32_t)(PHY_InitParams->CLANE_HS_PER_TIME   & 0x000000FFU)) << 24U));
    hdsi->InstanceWrap->DSIPHY_CTRL2 = tmpReg;
    
    /*********************** PHY wrapper control register3(DSIPHY_CTRL3), CLANE Timing ***********************/
    tmpReg = ((uint32_t)(((uint32_t)(PHY_InitParams->CLANE_HS_ZERO_TIME     & 0x000000FFU)) << 0U  | 
                         ((uint32_t)(PHY_InitParams->CLANE_HS_TRAIL_TIME    & 0x000000FFU)) << 8U  |
                         ((uint32_t)(PHY_InitParams->CLANE_HS_CLK_PRE_TIME  & 0x000000FFU)) << 16U |
                         ((uint32_t)(PHY_InitParams->CLANE_HS_CLK_POST_TIME & 0x000000FFU)) << 24U));
    hdsi->InstanceWrap->DSIPHY_CTRL3 = tmpReg;
    
    /*************************** PHY PLL Control Register 1 (DSIPHY_PLLCTRL1) ***************************/
    tmpReg = ((uint32_t)(((uint32_t)(PHY_InitParams->PLL_FBK_FRA7_0   & 0x000000FFU)) << 0U | 
                         ((uint32_t)(PHY_InitParams->PLL_FBK_FRA15_8  & 0x000000FFU)) << 8U |
                         ((uint32_t)(PHY_InitParams->PLL_FBK_FRA23_16 & 0x000000FFU)) << 16U));
    hdsi->InstanceWrap->DSIPHY_PLLCTRL1 = tmpReg;

    /*************************** PHY PLL Control Register 2 (DSIPHY_PLLCTRL2) ***************************/
    tmpReg = hdsi->InstanceWrap->DSIPHY_PLLCTRL2;
    tmpReg &= ~((uint32_t)(DSIPHY_PLLCTRL2_FBKINT | DSIPHY_PLLCTRL2_PREDIV));
    
    tmpReg |= ((uint32_t)(((uint32_t)(PHY_InitParams->PLL_FBK_INT & 0x000001FFU)) << 0U  | 
                          ((uint32_t)(PHY_InitParams->PLL_PRE_DIV & 0x00000003U)) << 9U));
    hdsi->InstanceWrap->DSIPHY_PLLCTRL2 = tmpReg;
                         
    if (PHY_InitParams->SSC_Enable)
    {
        /* Spread Spectrum Clocking function */
        tmpReg = hdsi->InstanceWrap->DSIPHY_PLLCTRL2;
        tmpReg &= ~((uint32_t)(DSIPHY_PLLCTRL2_SSCAMPOP));
        tmpReg |= ((uint32_t)(((uint32_t)(PHY_InitParams->PLL_SSC_DELTA7_0   & 0x000000FFU)) << 12U  | 
                              ((uint32_t)(PHY_InitParams->PLL_SSC_DELTA15_8  & 0x000000FFU)) << 20U |
                              ((uint32_t)(PHY_InitParams->PLL_SSC_DELTA17_16 & 0x00000003U)) << 28U));
        hdsi->InstanceWrap->DSIPHY_PLLCTRL2 = tmpReg;
        
        /*********************** DSI PHY PLL Control Register 3 (DSIPHY_PLLCTRL3) ***********************/
        tmpReg = ((uint32_t)(((uint32_t)(PHY_InitParams->PLL_SSC_DELTA_INIT7_0   & 0x000000FFU)) << 0U  | 
                             ((uint32_t)(PHY_InitParams->PLL_SSC_DELTA_INIT15_8  & 0x000000FFU)) << 8U |
                             ((uint32_t)(PHY_InitParams->PLL_SSC_DELTA_INIT17_16 & 0x00000003U)) << 16U |
                             ((uint32_t)(PHY_InitParams->PLL_SSC_PRD9_8          & 0x000003FFU)) << 18U |
                             ((uint32_t)(PHY_InitParams->SSC_Enable              & 0x00000001U)) << 28U));
        hdsi->InstanceWrap->DSIPHY_PLLCTRL3 = tmpReg;
    }
    else
    {
        /* No Process */
    }

    /* Config DSIPHY_PLLCTRL4 and DSIPHY_PLLCTRL5, added default lane impedance, only for high speed TX, LPDT does not use HS TX */
    hdsi->InstanceWrap->DSIPHY_PLLCTRL4 = 0x21084210U;
    hdsi->InstanceWrap->DSIPHY_PLLCTRL5 = 0x00084210U;
}

/**
*\*\name    DSI_Start.
*\*\fun     Initialize DSI PHY .
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
void DSI_Start(DSI_HandleTypeDef *hdsi)
{
    /* Enable clock lane */
    DSI_EnableClockLane(hdsi, ENABLE);
    
    /* Enable data lane */
    DSI_ConfigDataLaneNum(hdsi, hdsi->HostInit.NumOfLanes);
}

/**
*\*\name    DSI_EnterULPSData.
*\*\fun     Enter the ULPM (Ultra Low Power Mode) with the D-PHY PLL running(only data lanes are in ULPM)
*\*\param   hdsi: dsi host instance.
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  DSI_StateTypeDef.
**/
DSI_StateTypeDef DSI_EnterULPSData(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    uint32_t ulpsPram;
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    DSI_StateTypeDef state = DSI_OK;

    /* ULPS Request on Data Lanes */
    ulpsPram = ((uint32_t)(hdsi->HostInit.NumOfLanes << 1U));
    
    DSI_CofigWrapULPSDL(hdsi, ulpsPram, ENABLE);
    
    /* wait data lane enter ULPS */
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPCTRL_ULPSDLEN) != ulpsPram) && timeout--)
    {
    }
    
    if(timeout == 0xFFFFFFFF)
    {
        state = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }
    
    return state;
}

/**
*\*\name    DSI_EnterULPSClock.
*\*\fun     Enter the ULPM (Ultra Low Power Mode) with the C-PHY PLL running (only clock lanes are in ULPM)
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
DSI_StateTypeDef DSI_EnterULPSClock(DSI_HandleTypeDef *hdsi)
{
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    DSI_StateTypeDef state = DSI_OK;
    
    /* ULPS Request on Clock Lanes */
    DSI_EnableWrapULPSCL(hdsi, ENABLE);
    
    /* wait clock lane enter ULPS */
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPCTRL_ULPSCLEN) != DSI_WRPCTRL_ULPSCLEN) && timeout--)
    {
    }

    if(timeout == 0xFFFFFFFF)
    {
        state = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }
    
    return state;
}

/**
*\*\name    DSI_ExitULPSClock.
*\*\fun     Exit the ULPM (Ultra Low Power Mode) with the C-PHY PLL running (only clock lanes are in ULPM)
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
DSI_StateTypeDef DSI_ExitULPSClock(DSI_HandleTypeDef *hdsi)
{
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    DSI_StateTypeDef state = DSI_OK;

    /* Exit ULPS on Clock Lanes */
    DSI_EnableWrapULPSCL(hdsi, DISABLE);

    /* wait clock lane enter ULPS */
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPCTRL_ULPSCLEN) != 0) && timeout--)
    {
    }

    if(timeout == 0xFFFFFFFF)
    {
        state = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }
    
    return state;
}

/**
*\*\name    DSI_EnterULPS.
*\*\fun      Enter the ULPM (Ultra Low Power Mode)(both data and clock lanes are in ULPM)
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
DSI_StateTypeDef DSI_EnterULPS(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef result = DSI_OK;

    /* ULPS Request on Clock Lanes */
    result = DSI_EnterULPSClock(hdsi);
    
    if (result == DSI_OK)
    {
        /* ULPS Request on Data Lanes */
        result = DSI_EnterULPSData(hdsi, ENABLE);
    }
    else
    {
        /* No Process */
    }

    return result;
}

/**
*\*\name    DSI_ExitULPS.
*\*\fun     Exit the ULPM (Ultra Low Power Mode)(both data and clock lanes are in ULPM)
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_StateTypeDef.
**/
DSI_StateTypeDef DSI_ExitULPS(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef result = DSI_OK;

    /* Exit ULPS on Data Lanes */
    result = DSI_EnterULPSData(hdsi, DISABLE);
    
    if (result == DSI_OK)
    {
        /* Exit ULPS on Clock Lanes */
        result = DSI_ExitULPSClock(hdsi);
    }
    else
    {
        /* No Process */
    }

    return result;
}

/**
*\*\name    DSI_EnableWrapperISkewCal.
*\*\fun     Enable or disable Wrapper Initial skew calibration.
*\*\param   hdsi: dsi host instance.
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_EnableWrapperISkewCal(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    if(Cmd != DISABLE)
    {
        hdsi->InstanceWrap->DSI_WRPCTRL |= DSI_WRPCTRL_ISKEWCAL;
    }
    else
    {
        hdsi->InstanceWrap->DSI_WRPCTRL &= ~DSI_WRPCTRL_ISKEWCAL;
    }
}

/**
*\*\name    DSI_EnableWrapperPSkewCal.
*\*\fun     Enable or disable Wrapper Period skew calibration.
*\*\param   hdsi: dsi host instance.
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_EnableWrapperPSkewCal(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    if(Cmd != DISABLE)
    {
        hdsi->InstanceWrap->DSI_WRPCTRL |= DSI_WRPCTRL_PSKEWCAL;
    }
    else
    {
        hdsi->InstanceWrap->DSI_WRPCTRL &= ~DSI_WRPCTRL_PSKEWCAL;
    }
}

/**
*\*\name    DSI_EnableInitialSkewCali.
*\*\fun     Enable Wrapper Initialize skew calibration.
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
DSI_StateTypeDef DSI_EnableInitialSkewCalib(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef result = DSI_OK;
    uint32_t timeout = DSI_TIME_OUT_VALUE;

    /* Enable Initial Skew Calibration */
    DSI_EnableWrapperISkewCal(hdsi, ENABLE);

    /* Wait for Initial Skew Calibration DONE */ 
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPCTRL_ISKEWCAL) != DSI_WRPCTRL_ISKEWCAL) && timeout--)
    {
    }
    
    /* Write 1 to clear Initial Skew Calibration DONE status */
    hdsi->InstanceWrap->DSI_WRPSTS |= DSI_WRPSTS_ISKEWCALDN;
    
    if(timeout == 0xFFFFFFFF)
    {
        result = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }

    return result;
}

/**
*\*\name    DSI_EnableInitialSkewCali.
*\*\fun     Enable Wrapper Period skew calibration.
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
DSI_StateTypeDef DSI_EnablePeriodSkewCali(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef result = DSI_OK;
    uint32_t timeout = DSI_TIME_OUT_VALUE;

    /* Enable Periodic Skew Calibration */
    DSI_EnableWrapperPSkewCal(hdsi, ENABLE);

    /* Wait for Periodic Skew Calibration DONE */ 
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPSTS_PSKEWCALDN) != DSI_WRPSTS_PSKEWCALDN) && timeout--)
    {
    }

    /* Write 1 to clear Periodic Skew Calibration DONE status */
    hdsi->InstanceWrap->DSI_WRPSTS |= (DSI_WRPSTS_PSKEWCALDN);

    if(timeout == 0xFFFFFFFF)
    {
        result = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }

    return result;
}

/**
*\*\name    DSI_EnableInitialSkewCali.
*\*\fun     Enable Wrapper reset trigger.
*\*\param   hdsi: dsi host instance.
*\*\return  None.
**/
DSI_StateTypeDef DSI_EnableResetTrigger(DSI_HandleTypeDef *hdsi)
{
    DSI_StateTypeDef result = DSI_OK;
    uint32_t timeout = DSI_TIME_OUT_VALUE;

    /* Set Reset Trigger */
    hdsi->InstanceWrap->DSI_WRPCTRL &= ~DSI_WRPCTRL_TRIGSEND;
    /* Set Transmit trigger request */
    hdsi->InstanceWrap->DSI_WRPCTRL |= DSI_WRPCTRL_TRIGREQ;

    /* Wait for Transmit trigger request acknowledge status */ 
    while(((hdsi->InstanceWrap->DSI_WRPSTS & DSI_WRPSTS_TRIGACK) != DSI_WRPSTS_TRIGACK) && timeout--)
    {
    }
    
    /* Write 1 to clear Transmit trigger request acknowledge status */
    hdsi->InstanceWrap->DSI_WRPSTS |= DSI_WRPSTS_TRIGACK;

    if(timeout == 0xFFFFFFFF)
    {
        result = DSI_TIMEOUT;
    }
    else
    {
        /* No Process */
    }

    return result;
}

/**
*\*\name    DSI_Wrapper_ConfigInt
*\*\fun     Enables or disables the specified DSI Wrapper's interrupts.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_IT
*\*\          - DSI_WRAPPER_INT_BLKERRIEN
*\*\          - DSI_WRAPPER_INT_SPERRIEN
*\*\          - DSI_WRAPPER_INT_OVERRIEN
*\*\          - DSI_WRAPPER_INT_UNDERRIEN
*\*\param   Cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void DSI_Wrapper_ConfigInt(DSI_HandleTypeDef *hdsi, uint32_t DSI_IT, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the Interrupt sources */
        hdsi->InstanceWrap->DSI_WRPCTRL |= DSI_IT;
    }
    else
    {
        /* Disable the Interrupt sources */
        hdsi->InstanceWrap->DSI_WRPCTRL &= ~DSI_IT;
    }
}


/**
*\*\name    DSI_Wrapper_GetFlagStatus
*\*\fun     Checks whether the specified DSI Wraper flag is set or not.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_FLAG
*\*\          - DSI_WRAPPER_FLAG_ISKEWCALDN
*\*\          - DSI_WRAPPER_FLAG_PSKEWCALDN
*\*\          - DSI_WRAPPER_FLAG_TRIGACK
*\*\          - DSI_WRAPPER_FLAG_BLKERRIEN
*\*\          - DSI_WRAPPER_FLAG_SPERRIEN
*\*\          - DSI_WRAPPER_FLAG_OVERRIEN
*\*\          - DSI_WRAPPER_FLAG_UNDERRIEN
*\*\return  The new state of DSI Wrapper FLAG (SET or RESET).
**/
FlagStatus DSI_Wrapper_GetFlagStatus(DSI_HandleTypeDef *hdsi, uint32_t DSI_FLAG)
{
    FlagStatus bitstatus;

    if ((hdsi->InstanceWrap->DSI_WRPSTS & DSI_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}

/**
*\*\name    DSI_Wrapper_ClearFlag
*\*\fun     Clears the DSI Wrapper's pending flags.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_FLAG
*\*\          - DSI_WRAPPER_FLAG_ISKEWCALDN
*\*\          - DSI_WRAPPER_FLAG_PSKEWCALDN
*\*\          - DSI_WRAPPER_FLAG_TRIGACK
*\*\          - DSI_WRAPPER_FLAG_BLKERRIEN
*\*\          - DSI_WRAPPER_FLAG_SPERRIEN
*\*\          - DSI_WRAPPER_FLAG_OVERRIEN
*\*\          - DSI_WRAPPER_FLAG_UNDERRIEN
*\*\return  none.
**/
void DSI_Wrapper_ClearFlag(DSI_HandleTypeDef *hdsi, uint32_t DSI_FLAG)
{
    /* Clear the flags */
    hdsi->InstanceWrap->DSI_WRPSTS |= DSI_FLAG;
}

/**
*\*\name    DSI_Wrapper_GetIntStatus
*\*\fun     Checks whether the specified DSI Wrapper's interrupt has occurred or not.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_IT
*\*\          - DSI_WRAPPER_INT_BLKERRIEN
*\*\          - DSI_WRAPPER_INT_SPERRIEN
*\*\          - DSI_WRAPPER_INT_OVERRIEN
*\*\          - DSI_WRAPPER_INT_UNDERRIEN
*\*\return  The new state of DSI_IT (SET or RESET).
**/
INTStatus DSI_Wrapper_GetIntStatus(DSI_HandleTypeDef *hdsi, uint32_t DSI_IT)
{
    INTStatus bitstatus;

    if ((hdsi->InstanceWrap->DSI_WRPCTRL & DSI_IT) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    if ((bitstatus != (uint32_t)RESET) && ((hdsi->InstanceWrap->DSI_WRPSTS & (DSI_IT >> 2U)) != (uint32_t)RESET))
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }

    return bitstatus;
}


/**
*\*\name    DSI_Init.
*\*\fun     Initialize the dsi host according to the specified parameters in the DSI_HandleTypeDef.
*\*\param   hdsi: dsi host instance.
*\*\return  DSI_OK or DSI_ERROR.
**/
DSI_StateTypeDef DSI_Init(DSI_HandleTypeDef *hdsi)
{
    uint16_t i = 0, numLane = 0;
    DSI_StateTypeDef result = DSI_OK;
    
    if (hdsi != NULL)
    {
        /************************ Set the DSI HOST BASE parameter ************************/
        /* Continuous high-speed clock configuration */
        hdsi->Instance->DSI_CONTHSCLK = hdsi->ClockInit.ContinuousHSCLK;
        
        /* Configuration DPHY timing */
        hdsi->Instance->DSI_TPRE  = ((uint32_t)(hdsi->ClockInit.ClockBeforeHS & 0x000000FFU));
        hdsi->Instance->DSI_TPOST = ((uint32_t)(hdsi->ClockInit.ClockHS2LP & 0x000000FFU));
        hdsi->Instance->DSI_TXGAP = ((uint32_t)(hdsi->ClockInit.ClockLP2HS & 0x000000FFU));
        
        /* Continuous auto insert EOTP */
        hdsi->Instance->DSI_AUTOINSERT_EOTP = hdsi->HostInit.AutoInsertEOTP;
        
        /* Continuous disable RX CRC check */
        hdsi->Instance->DSI_DISRXCRCCHK = hdsi->HostInit.DisRXCRC;
        
        /* Continuous DPHY Twakeup timing parameter */
        hdsi->Instance->DSI_TWAKEUP = ((uint32_t)(hdsi->ClockInit.ClockExitULPS & 0x0007FFFFU));
        
        /* Continuous host disable burst */
        hdsi->Instance->DSI_DISBST = hdsi->HostInit.BurstSel;
        
        
        /************************ Set DSI HOST SKEWCAL Parameter ************************/
        hdsi->Instance->DSI_SKEWCALTIMI = ((uint32_t)(hdsi->SkewcalInit.ClockInitialSkewcal & 0x0000FFFFU));
        hdsi->Instance->DSI_SKEWCALTIMP = ((uint32_t)(hdsi->SkewcalInit.ClockPeriodicSkewcal & 0x0000FFFFU));
        hdsi->Instance->DSI_SKEWCALINE  = ((uint32_t)(hdsi->SkewcalInit.LinePeriodicSkewcal & 0x000000FFU));
        
        /* Config the num_lanes to enable the host controller */
        for (i = 0; i < 3U; i++)
        {
            if (((hdsi->HostInit.NumOfLanes >> i) & 0x01U) != 0U)
            {
                numLane++;
            }
            else
            {
                /* No Process */
            }
        }
        hdsi->Instance->DSI_NUMLANES = ((uint32_t)(numLane & 0x00000007U));
    }
    else
    {
        result = DSI_ERROR;
    }

    return result;
}

/**
*\*\name    DSI_ConfigVideoMode.
*\*\fun     Select video mode and configure the corresponding parameters
*\*\param   hdsi: dsi host instance.
*\*\param   VidCfg: pointer to a DSI_VidCfgTypeDef structure that contains the DSI video mode configuration parameters
*\*\return  DSI_OK or DSI_ERROR.
**/
DSI_StateTypeDef DSI_ConfigVideoMode(DSI_HandleTypeDef *hdsi, DSI_VidCfgTypeDef *VidCfg)
{
    DSI_StateTypeDef result = DSI_OK;
    
    if (hdsi != NULL && VidCfg != NULL)
    {
        /************************ Set DSI HOST VID Interface ************************/
        hdsi->Instance->VID_PIXPERPKT = ((uint32_t)(VidCfg->PixelsPerPacket & 0x0000FFFFU));

        hdsi->Instance->VID_PIXPLDSIZ = ((uint32_t)(VidCfg->PixelPayloadSize & 0x0000FFFFU));

        /* Forced set 1 MSB alignment  for videio mode */
        hdsi->Instance->VID_PIXALIGN = VID_PIXALIGN_PIXALIGN;
        
        hdsi->Instance->VID_PIXFMT = ((uint32_t)(VidCfg->PixelFormat & 0x0000003FU));
        
        
        hdsi->Instance->VID_VSYNCPOL    = VidCfg->VSPolarity;
        hdsi->Instance->VID_HSYNCPOL    = VidCfg->HSPolarity;
        hdsi->Instance->VID_VIDEOMOD    = VidCfg->Mode;
        hdsi->Instance->VID_OVERIDE     = VidCfg->Override;
        hdsi->Instance->VID_PKTPERLINE  = VidCfg->PacketsPerLine;
        hdsi->Instance->VID_BLLPMOD     = VidCfg->BLLPMode;
        hdsi->Instance->VID_NULLPKTBLLP = VidCfg->NULLPacketInBLLP;
        
        
        hdsi->Instance->VID_STD       = ((uint32_t)(VidCfg->DelayFromStart & 0x0000FFFFU));
        hdsi->Instance->VID_VC        = ((uint32_t)(VidCfg->VirtualChannel & 0x00000003U));
        hdsi->Instance->VID_VSSPLD    = ((uint32_t)(VidCfg->VerticalSyncStartPayload & 0x0000FFFFU));
        hdsi->Instance->VID_PLDPERPKT = ((uint32_t)(VidCfg->PayloadPerPacket & 0x0000FFFFU));
        
        hdsi->Instance->VID_EXTPKTEN = VidCfg->ExternalPacket;
        
        /* Only Override = 1 need to program timing register, Otherwise, the reset value is maintained */
        if (VidCfg->Override == VID_OVERIDE_OVERIDE)
        {
            hdsi->Instance->VID_HSA = ((uint32_t)(VidCfg->HorizontalSyncActive & 0x0000FFFFU));
            hdsi->Instance->VID_HBP = ((uint32_t)(VidCfg->HorizontalBackPorch & 0x0000FFFFU));
            hdsi->Instance->VID_HFP = ((uint32_t)(VidCfg->HorizontalFrontPorch & 0x0000FFFFU));

            hdsi->Instance->VID_VBP = ((uint32_t)(VidCfg->VerticalBackPorch & 0x0000FFFFU));
            hdsi->Instance->VID_VFP = ((uint32_t)(VidCfg->VerticalFrontPorch & 0x0000FFFFU));
            hdsi->Instance->VID_VACT = ((uint32_t)(VidCfg->VerticalActive & 0x0000FFFFU));
        }
        else
        {
            /* No Process */
        }

        hdsi->Instance->VID_EN = VID_EN_EN;
    }
    else
    {
        result = DSI_ERROR;
    }

    return result;
}

/**
*\*\name    DSI_ConfigTimeout.
*\*\fun     Configure DSI host timeout parameters.
*\*\param   hdsi: dsi host instance.
*\*\param   hstxcnt: 0~0xFFFFFF, Hight speed TX timeout count.
*\*\param   lprxcnt: 0~0xFFFFFF, Low power RX timeout count.
*\*\param   btacnt:  0~0xFFFFFF, Bus turn around timeout count.
*\*\return  DSI_OK.
**/
void DSI_ConfigTimeout(DSI_HandleTypeDef *hdsi, uint32_t hstxcnt, uint32_t lprxcnt, uint32_t btacnt)
{
    hdsi->Instance->DSI_HSTXTOCNT = ((uint32_t)(hstxcnt & 0x00FFFFFFU));
    hdsi->Instance->DSI_LRXTOCNT  = ((uint32_t)(lprxcnt & 0x00FFFFFFU));
    hdsi->Instance->DSI_BTATOCNT  = ((uint32_t)(btacnt & 0x00FFFFFFU));
}

/**
*\*\name    DSI_APBPKT_ShortWrite.
*\*\fun     write short DCS or short Generic command.
*\*\param   hdsi: dsi host instance.
*\*\param   ChannelID: Virtual channel ID.
*\*\param   Mode: DSI short packet data type.
*\*\              - DSI_DCS_SHORT_PKT_WRITE_P0
*\*\              - DSI_DCS_SHORT_PKT_WRITE_P1
*\*\              - DSI_GEN_SHORT_PKT_WRITE_P0
*\*\              - DSI_GEN_SHORT_PKT_WRITE_P1
*\*\              - DSI_GEN_SHORT_PKT_WRITE_P2
*\*\param   Speed: DSI short packet speed.
*\*\              - DSI_MODE_LPDT
*\*\              - DSI_MODE_HSDT
*\*\param   Param1: DSC command or first generic parameter.
*\*\param   Param2: DSC parameter or second generic parameter.
*\*\return  none
**/
DSI_StateTypeDef DSI_APBPKT_ShortWrite(DSI_HandleTypeDef *hdsi, uint32_t ChannelID, uint32_t Mode, uint32_t Speed, uint32_t Param1, uint32_t Param2)
{
    DSI_PKTCtrlTypeDef PacketCtrl;

    PacketCtrl.IsBTAEnable      = 0U;
    PacketCtrl.IsBTAOnly        = 0U;
    PacketCtrl.CmdType          = Speed;
    PacketCtrl.Header.ChannelID = ChannelID;
    PacketCtrl.Header.DataType  = Mode;
    PacketCtrl.Header.WordCount = (Param2 << 8U | (Param1 & 0xFFU));

    return DSI_ShortWrite(hdsi, &PacketCtrl);
}

/**
*\*\name    DSI_ShortWrite.
*\*\fun     write short DCS or short Generic command.
*\*\param   hdsi: dsi host instance.
*\*\param   PacketCtrl: Pointer to parameter DSI_PKTCtrlType struct.
*\*\return  DSI_State
**/
static DSI_StateTypeDef DSI_ShortWrite(DSI_HandleTypeDef *hdsi, DSI_PKTCtrlTypeDef *PacketCtrl)
{
    DSI_StateTypeDef result = DSI_OK;
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    
    /* Wait for DPHY direction is tx */
    while(((hdsi->Instance->DSI_PKTSTS & DSI_PKTSTS_DPHYDIR) == DSI_PKTSTS_DPHYDIR) && timeout--);
     
    if(timeout == 0xFFFFFFFF)
    {
        result = DSI_TIMEOUT;
    }
    else
    {
        timeout = DSI_TIME_OUT_VALUE;
        
        /* Configure the packet to send a short command with 0 or 1 parameter */
        hdsi->Instance->DSI_PKTCTRL = ((PacketCtrl->IsBTAOnly << 26U) | (PacketCtrl->IsBTAEnable << 25U)     | 
                                       (PacketCtrl->CmdType << 24U)   | (PacketCtrl->Header.DataType << 18U) | 
                                       (PacketCtrl->Header.ChannelID << 16U) | (PacketCtrl->Header.WordCount));
             
        /* Set TX send enable */
        hdsi->Instance->DSI_SENDPKT = DSI_SENDPKT_SENDPKT;
        
        if(PacketCtrl->IsBTAEnable == 0U)  
        {
            /* Wait for Machine state idle */
            while(((hdsi->Instance->DSI_PKTSTS & DSI_PKTSTS_NIDLE) == DSI_PKTSTS_NIDLE) && timeout--)
            {
            }
            
            if(timeout == 0xFFFFFFFF)
            {
                result = DSI_TIMEOUT;
            }
            else
            {
                /* No Process */
            }
        }
        else
        {
            /* No Process */
        }
    }
    
    return result;
}

/**
*\*\name    DSI_APBPKT_LongWrite.
*\*\fun     write long DCS or long Generic command
*\*\param   hdsi: dsi host instance.
*\*\param   ChannelID: Virtual channel ID.
*\*\param   Mode: DSI short packet data type.
*\*\              - DSI_DCS_LONG_PKT_WRITE
*\*\              - DSI_GEN_LONG_PKT_WRITE
*\*\param   Speed: DSI short packet speed.
*\*\              - DSI_MODE_LPDT
*\*\              - DSI_MODE_HSDT
*\*\param   ParametersTable: Pointer to parameter values table.
*\*\param   NbParams: Number of parameters.
*\*\return  DSI_State
**/
DSI_StateTypeDef DSI_APBPKT_LongWrite(DSI_HandleTypeDef *hdsi, uint32_t ChannelID, uint32_t Mode, uint32_t Speed, uint8_t *ParametersTable, uint32_t NbParams)
{
    DSI_StateTypeDef result = DSI_OK;
    DSI_PKTCtrlTypeDef PacketCtrl;

    if ((NbParams != 0U) && (ParametersTable == NULL))
    {
        result = DSI_ERROR;
    }
    else
    {
        PacketCtrl.IsBTAEnable      = 0;
        PacketCtrl.IsBTAOnly        = 0;
        PacketCtrl.CmdType          = Speed; 
        PacketCtrl.Header.ChannelID = ChannelID;
        PacketCtrl.Header.DataType  = Mode;
        PacketCtrl.Header.WordCount = NbParams;
        
        result = DSI_LongWrite(hdsi, &PacketCtrl, ParametersTable);
    }

    return result;
}

/**
*\*\name    DSI_LongWrite.
*\*\fun     write long DCS or long Generic command
*\*\param   hdsi: dsi host instance.
*\*\param   PacketCtrl: Pointer to parameter DSI_PKTCtrlType struct.
*\*\param   ParametersTable: Pointer to parameter values table.
*\*\return  DSI_State
**/
static DSI_StateTypeDef DSI_LongWrite(DSI_HandleTypeDef *hdsi, DSI_PKTCtrlTypeDef *PacketCtrl, uint8_t *ParametersTable)
{
    uint32_t fifoword, fifolast;
    uint32_t i = 0, wCount = 0;
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    DSI_StateTypeDef result = DSI_OK;
    

    /* Wait for DPHY direction is tx */
    while(((hdsi->Instance->DSI_PKTSTS & DSI_PKTSTS_DPHYDIR) == DSI_PKTSTS_DPHYDIR) && timeout--);
     
    if(timeout == 0xFFFFFFFFU)
    {
        result = DSI_TIMEOUT;
    }
    else
    {
        /* Configure the packet to send a long command */
        hdsi->Instance->DSI_PKTCTRL = ((PacketCtrl->IsBTAOnly << 26U) | (PacketCtrl->IsBTAEnable << 25U) | 
                                       (PacketCtrl->CmdType << 24U)   | (PacketCtrl->Header.DataType << 18U) | 
                                       (PacketCtrl->Header.ChannelID << 16U) | (PacketCtrl->Header.WordCount));
        
        /* Calculate 32-bit write count */
        wCount = PacketCtrl->Header.WordCount / 4U;
        
        for (i = 0U; i < wCount; i++)
        {
            fifoword = ((uint32_t *)ParametersTable)[i];
            
            hdsi->Instance->DSI_TXPLD = fifoword;
        }
        
        if ((PacketCtrl->Header.WordCount % 4U) != 0U)
        {
            fifolast = 0U;
            
            for (i = 0U; i < (PacketCtrl->Header.WordCount % 4U); i++)
            {
                fifolast |= (((uint32_t)(ParametersTable[((wCount * 4U) + i)])) << (i * 8U));
            }
            
            hdsi->Instance->DSI_TXPLD = fifolast;
        }
        else
        {
            /* No Process */
        }

        /* Set TX send enable */
        hdsi->Instance->DSI_SENDPKT = DSI_SENDPKT_SENDPKT;
        
        timeout = DSI_TIME_OUT_VALUE;
        /* wait until tx done */
        while(((hdsi->Instance->DSI_PKTSTS & DSI_PKTSTS_TXD) == DSI_PKTSTS_TXD) && timeout--)
        {
        }
        
        if(timeout == 0xFFFFFFFFU)
        {
            result = DSI_TIMEOUT;
        }
        else
        {
            /* No Process */
        }
    }
    
    
    return result;
}

/**
*\*\name    DSI_APBPKT_Read.
*\*\fun     Read command (DCS or generic)
*\*\param   hdsi: dsi host instance.
*\*\param   ChannelNbr: Virtual channel ID.
*\*\param   Array: Array pointer to a buffer to store the payload of a read back operation.
*\*\param   Size: Data size to be read (in byte).
*\*\param   Mode: DSI short packet data type.
*\*\              - DSI_DCS_SHORT_PKT_READ
*\*\              - DSI_GEN_SHORT_PKT_READ_P0
*\*\              - DSI_GEN_SHORT_PKT_READ_P1
*\*\              - DSI_GEN_SHORT_PKT_READ_P2
*\*\param   DCSCmd: DCS get/read command.
*\*\param   ParametersTable: Pointer to parameter values table.
*\*\return  DSI_State
**/
DSI_StateTypeDef DSI_APBPKT_Read(DSI_HandleTypeDef *hdsi, uint32_t ChannelNbr, uint8_t *Array, uint32_t Size, uint32_t Mode, uint32_t DCSCmd, uint8_t *ParametersTable)
{
    DSI_PKTCtrlTypeDef PacketCtrl;
    DSI_StateTypeDef status = DSI_OK;

    do
    {
        if ((hdsi == NULL) || (Array == NULL))
        {
            status = DSI_ERROR;
            break;
        }
        else
        {
            /* No Process */
        }
        
        /* step1: set max return packet size */
        if ((Size > 2U) && (status = DSI_APBPKT_ShortWrite(hdsi, ChannelNbr, DSI_MAX_RETURN_PKT_SIZE, DSI_MODE_LPDT, ((Size) & 0xFFU), (((Size) >> 8U) & 0xFFU))) != DSI_OK)
        {
            break;
        }
        else
        {
            /* No Process */
        }
        
        /* step2: send read cmd */
        PacketCtrl.IsBTAEnable = 1;    
        PacketCtrl.IsBTAOnly   = 0;
        PacketCtrl.CmdType     = DSI_MODE_LPDT;

        PacketCtrl.Header.ChannelID = ChannelNbr;
        PacketCtrl.Header.DataType  = Mode;
        
        /* step3: dsi rx secondly */
        if (Mode == DSI_DCS_SHORT_PKT_READ)
        {
            /* Data Type 0x06, DCS reads instruction, no parameters */
            PacketCtrl.Header.WordCount = (0x00U << 8U | (DCSCmd & 0xFFU));
            status = DSI_ShortWrite(hdsi, &PacketCtrl);
        }
        else if (Mode == DSI_GEN_SHORT_PKT_READ_P0)
        {
            /* Data Type 0x04, General read command, no parameters */
            PacketCtrl.Header.WordCount = 0U;
            status = DSI_ShortWrite(hdsi, &PacketCtrl);
        }
        else if (Mode == DSI_GEN_SHORT_PKT_READ_P1)
        {
            /* Data Type 0x14, General read command, one parameters */
            PacketCtrl.Header.WordCount = DCSCmd << 8U;
            status = DSI_ShortWrite(hdsi, &PacketCtrl);
            
            if (status == DSI_OK)
            {
                PacketCtrl.Header.DataType = DSI_EOTP;
                PacketCtrl.Header.WordCount = (0x0FU << 8U | 0x0FU);
                PacketCtrl.IsBTAEnable = 1U;    
                status = DSI_ShortWrite(hdsi, &PacketCtrl);
            }
            else
            {
                /* No Process */
            }
        }
        else if ((Mode == DSI_GEN_SHORT_PKT_READ_P2) && (ParametersTable != NULL))
        {
            /* Data Type 0x24, General read command, two parameters */
            PacketCtrl.Header.WordCount = (DCSCmd << 8U | ParametersTable[0U]);
            status = DSI_ShortWrite(hdsi, &PacketCtrl);
        }
        else
        {
            status = DSI_ERROR;
        }
        
        /* step4: dsi rx */
        if (status == DSI_OK)
        {
            status = DSI_Read(hdsi, (uint8_t *)Array, Size);
        }
        else
        {
            /* No Process */
        }
    } while (0);

    
    return status;
}

/**
*\*\name    DSI_APBPKT_Read_BTA.
*\*\fun     Read command (DCS or generic)
*\*\param   hdsi: dsi host instance.
*\*\param   ChannelNbr: Virtual channel ID.
*\*\param   Array: Array pointer to a buffer to store the payload of a read back operation.
*\*\param   Size: Data size to be read (in byte).
*\*\param   Mode: DSI short packet data type.
*\*\              - DSI_DCS_SHORT_PKT_READ
*\*\              - DSI_GEN_SHORT_PKT_READ_P0
*\*\              - DSI_GEN_SHORT_PKT_READ_P1
*\*\              - DSI_GEN_SHORT_PKT_READ_P2
*\*\return  DSI_State
**/
DSI_StateTypeDef DSI_APBPKT_Read_BTA(DSI_HandleTypeDef *hdsi, uint32_t ChannelNbr, uint8_t *Array, uint32_t Size, uint32_t Mode)
{
    DSI_PKTCtrlTypeDef PacketCtrl;
    DSI_StateTypeDef status = DSI_OK;
    
     do
    {
        if ((hdsi == NULL) || (Array == NULL))
        {
            status = DSI_ERROR;
            break;
        }
        else
        {
            /* No Process */
        }
        
        /* step1: set max return packet size */
        if ((Size > 2U) && (status = DSI_APBPKT_ShortWrite(hdsi, ChannelNbr, DSI_MAX_RETURN_PKT_SIZE, DSI_MODE_LPDT, ((Size) & 0xFFU), (((Size) >> 8U) & 0xFFU))) != DSI_OK)
        {
            break;
        }
        else
        {
            /* No Process */
        }
        
        /* step2: send read cmd */
        PacketCtrl.IsBTAEnable = 0U;
        PacketCtrl.IsBTAOnly   = 1U;
        PacketCtrl.CmdType     = DSI_MODE_LPDT;

        PacketCtrl.Header.ChannelID = ChannelNbr;
        PacketCtrl.Header.DataType  = Mode;

        /* step3: dsi rx secondly*/
        PacketCtrl.Header.WordCount = 0U;
        status = DSI_ShortWrite(hdsi, &PacketCtrl);
        
        /* step4: dsi rx */
        if (status != DSI_OK)
        {
            status = DSI_Read(hdsi, (uint8_t *)Array, Size);
        }
        else
        {
            /* No Process */
        }
    } while (0);
    
    return status;
}

/**
*\*\name    DSI_Read.
*\*\fun     DSI Read data
*\*\param   hdsi: dsi host instance.
*\*\param   RxArray: Array pointer to a buffer to store the payload of a read back operation.
*\*\param   RxSize: Data size to be read (in byte).
*\*\return  DSI_State
**/
static DSI_StateTypeDef DSI_Read(DSI_HandleTypeDef *hdsi, uint8_t *RxArray, uint32_t RxSize)
{
    uint16_t i = 0, j = 0;
    uint32_t fifoword = 0;
    uint32_t timeout = DSI_TIME_OUT_VALUE;
    DSI_PKTHeaderTypeDef RxPKTHeader;
    DSI_StateTypeDef status = DSI_OK;
    
    do
    {
        /* Wait until packet header has been received */
        while(((hdsi->Instance->DSI_PKTSTS & DSI_PKTSTS_RXPKTD) != DSI_PKTSTS_RXPKTD) && timeout--)
        {
        }
        
        if(timeout == 0xFFFFFFFFU)
        {
            status = DSI_TIMEOUT;
            break;
        }
        else
        {
            /* No Process */
        }
        
        /* Read rx packet header */
        RxPKTHeader.WordCount = (hdsi->Instance->DSI_PKTRXHDR & 0x0000FFFFU);
        RxPKTHeader.DataType  = ((hdsi->Instance->DSI_PKTRXHDR >> 16U) & 0x3FU);
        RxPKTHeader.ChannelID = ((hdsi->Instance->DSI_PKTRXHDR >> 22U) & 0x03U);
        
        if ((RxPKTHeader.DataType != 0x1AU) && (RxPKTHeader.DataType != 0x1CU))
        {
            for (i = 0; i < RxSize; i++)
            {
                /* Not DCS long-read response and General long-read response */
                RxArray[i] = ((uint8_t)((RxPKTHeader.WordCount >> (i * 8U)) & 0x00FFU));
            }
            
            break;
        }
        else
        {
            /* No Process */
        }
        
        for (i = 0; i < RxPKTHeader.WordCount; i += 4U)
        {
            fifoword = hdsi->Instance->DSI_PKTRXPLD;
            
            if ((i + 4U) < RxSize)
            {
                (*((uint32_t *)(RxArray + i))) = fifoword;
            }
            else
            {
                for (j = 0; j < (4U - ((i + 4U) - RxSize)); j++)
                {
                    RxArray[(i + j)] = ((uint8_t)((fifoword >> (j * 8U)) & 0x000000FFU));
                }
            }
        }
    } while (0);
    
    return status;
}



/**
*\*\name    DSI_GetSTS
*\*\fun     Get DSI flag status.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_FLAG (The input parameters must be the following values):
*\*\          - DSI_FLAG_SOT_ERROR
*\*\          - DSI_FLAG_SOTSYNC_ERROR
*\*\          - DSI_FLAG_EOTSYNC_ERROR
*\*\          - DSI_FLAG_ESCMOD_ERROR
*\*\          - DSI_FLAG_LPTXSYNC_ERROR
*\*\          - DSI_FLAG_TO_ERROR
*\*\          - DSI_FLAG_FCTRL_ERROR
*\*\          - DSI_FLAG_COTDET_ERROR
*\*\          - DSI_FLAG_ECCSB_ERROR
*\*\          - DSI_FLAG_ECCML_ERROR
*\*\          - DSI_FLAG_CRC_ERROR
*\*\          - DSI_FLAG_DATATYPE_ERROR
*\*\          - DSI_FLAG_VCIDINVLID_ERROR
*\*\          - DSI_FLAG_INVLID_TXLEN_ERROR
*\*\          - DSI_FLAG_PROVILT_ERROR
*\*\          - DSI_FLAG_BIT0_ERROR
*\*\          - DSI_FLAG_BIT1_ERROR
*\*\          - DSI_FLAG_BIT2_ERROR
*\*\          - DSI_FLAG_BIT3_ERROR
*\*\return  bitstatus
*\*\          - SET   
*\*\          - RESET 
**/
FlagStatus DSI_GetSTS(DSI_HandleTypeDef *hdsi, uint32_t DSI_FLAG)
{
    FlagStatus bitstatus;

    if ((hdsi->Instance->DSI_STS & DSI_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    
    return bitstatus;
}

/**
*\*\name    DSI_GetERRSTS
*\*\fun     Get DSI error flag status.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_FLAG (The input parameters must be the following values):
*\*\          - DSI_ERROR_FLAG_ECCSB
*\*\          - DSI_ERROR_FLAG_ECCML
*\*\          - DSI_ERROR_FLAG_CRC  
*\*\          - DSI_ERROR_FLAG_HTXTO
*\*\          - DSI_ERROR_FLAG_LRXTO
*\*\          - DSI_ERROR_FLAG_BATTO
*\*\return  bitstatus
*\*\          - SET   
*\*\          - RESET 
**/
FlagStatus DSI_GetERRSTS(DSI_HandleTypeDef *hdsi, uint32_t DSI_FLAG)
{
    FlagStatus bitstatus;

    if ((hdsi->Instance->DSI_ERRSTS & DSI_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    
    return bitstatus;
}

/**
*\*\name    DSI_GetPKTSTS
*\*\fun     Get DSI packet flag status.
*\*\param   hdsi: dsi host instance.
*\*\param   DSI_FLAG (The input parameters must be the following values):
*\*\          - DSI_PKT_FLAG_NIDLE    
*\*\          - DSI_PKT_FLAG_TXD      
*\*\          - DSI_PKT_FLAG_DPHYDIR  
*\*\          - DSI_PKT_FLAG_TXFOVER  
*\*\          - DSI_PKT_FLAG_TXFUNDER 
*\*\          - DSI_PKT_FLAG_RXFOVER  
*\*\          - DSI_PKT_FLAG_RXFUNDER 
*\*\          - DSI_PKT_FLAG_RXPKTD   
*\*\          - DSI_PKT_FLAG_ALLRXPKTD
*\*\return  bitstatus
*\*\          - SET   
*\*\          - RESET 
**/
FlagStatus DSI_GetPKTSTS(DSI_HandleTypeDef *hdsi, uint32_t DSI_FLAG)
{
    FlagStatus bitstatus;
    
    if ((hdsi->Instance->DSI_PKTSTS & DSI_FLAG) != (uint32_t)RESET)
    {
        bitstatus = SET;
    }
    else
    {
        bitstatus = RESET;
    }
    
    return bitstatus;
}



/**
*\*\name    DSI_CofigWrapULPSDL.
*\*\fun     Configure DSI wrapper data lane number.
*\*\param   hdsi: dsi host instance.
*\*\param   datalinenum : 
*\*\          - DSI_ULPS_NO_DATA_LANE
*\*\          - DSI_ULPS_ONE_DATA_LANE
*\*\          - DSI_ULPS_TWO_DATA_LANE
*\*\          - DSI_ULPS_THREE_DATA_LANE
*\*\          - DSI_ULPS_FOUR_DATA_LANE
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_CofigWrapULPSDL(DSI_HandleTypeDef *hdsi, uint32_t datalinenum, FunctionalState Cmd)
{
    if(Cmd != DISABLE)
    {
        hdsi->InstanceWrap->DSI_WRPCTRL |= datalinenum;
    }
    else
    {
        hdsi->InstanceWrap->DSI_WRPCTRL &= ~datalinenum;
    }
}

/**
*\*\name    DSI_EnableWrapULPSDL.
*\*\fun     Configure DSI wrapper data lane number.
*\*\param   hdsi: dsi host instance.
*\*\param   Cmd : 
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  None.
**/
void DSI_EnableWrapULPSCL(DSI_HandleTypeDef *hdsi, FunctionalState Cmd)
{
    if(Cmd != DISABLE)
    {
        hdsi->InstanceWrap->DSI_WRPCTRL |= DSI_WRPCTRL_ULPSCLEN;
    }
    else
    {
        hdsi->InstanceWrap->DSI_WRPCTRL &= ~DSI_WRPCTRL_ULPSCLEN;
    }
}
