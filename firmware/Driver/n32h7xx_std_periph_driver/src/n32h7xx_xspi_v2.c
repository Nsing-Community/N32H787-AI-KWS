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
 * @file n32h7xx_xspi_v2.c
 * @author Nations
 * @version From N32H7xx_Library.1.2.0
 *
 * @copyright Copyright (c) 2025, Nations Technologies Inc. All rights reserved.
 */#include "n32h7xx_xspi_v2.h"
#include "n32h7xx_rcc.h"

/** N32H7xx StdPeriph_Driver **/

#define XSPI_CTRL0_MASK          ((uint32_t)(XSPI_V2_CTRL0_MST | XSPI_V2_CTRL0_SPIFRF | XSPI_V2_CTRL0_SSTE | XSPI_V2_CTRL0_TMOD | XSPI_V2_CTRL0_SCPOL | XSPI_V2_CTRL0_SCPH | XSPI_V2_CTRL0_DFS))
#define XSPI_RX_DELAY_MASK       ((uint32_t)(XSPI_V2_RX_DELAY_SES | XSPI_V2_RX_DELAY_SDCN))

#define XSPI_ENH_CTRL0_BASE_MASK ((uint32_t)(XSPI_V2_ENH_CTRL0_CLKSTREN | XSPI_V2_ENH_CTRL0_WAITCYCLES | XSPI_V2_ENH_CTRL0_INSTL | XSPI_V2_ENH_CTRL0_ADDRLEN | XSPI_V2_ENH_CTRL0_TRANSTYPE))

#define XSPI_ENH_CTRL0_DDR_MASK  ((uint32_t)(XSPI_V2_ENH_CTRL0_SPIDMEN | XSPI_V2_ENH_CTRL0_SPIRXDSEN | XSPI_V2_ENH_CTRL0_WRINDDREN | XSPI_V2_ENH_CTRL0_WRSPIDDREN))

#define XSPI_XIP_CTRL_BASE_MASK  ((uint32_t)(XSPI_V2_XIP_CTRL_XIPPREEN | XSPI_V2_XIP_CTRL_XIPMBL | XSPI_V2_XIP_CTRL_RXDSSIGEN | XSPI_V2_XIP_CTRL_XIPHYPEEN | XSPI_V2_XIP_CTRL_XIPCTEN | \
                                             XSPI_V2_XIP_CTRL_XIPINSTEN | XSPI_V2_XIP_CTRL_DFSHC | XSPI_V2_XIP_CTRL_WAITCYCLES | XSPI_V2_XIP_CTRL_MDBITSEN | XSPI_V2_XIP_CTRL_INSTL |   \
                                             XSPI_V2_XIP_CTRL_ADDRL | XSPI_V2_XIP_CTRL_TRANSTYPE | XSPI_V2_XIP_CTRL_FRF))

#define XSPI_XIP_CTRL_DDR_MASK   ((uint32_t)(XSPI_V2_XIP_CTRL_RXDSEN | XSPI_V2_XIP_CTRL_WRINDDREN | XSPI_V2_XIP_CTRL_DDREN))

#define XSPI_XIP_WRITE_CTRL_MASK ((uint32_t)(XSPI_V2_XIP_WRITE_CTRL_XIPWRWCY | XSPI_V2_XIP_WRITE_CTRL_XIPWRRXDSSIGEN | XSPI_V2_XIP_WRITE_CTRL_XIPWRHYPEEN | XSPI_V2_XIP_WRITE_CTRL_WRINDDREN | \
                                             XSPI_V2_XIP_WRITE_CTRL_WRSPIDDREN | XSPI_V2_XIP_WRITE_CTRL_WRINSTL | XSPI_V2_XIP_WRITE_CTRL_WRADDRL | XSPI_V2_XIP_WRITE_CTRL_WRTRTYPE | XSPI_V2_XIP_WRITE_CTRL_WRFRF))


/**
*\*\name    XSPI_DeInit.
*\*\fun     Deinitializes the XSPI peripheral registers to its default reset values.
*\*\return  none
**/
void XSPI_DeInit(void)
{
    RCC_EnableAXIPeriphReset4(RCC_AXI_PERIPHEN_M7_XSPI2|RCC_AXI_PERIPHEN_M4_XSPI2);
    RCC_EnableAXIPeriphReset4(RCC_AXI_PERIPHEN_M7_XSPI2LP | RCC_AXI_PERIPHEN_M4_XSPI2LP);
}

/**
*\*\name    XSPI_InitBase.
*\*\fun     Initilizes the Standard SPI Master mode or Slave mode.
*\*\param   SCPH: 
*\*\          - XSPI_SCPH_FIRST_EDGE
*\*\          - XSPI_SCPH_SECOND_EDGE
*\*\param   SCPOL: 
*\*\          - XSPI_SCPOL_LOW_LEVEL
*\*\          - XSPI_SCPOL_HIGH_LEVEL
*\*\param   FrameFormat: 
*\*\          - XSPI_STANDARD_MODE   
*\*\          - XSPI_DUAL_LINE_MODE  Note: Only Slave mode can used
*\*\          - XSPI_QUAD_LINE_MODE  Note: Only Slave mode can used
*\*\          - XSPI_OCTAL_LINE_MODE Note: Only Slave mode can used
*\*\param   DataFrameSize: 
*\*\          - XSPI_FRAME_SIZE_4_BIT
*\*\          - XSPI_FRAME_SIZE_5_BIT
*\*\          - XSPI_FRAME_SIZE_6_BIT
*\*\          - XSPI_FRAME_SIZE_7_BIT
*\*\          - XSPI_FRAME_SIZE_8_BIT
*\*\          - XSPI_FRAME_SIZE_9_BIT
*\*\          - XSPI_FRAME_SIZE_10_BIT
*\*\          - XSPI_FRAME_SIZE_11_BIT
*\*\          - XSPI_FRAME_SIZE_12_BIT
*\*\          - XSPI_FRAME_SIZE_13_BIT
*\*\          - XSPI_FRAME_SIZE_14_BIT
*\*\          - XSPI_FRAME_SIZE_15_BIT
*\*\          - XSPI_FRAME_SIZE_16_BIT
*\*\          - XSPI_FRAME_SIZE_17_BIT
*\*\          - XSPI_FRAME_SIZE_18_BIT
*\*\          - XSPI_FRAME_SIZE_19_BIT
*\*\          - XSPI_FRAME_SIZE_20_BIT
*\*\          - XSPI_FRAME_SIZE_21_BIT
*\*\          - XSPI_FRAME_SIZE_22_BIT
*\*\          - XSPI_FRAME_SIZE_23_BIT
*\*\          - XSPI_FRAME_SIZE_24_BIT
*\*\          - XSPI_FRAME_SIZE_25_BIT
*\*\          - XSPI_FRAME_SIZE_26_BIT
*\*\          - XSPI_FRAME_SIZE_27_BIT
*\*\          - XSPI_FRAME_SIZE_28_BIT
*\*\          - XSPI_FRAME_SIZE_29_BIT
*\*\          - XSPI_FRAME_SIZE_30_BIT
*\*\          - XSPI_FRAME_SIZE_31_BIT
*\*\          - XSPI_FRAME_SIZE_32_BIT
*\*\param   TransferMode: 
*\*\          - XSPI_TX_AND_RX_MODE
*\*\          - XSPI_TX_ONLY_MODE
*\*\          - XSPI_RX_ONLY_MODE
*\*\          - XSPI_EEPROM_READ_MODE
*\*\param   Role: 
*\*\          - XSPI_SLAVE_ROLE
*\*\          - XSPI_MASTER_ROLE
*\*\param   Baudr: 
*\*\          - Any even number between 2 and 65534
*\*\param   RxdSamplingEdge: 
*\*\          - XSPI_RXD_HCLK_RISING_SAMPLING
*\*\          - XSPI_RXD_HCLK_FALLING_SAMPLING
*\*\param   RxdSampleDelay: 
*\*\          - Valid values 0 ~ 255, measured in HCLK
*\*\param   NssToggle: 
*\*\          - XSPI_NSS_TOGGLE_DISABLE
*\*\          - XSPI_NSS_TOGGLE_ENABLE
*\*\param   EnhSlave_ClockStrech: 
*\*\          - XSPI_CLOCK_STRETCH_DISABLE
*\*\          - XSPI_CLOCK_STRETCH_ENABLE
*\*\return  none
**/
void XSPI_InitBase(XSPI_Module *xSPIx, XSPI_InitType* XSPI_StandInitStruct)
{
    uint32_t tmpregister = 0;

    /*---------------------------- XSPI CTRL0 Configuration ------------------------*/
    /* Get the XSPI CTRL0 value */
    tmpregister = xSPIx->CTRL0;

    /* Clear Control bit */
    tmpregister &= ~XSPI_CTRL0_MASK;

    /* Configure: Data Frame Size 
     * Configure: Serial Clock Phase
     * Configure: Serial Clock Polarity
     * Configure: Transfer Mode
     * Configure: Master or Slave mode 
     */
    tmpregister |= ((uint32_t)(XSPI_StandInitStruct->DataFrameSize | XSPI_StandInitStruct->SCPH 
                     | XSPI_StandInitStruct->SCPOL | XSPI_StandInitStruct->TransferMode
                     | XSPI_StandInitStruct->Role | XSPI_StandInitStruct->FrameFormat));
                     
    /* Configure: Slave Select Toggle Enable */
    if (XSPI_StandInitStruct->SCPH == XSPI_SCPH_FIRST_EDGE)
    {
        tmpregister |= XSPI_StandInitStruct->NssToggle;
    }
    else
    {
        /* No Process */
    }

    /* Write to XSPI CTRL0 */
    xSPIx->CTRL0 = tmpregister;

    /* Set the XSPI BAUD value */ 
    xSPIx->BAUD = ((uint32_t)((XSPI_StandInitStruct->Baudr / 2U) << 1U));

    if ((XSPI_StandInitStruct->Role == XSPI_MASTER_ROLE) && (XSPI_StandInitStruct->TransferMode != XSPI_TX_ONLY_MODE))
    {
        /* Configure: Receive Data (rxd) Sampling Edge
         * Configure: Receive Data (rxd) Sample Delay
         */
        xSPIx->RX_DELAY = ((uint32_t)(XSPI_StandInitStruct->RxdSamplingEdge | (XSPI_StandInitStruct->RxdSampleDelay & 0x000000FFU)));
    }
    else
    {
         /* No Process */
    }


    /* Configure: Both Instruction and Address will be sent in the mode specified by multi-channel
     * Configure: clock stretching capability
     */
    if (XSPI_StandInitStruct->FrameFormat != XSPI_STANDARD_MODE)
    {
        /* Configure: Enhanced Slave */
        /* Get the XSPI ENH_CTRL0 value */
        tmpregister = xSPIx->ENH_CTRL0;

        /* Clear Control bit */
        tmpregister &= ~(XSPI_ENH_CTRL0_BASE_MASK | XSPI_ENH_CTRL0_DDR_MASK);
        
        tmpregister |= ((uint32_t)(XSPI_StandInitStruct->EnhSlave_ClockStrech | XSPI_V2_ENH_CTRL0_TRANSTYPE));
        
        /* Write to XSPI ENH_CTRL0 */
        xSPIx->ENH_CTRL0 = tmpregister;
    }
    else
    {
         /* No Process */
    }
}

/**
*\*\name    XSPI_StructBaseInit.
*\*\fun     Fills each XSPI_InitType member with its default value.
*\*\param   XSPI_InitType (The input parameters must be the following values):
*\*\          - SCPH
*\*\          - SCPOL
*\*\          - FrameFormat
*\*\          - DataFrameSize
*\*\          - TransferMode
*\*\          - Role
*\*\          - Baudr
*\*\          - RxdSamplingEdge
*\*\          - RxdSampleDelay
*\*\          - NssToggle
*\*\          - EnhSlave_ClockStrech
*\*\return  none
**/
void XSPI_StructBaseInit(XSPI_InitType* XSPI_StandInitStruct)
{
    XSPI_StandInitStruct->SCPH                 = XSPI_SCPH_FIRST_EDGE;
    XSPI_StandInitStruct->SCPOL                = XSPI_SCPOL_LOW_LEVEL;
    XSPI_StandInitStruct->FrameFormat          = XSPI_STANDARD_MODE;
    XSPI_StandInitStruct->DataFrameSize        = XSPI_FRAME_SIZE_8_BIT;
    XSPI_StandInitStruct->TransferMode         = XSPI_TX_AND_RX_MODE;
    XSPI_StandInitStruct->Role                 = XSPI_MASTER_ROLE;
    XSPI_StandInitStruct->Baudr                = 0U;
    XSPI_StandInitStruct->RxdSamplingEdge      = XSPI_RXD_HCLK_RISING_SAMPLING;
    XSPI_StandInitStruct->RxdSampleDelay       = 0U;
    
    XSPI_StandInitStruct->NssToggle            = XSPI_NSS_TOGGLE_DISABLE;
    XSPI_StandInitStruct->EnhSlave_ClockStrech = XSPI_CLOCK_STRETCH_DISABLE;
}


/**
*\*\name    XSPI_InitEnhanced.
*\*\fun     Initilizes the enhanced SPI mode.
*\*\param   SCPH: 
*\*\          - XSPI_SCPH_FIRST_EDGE
*\*\          - XSPI_SCPH_SECOND_EDGE
*\*\param   SCPOL: 
*\*\          - XSPI_SCPOL_LOW_LEVEL
*\*\          - XSPI_SCPOL_HIGH_LEVEL
*\*\param   FrameFormat: 
*\*\          - XSPI_DUAL_LINE_MODE  
*\*\          - XSPI_QUAD_LINE_MODE 
*\*\          - XSPI_OCTAL_LINE_MODE 
*\*\param   DataFrameSize: 
*\*\          - XSPI_FRAME_SIZE_4_BIT
*\*\          - XSPI_FRAME_SIZE_5_BIT
*\*\          - XSPI_FRAME_SIZE_6_BIT
*\*\          - XSPI_FRAME_SIZE_7_BIT
*\*\          - XSPI_FRAME_SIZE_8_BIT
*\*\          - XSPI_FRAME_SIZE_9_BIT
*\*\          - XSPI_FRAME_SIZE_10_BIT
*\*\          - XSPI_FRAME_SIZE_11_BIT
*\*\          - XSPI_FRAME_SIZE_12_BIT
*\*\          - XSPI_FRAME_SIZE_13_BIT
*\*\          - XSPI_FRAME_SIZE_14_BIT
*\*\          - XSPI_FRAME_SIZE_15_BIT
*\*\          - XSPI_FRAME_SIZE_16_BIT
*\*\          - XSPI_FRAME_SIZE_17_BIT
*\*\          - XSPI_FRAME_SIZE_18_BIT
*\*\          - XSPI_FRAME_SIZE_19_BIT
*\*\          - XSPI_FRAME_SIZE_20_BIT
*\*\          - XSPI_FRAME_SIZE_21_BIT
*\*\          - XSPI_FRAME_SIZE_22_BIT
*\*\          - XSPI_FRAME_SIZE_23_BIT
*\*\          - XSPI_FRAME_SIZE_24_BIT
*\*\          - XSPI_FRAME_SIZE_25_BIT
*\*\          - XSPI_FRAME_SIZE_26_BIT
*\*\          - XSPI_FRAME_SIZE_27_BIT
*\*\          - XSPI_FRAME_SIZE_28_BIT
*\*\          - XSPI_FRAME_SIZE_29_BIT
*\*\          - XSPI_FRAME_SIZE_30_BIT
*\*\          - XSPI_FRAME_SIZE_31_BIT
*\*\          - XSPI_FRAME_SIZE_32_BIT
*\*\param   TransferMode: 
*\*\          - XSPI_TX_ONLY_MODE
*\*\          - XSPI_RX_ONLY_MODE
*\*\param   Baudr: 
*\*\          - Any even number between 2 and 65534
*\*\param   RxdSamplingEdge:   
*\*\          - XSPI_RXD_HCLK_RISING_SAMPLING
*\*\          - XSPI_RXD_HCLK_FALLING_SAMPLING
*\*\          Note: no need to worry about it in DDR mode
*\*\param   RxdSampleDelay: 
*\*\          - Valid values 0 ~ 255, measured in HCLK
*\*\          Note: no need to worry about it in DDR mode
*\*\param   TransferType: 
*\*\          - XSPI_INST_ADDR_SINGLE_LINE
*\*\          - XSPI_INST_SINGLE_LINE_ADDR_MULTI_LINE
*\*\          - XSPI_INST_ADDR_MULTI_LINE
*\*\param   AddrLen: 
*\*\          - XSPI_NO_ADDR_PHASE
*\*\          - XSPI_ADDR_LEN_4BIT
*\*\          - XSPI_ADDR_LEN_8BIT
*\*\          - XSPI_ADDR_LEN_12BIT
*\*\          - XSPI_ADDR_LEN_16BIT
*\*\          - XSPI_ADDR_LEN_20BIT
*\*\          - XSPI_ADDR_LEN_24BIT
*\*\          - XSPI_ADDR_LEN_28BIT
*\*\          - XSPI_ADDR_LEN_32BIT
*\*\          - XSPI_ADDR_LEN_36BIT
*\*\          - XSPI_ADDR_LEN_40BIT
*\*\          - XSPI_ADDR_LEN_44BIT
*\*\          - XSPI_ADDR_LEN_48BIT
*\*\          - XSPI_ADDR_LEN_52BIT
*\*\          - XSPI_ADDR_LEN_56BIT
*\*\          - XSPI_ADDR_LEN_60BIT
*\*\param   InstructLen: 
*\*\          - XSPI_NO_INST_PHASE
*\*\          - XSPI_INST_LEN_4BIT
*\*\          - XSPI_INST_LEN_8BIT
*\*\          - XSPI_INST_LEN_16BIT
*\*\param   WaitCycles: 
*\*\          - XSPI_WAIT_0_CYCLES
*\*\          - XSPI_WAIT_1_CYCLES
*\*\          - XSPI_WAIT_2_CYCLES
*\*\          - XSPI_WAIT_3_CYCLES
*\*\          - XSPI_WAIT_4_CYCLES
*\*\          - XSPI_WAIT_5_CYCLES
*\*\          - XSPI_WAIT_6_CYCLES
*\*\          - XSPI_WAIT_7_CYCLES
*\*\          - XSPI_WAIT_8_CYCLES
*\*\          - XSPI_WAIT_9_CYCLES
*\*\          - XSPI_WAIT_10_CYCLES
*\*\          - XSPI_WAIT_11_CYCLES
*\*\          - XSPI_WAIT_12_CYCLES
*\*\          - XSPI_WAIT_13_CYCLES
*\*\          - XSPI_WAIT_14_CYCLES
*\*\          - XSPI_WAIT_15_CYCLES
*\*\          - XSPI_WAIT_16_CYCLES
*\*\          - XSPI_WAIT_17_CYCLES
*\*\          - XSPI_WAIT_18_CYCLES
*\*\          - XSPI_WAIT_19_CYCLES
*\*\          - XSPI_WAIT_20_CYCLES
*\*\          - XSPI_WAIT_21_CYCLES
*\*\          - XSPI_WAIT_22_CYCLES
*\*\          - XSPI_WAIT_23_CYCLES
*\*\          - XSPI_WAIT_24_CYCLES
*\*\          - XSPI_WAIT_25_CYCLES
*\*\          - XSPI_WAIT_26_CYCLES
*\*\          - XSPI_WAIT_27_CYCLES
*\*\          - XSPI_WAIT_28_CYCLES
*\*\          - XSPI_WAIT_29_CYCLES
*\*\          - XSPI_WAIT_30_CYCLES
*\*\          - XSPI_WAIT_31_CYCLES
*\*\          - XSPI_WAIT_32_CYCLES
*\*\param   ClockStrech: 
*\*\          - XSPI_CLOCK_STRETCH_DISABLE
*\*\          - XSPI_CLOCK_STRETCH_ENABLE
*\*\param   DDREable: 
*\*\          - XSPI_DDR_DISABLE
*\*\          - XSPI_DDR_ENABLE
*\*\param   DDR_InitType: 
*\*\          Note: The following parameters only need to be configured in DDR mode
*\*\param     InstrucDDR: 
*\*\            - XSPI_INST_DDR_DISABLE
*\*\            - XSPI_INST_DDR_ENABLE
*\*\param     RxdsSignal: 
*\*\            - XSPI_RXDS_SIGNAL_DISABLE
*\*\            - XSPI_RXDS_SIGNAL_ENABLE
*\*\param     TxdDataMask: 
*\*\            - XSPI_DATA_MASK_DISABLE
*\*\            - XSPI_DATA_MASK_ENABLE
*\*\param     TxdDriverEdge: 
*\*\            - 0 ~ 255, Max value = (BAUD/2) - 1, BAUDR = CLKDIV[14:0] * 2)
*\*\return  none
**/
void XSPI_InitEnhanced(XSPI_Module *xSPIx, XSPI_EnhancedInitType* XSPI_EnhancedInitStruct)
{
    uint32_t tmpregister = 0;

    /*---------------------------- XSPI CTRL0 Configuration ------------------------*/
    /* Get the XSPI CTRL0 value */
    tmpregister = xSPIx->CTRL0;

    /* Clear Control bit */
    tmpregister &= ~XSPI_CTRL0_MASK;

    /* Configure: Data Frame Size 
     * Configure: Serial Clock Phase
     * Configure: Serial Clock Polarity
     * Configure: Transfer Mode
     * Configure: Master mode 
     */
    tmpregister |= ((uint32_t)(XSPI_EnhancedInitStruct->DataFrameSize| XSPI_EnhancedInitStruct->SCPH | XSPI_EnhancedInitStruct->SCPOL 
                               | XSPI_EnhancedInitStruct->TransferMode | XSPI_EnhancedInitStruct->FrameFormat | XSPI_V2_CTRL0_MST));
                     
    /* Write to XSPI CTRL0 */
    xSPIx->CTRL0 = tmpregister;

    /* Set the XSPI BAUD value */
    xSPIx->BAUD = ((uint32_t)((XSPI_EnhancedInitStruct->Baudr / 2U) << 1U));

    if (XSPI_EnhancedInitStruct->TransferMode == XSPI_RX_ONLY_MODE && XSPI_EnhancedInitStruct->DDREable != XSPI_DDR_ENABLE)
    {
        /* Configure: Receive Data (rxd) Sampling Edge
         * Configure: Receive Data (rxd) Sample Delay
         */
        xSPIx->RX_DELAY = ((uint32_t)(XSPI_EnhancedInitStruct->RxdSamplingEdge | (XSPI_EnhancedInitStruct->RxdSampleDelay & 0x000000FFU)));
    }
    else
    {
        /* Clear XSPI RX_DELAY */
        xSPIx->RX_DELAY = 0x00000000U;
    }

                     
    /*---------------------------- XSPI ENH_CTRL0 Configuration ------------------------*/
    /* Get the XSPI ENH_CTRL0 value */
    tmpregister = xSPIx->ENH_CTRL0;

    /* Clear Control bit */
    tmpregister &= ~(XSPI_ENH_CTRL0_BASE_MASK | XSPI_ENH_CTRL0_DDR_MASK);

    /* Configure: Dual/Quad/Octal mode address and instruction transfer format
     * Configure: Dual/Quad/Octal mode address length
     * Configure: Dual/Quad/Octal mode instruction length
     * Configure: Dual/Quad/Octal mode wait cycles between control frames transmit and data reception
     * Configure: Dual/Quad/Octal mode clock stretching capability enable 
     */
    tmpregister |= ((uint32_t)((XSPI_EnhancedInitStruct->WaitCycles & 0x0000001FU) << 11U));
    tmpregister |= ((uint32_t)(XSPI_EnhancedInitStruct->TransferType | XSPI_EnhancedInitStruct->InstructLen 
                               | XSPI_EnhancedInitStruct->AddrLen | XSPI_EnhancedInitStruct->ClockStrech));

    /* Configure DDR Mode */
    if (XSPI_EnhancedInitStruct->DDREable)
    {
        /* Configure: Dual/Quad/Octal mode DDR Enable (address DDR and data DDR)
         * Configure: Dual/Quad/Octal mode instruction DDR Enable
         * Configure: Dual/Quad/Octal mode read data strobe enable (DDR mode)
         * Configure: Dual/Quad/Octal mode txd_line data mask enable (DDR mode)
         */

        tmpregister |= ((uint32_t)(XSPI_EnhancedInitStruct->DDREable | XSPI_EnhancedInitStruct->DDR_InitType.InstrucDDR 
                                   | XSPI_EnhancedInitStruct->DDR_InitType.RxdsSignal | XSPI_EnhancedInitStruct->DDR_InitType.TxdDataMask));


        /* Configure: DDR mode TXD Drive edge register which decided the driving edge of transmit data
         * Note: maximum value is = (BAUDR/2) -1 (BAUDR = CLKDIV [14:0] * 2)
         */
        xSPIx->DDR_TXDE = XSPI_EnhancedInitStruct->DDR_InitType.TxdDriverEdge;
    }
    else
    {
        /* Clear DDR_TXDE and RXDS_DELAY_CTRL register */
        xSPIx->DDR_TXDE = 0x00000000U;
    }

    /* Write to XSPI ENH_CTRL0 */
    xSPIx->ENH_CTRL0 = tmpregister;
    
}

/**
*\*\name    XSPI_InitEnhancedStruct.
*\*\fun     Fills each XSPI_EnhancedInitType member with its default value.
*\*\param   XSPI_EnhancedInitType (The input parameters must be the following values):
*\*\          - SCPH
*\*\          - SCPOL
*\*\          - FrameFormat
*\*\          - DataFrameSize
*\*\          - TransferMode
*\*\          - Baudr
*\*\          - RxdSamplingEdge
*\*\          - RxdSampleDelay
*\*\          - TransferType
*\*\          - AddrLen
*\*\          - InstructLen
*\*\          - WaitCycles
*\*\          - ClockStrech
*\*\          - DDREable
*\*\          - InstrucDDR
*\*\          - RxdsSignal
*\*\          - TxdDataMask
*\*\          - TxdDriverEdge
*\*\return  none
**/
void XSPI_InitEnhancedStruct(XSPI_EnhancedInitType* XSPI_EnhancedInitStruct)
{
    XSPI_EnhancedInitStruct->SCPH            = XSPI_SCPH_FIRST_EDGE;
    XSPI_EnhancedInitStruct->SCPOL           = XSPI_SCPOL_LOW_LEVEL;
    XSPI_EnhancedInitStruct->FrameFormat     = XSPI_OCTAL_LINE_MODE;
    XSPI_EnhancedInitStruct->DataFrameSize   = XSPI_FRAME_SIZE_8_BIT;
    XSPI_EnhancedInitStruct->TransferMode    = XSPI_RX_ONLY_MODE;
    XSPI_EnhancedInitStruct->Baudr           = 0U;
    XSPI_EnhancedInitStruct->RxdSamplingEdge = XSPI_RXD_HCLK_RISING_SAMPLING;
    XSPI_EnhancedInitStruct->RxdSampleDelay  = 0U;


    XSPI_EnhancedInitStruct->TransferType    = XSPI_INST_ADDR_SINGLE_LINE;
    XSPI_EnhancedInitStruct->AddrLen         = XSPI_NO_ADDR_PHASE;
    XSPI_EnhancedInitStruct->InstructLen     = XSPI_NO_INST_PHASE;
    XSPI_EnhancedInitStruct->WaitCycles      = XSPI_WAIT_0_CYCLES;
    XSPI_EnhancedInitStruct->ClockStrech     = XSPI_CLOCK_STRETCH_DISABLE;
    XSPI_EnhancedInitStruct->DDREable        = XSPI_DDR_DISABLE;

    XSPI_EnhancedInitStruct->DDR_InitType.InstrucDDR    = XSPI_INST_DDR_DISABLE;
    XSPI_EnhancedInitStruct->DDR_InitType.RxdsSignal    = XSPI_RXDS_SIGNAL_DISABLE;
    XSPI_EnhancedInitStruct->DDR_InitType.TxdDataMask   = XSPI_DATA_MASK_DISABLE;
    XSPI_EnhancedInitStruct->DDR_InitType.TxdDriverEdge = 0x00000000U;
    
}


/**
*\*\name    XSPI_InitEnhanced.
*\*\fun     Initilizes the XIP mode.
*\*\param   SCPH: 
*\*\          - XSPI_SCPH_FIRST_EDGE
*\*\          - XSPI_SCPH_SECOND_EDGE
*\*\param   SCPOL: 
*\*\          - XSPI_SCPOL_LOW_LEVEL
*\*\          - XSPI_SCPOL_HIGH_LEVEL
*\*\param   DataFrameSize: 
*\*\          - XSPI_FRAME_SIZE_4_BIT
*\*\          - XSPI_FRAME_SIZE_5_BIT
*\*\          - XSPI_FRAME_SIZE_6_BIT
*\*\          - XSPI_FRAME_SIZE_7_BIT
*\*\          - XSPI_FRAME_SIZE_8_BIT
*\*\          - XSPI_FRAME_SIZE_9_BIT
*\*\          - XSPI_FRAME_SIZE_10_BIT
*\*\          - XSPI_FRAME_SIZE_11_BIT
*\*\          - XSPI_FRAME_SIZE_12_BIT
*\*\          - XSPI_FRAME_SIZE_13_BIT
*\*\          - XSPI_FRAME_SIZE_14_BIT
*\*\          - XSPI_FRAME_SIZE_15_BIT
*\*\          - XSPI_FRAME_SIZE_16_BIT
*\*\          - XSPI_FRAME_SIZE_17_BIT
*\*\          - XSPI_FRAME_SIZE_18_BIT
*\*\          - XSPI_FRAME_SIZE_19_BIT
*\*\          - XSPI_FRAME_SIZE_20_BIT
*\*\          - XSPI_FRAME_SIZE_21_BIT
*\*\          - XSPI_FRAME_SIZE_22_BIT
*\*\          - XSPI_FRAME_SIZE_23_BIT
*\*\          - XSPI_FRAME_SIZE_24_BIT
*\*\          - XSPI_FRAME_SIZE_25_BIT
*\*\          - XSPI_FRAME_SIZE_26_BIT
*\*\          - XSPI_FRAME_SIZE_27_BIT
*\*\          - XSPI_FRAME_SIZE_28_BIT
*\*\          - XSPI_FRAME_SIZE_29_BIT
*\*\          - XSPI_FRAME_SIZE_30_BIT
*\*\          - XSPI_FRAME_SIZE_31_BIT
*\*\          - XSPI_FRAME_SIZE_32_BIT
*\*\param   TransferMode: 
*\*\          - XSPI_TX_ONLY_MODE
*\*\          - XSPI_RX_ONLY_MODE
*\*\param   Baudr: 
*\*\          - Any even number between 2 and 65534
*\*\param   XipFrameFormat: 
*\*\          - XSPI_DUAL_LINE_MODE  
*\*\          - XSPI_QUAD_LINE_MODE  
*\*\          - XSPI_OCTAL_LINE_MODE 
*\*\param   XipTransferType: 
*\*\          - XSPI_INST_ADDR_SINGLE_LINE
*\*\          - XSPI_INST_SINGLE_LINE_ADDR_MULTI_LINE
*\*\          - XSPI_INST_ADDR_MULTI_LINE
*\*\param   XipAddrLen: 
*\*\         For XIP Read Mode:
*\*\          - XSPI_NO_ADDR_PHASE
*\*\          - XSPI_ADDR_LEN_4BIT
*\*\          - XSPI_ADDR_LEN_8BIT
*\*\          - XSPI_ADDR_LEN_12BIT
*\*\          - XSPI_ADDR_LEN_16BIT
*\*\          - XSPI_ADDR_LEN_20BIT
*\*\          - XSPI_ADDR_LEN_24BIT
*\*\          - XSPI_ADDR_LEN_28BIT
*\*\          - XSPI_ADDR_LEN_32BIT
*\*\          - XSPI_ADDR_LEN_36BIT
*\*\          - XSPI_ADDR_LEN_40BIT
*\*\          - XSPI_ADDR_LEN_44BIT
*\*\          - XSPI_ADDR_LEN_48BIT
*\*\          - XSPI_ADDR_LEN_52BIT
*\*\          - XSPI_ADDR_LEN_56BIT
*\*\          - XSPI_ADDR_LEN_60BIT
*\*\         
*\*\         For XIP Read Mode:
*\*\          - XSPI_NO_ADDR_PHASE
*\*\          - XSPI_ADDR_LEN_4BIT
*\*\          - XSPI_ADDR_LEN_8BIT
*\*\          - XSPI_ADDR_LEN_12BIT
*\*\          - XSPI_ADDR_LEN_16BIT
*\*\          - XSPI_ADDR_LEN_20BIT
*\*\          - XSPI_ADDR_LEN_24BIT
*\*\          - XSPI_ADDR_LEN_28BIT
*\*\          - XSPI_ADDR_LEN_32BIT
*\*\param   XipWaitCycles: 
*\*\          - XSPI_WAIT_0_CYCLES
*\*\          - XSPI_WAIT_1_CYCLES
*\*\          - XSPI_WAIT_2_CYCLES
*\*\          - XSPI_WAIT_3_CYCLES
*\*\          - XSPI_WAIT_4_CYCLES
*\*\          - XSPI_WAIT_5_CYCLES
*\*\          - XSPI_WAIT_6_CYCLES
*\*\          - XSPI_WAIT_7_CYCLES
*\*\          - XSPI_WAIT_8_CYCLES
*\*\          - XSPI_WAIT_9_CYCLES
*\*\          - XSPI_WAIT_10_CYCLES
*\*\          - XSPI_WAIT_11_CYCLES
*\*\          - XSPI_WAIT_12_CYCLES
*\*\          - XSPI_WAIT_13_CYCLES
*\*\          - XSPI_WAIT_14_CYCLES
*\*\          - XSPI_WAIT_15_CYCLES
*\*\          - XSPI_WAIT_16_CYCLES
*\*\          - XSPI_WAIT_17_CYCLES
*\*\          - XSPI_WAIT_18_CYCLES
*\*\          - XSPI_WAIT_19_CYCLES
*\*\          - XSPI_WAIT_20_CYCLES
*\*\          - XSPI_WAIT_21_CYCLES
*\*\          - XSPI_WAIT_22_CYCLES
*\*\          - XSPI_WAIT_23_CYCLES
*\*\          - XSPI_WAIT_24_CYCLES
*\*\          - XSPI_WAIT_25_CYCLES
*\*\          - XSPI_WAIT_26_CYCLES
*\*\          - XSPI_WAIT_27_CYCLES
*\*\          - XSPI_WAIT_28_CYCLES
*\*\          - XSPI_WAIT_29_CYCLES
*\*\          - XSPI_WAIT_30_CYCLES
*\*\          - XSPI_WAIT_31_CYCLES
*\*\          - XSPI_WAIT_32_CYCLES
*\*\param   XipDDREable: 
*\*\          - XSPI_DDR_DISABLE
*\*\          - XSPI_DDR_ENABLE
*\*\
*\*\
*\*\param   Info:   
*\*\
*\*\param     Read_XIPInitType:  
*\*\             Note: XIP Read Parameter, Configuration is only needed when reading from XIP
*\*\param        XipInstructLen: 
*\*\               - XSPI_NO_INST_PHASE
*\*\               - XSPI_INST_LEN_4BIT
*\*\               - XSPI_INST_LEN_8BIT
*\*\               - XSPI_INST_LEN_16BIT
*\*\param        XipInstrctEnable: 
*\*\               - XSPI_XIP_INSTRUCT_DISABLE
*\*\               - XSPI_XIP_INSTRUCT_ENABLE
*\*\param        XipDFSHC: 
*\*\               - XSPI_XIP_DFSHC_DISABLE
*\*\               - XSPI_XIP_DFSHC_ENABLE
*\*\param        XipPrefetch: 
*\*\               - XSPI_XIP_PREFETCH_DISABLE
*\*\               - XSPI_XIP_PREFETCH_ENABLE
*\*\param        XipModeBit: 
*\*\               - XSPI_XIP_MODE_BIT_DISABLE
*\*\               - XSPI_XIP_MODE_BIT_ENABLE
*\*\param        XipModeBitLen: 
*\*\               - XSPI_XIP_MODE_BIT_LEN_2_BIT
*\*\               - XSPI_XIP_MODE_BIT_LEN_4_BIT
*\*\               - XSPI_XIP_MODE_BIT_LEN_8_BIT
*\*\               - XSPI_XIP_MODE_BIT_LEN_16_BIT
*\*\
*\*\param        ContinueTransfer: 
*\*\             Note: XIP Read Parameter 
*\*\param          XipContinueTransfer: 
*\*\                 - XSPI_XIP_CONTINUE_TRANSFER_DISABLE
*\*\                 - XSPI_XIP_CONTINUE_TRANSFER_ENABLE
*\*\param          XipWatchDogTimeout: 
*\*\                 - 0 ~ 255, in terms of HCLK
*\*\
*\*\param        DDR_Info: 
*\*\param          DDR_InitType: 
*\*\               Note: Configuration is only required for XIP DDR reads
*\*\param            TxdDriverEdge: 
*\*\                   - 0 ~ 255, Max value = (BAUD/2) - 1, BAUDR = CLKDIV[14:0] * 2)
*\*\param            XipInstrucDDR: 
*\*\                   - XSPI_INST_DDR_DISABLE
*\*\                   - XSPI_INST_DDR_ENABLE
*\*\param            XipRxdsSignal: 
*\*\                   - XSPI_RXDS_SIGNAL_DISABLE
*\*\                   - XSPI_RXDS_SIGNAL_ENABLE
*\*\
*\*\param          Rxds_InitType: 
*\*\               Note: Configuration is only required for XIP Non-DDR reads
*\*\param            RxdSamplingEdge: 
*\*\                   - XSPI_RXD_HCLK_RISING_SAMPLING
*\*\                   - XSPI_RXD_HCLK_FALLING_SAMPLING
*\*\param            RxdSampleDelay: 
*\*\                   - Valid values 0 ~ 255, measured in HCLK
*\*\
*\*\
*\*\param     Write_XIPInitType:  
*\*\             Note: XIP Write Parameter, Configuration is only needed when writing from XIP
*\*\param        XipInstructLen: 
*\*\               - XSPI_NO_INST_PHASE
*\*\               - XSPI_INST_LEN_4BIT
*\*\               - XSPI_INST_LEN_8BIT
*\*\               - XSPI_INST_LEN_16BIT
*\*\
*\*\param        DDR_InitType: 
*\*\             Note: Configuration is only required for XIP DDR writes
*\*\param          TxdDriverEdge: 
*\*\                 - 0 ~ 255, Max value = (BAUD/2) - 1, BAUDR = CLKDIV[14:0] * 2)
*\*\param          XipInstrucDDR: 
*\*\                 - XSPI_INST_DDR_DISABLE
*\*\                 - XSPI_RXDS_SIGNAL_ENABLE
*\*\
*\*\return  none
**/
void XSPI_InitXIP(XSPI_Module *xSPIx, XSPI_XIPInitType* XSPI_XIPInitStruct)
{
    uint32_t tmpregister = 0;

    /*---------------------------- XSPI CTRL0 Configuration ------------------------*/
    /* Get the XSPI CTRL0 value */
    tmpregister = xSPIx->CTRL0;

    /* Clear Control bit */
    tmpregister &= ~XSPI_CTRL0_MASK;

    /* Configure: Data Frame Size 
     * Configure: Serial Clock Phase
     * Configure: Serial Clock Polarity
     * Configure: Transfer Mode
     * Configure: Master mode 
     */
    tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->DataFrameSize| XSPI_XIPInitStruct->SCPH | 
                               XSPI_XIPInitStruct->SCPOL | XSPI_XIPInitStruct->TransferMode | XSPI_V2_CTRL0_MST));
                     
    /* Write to XSPI CTRL0 */
    xSPIx->CTRL0 = tmpregister;

    /* Set the XSPI BAUD value */
    xSPIx->BAUD = ((uint32_t)((XSPI_XIPInitStruct->Baudr / 2U) << 1U));

    /* Get the XSPI ENH_CTRL0 value */
    tmpregister = xSPIx->ENH_CTRL0;

    /* Clear clock stretching capability bit */
    tmpregister &= ~XSPI_V2_ENH_CTRL0_CLKSTREN;
                        
    /* Write to XSPI CTRL0 */
    xSPIx->ENH_CTRL0 = tmpregister;

    /***************** XIP Read Mode *****************/
    if (XSPI_XIPInitStruct->TransferMode == XSPI_RX_ONLY_MODE)
    {
        /*---------------------------- XSPI XIP_CTRL Configuration ------------------------*/
        /* Get the XSPI XIP_CTRL value */
        tmpregister = xSPIx->XIP_CTRL;

        /* Clear XIP_CTRL Register */
        tmpregister &= ~(XSPI_XIP_CTRL_BASE_MASK | XSPI_XIP_CTRL_DDR_MASK);

        /* Configure: SPI Frame Format */
        tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XipFrameFormat >> 22U));

        /* Configure: Address and instruction transfer format 
        * Configure: Address Length
        */
        tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipTransferType | XSPI_XIPInitStruct->XipAddrLen) << 2U));

        /* Configure: Wait cycles between control frames transmit and data reception */
        tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipWaitCycles & 0x0000001FU) << 13U));

        /* Configure: XIP transfer instruction phase */
        if (XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipInstrctEnable)
        {
            /* Configure: instruction length */
            tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipInstructLen << 1U));

            /* Configure: XIP instruction enable */
            tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipInstrctEnable));
        }
        else
        {
            /* No Process */
        }
        
        /* Configure: Enables XIP pre-fetch functionality 
         * Configure: Fix DFS for XIP transfers
         */
        tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipPrefetch | XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipDFSHC));
        
        /* Configure: Mode bits enable in XIP mode
         * Configure: XIP Mode bits length
         */
        if (XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipModeBit == XSPI_XIP_MODE_BIT_ENABLE)
        {
            tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipModeBit | XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipModeBitLen));
        }
        else
        {
            /* No Process */
        }

        /* Configure: Enable continuous transfer in XIP mode */
        if (XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.ContinueTransfer.XipContinueTransfer)
        {
            tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.ContinueTransfer.XipContinueTransfer));

            /* Configure: XIP time out value in terms of hclk */
            xSPIx->XIP_TOUT = XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.ContinueTransfer.XipWatchDogTimeout;
        }
        else
        {
            /* Clear XIP_TOUT register */
            xSPIx->XIP_TOUT = 0x00000000U;
        }

        /* Configure DDR Mode */
        if (XSPI_XIPInitStruct->XipDDREable)
        {
            /* Configure: XIP mode DDR Enable (address DDR and data DDR)
             * Configure: XIP mode instruction DDR Enable
             * Configure: XIP mode read data strobe enable (DDR mode)
             */

            tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipDDREable | 
                                        XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.XipInstrucDDR | 
                                        XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.XipRxdsSignal) << 3U));
            
            /* Configure: DDR mode TXD Drive edge register which decided the driving edge of transmit data
             * Note: maximum value is = (BAUDR/2) -1 (BAUDR = CLKDIV [14:0] * 2)
             */
            xSPIx->DDR_TXDE = XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.TxdDriverEdge;
        }
        else
        {
            /* Write XSPI RX_DELAY */
            xSPIx->RX_DELAY = ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.Rxds_InitType.RxdSamplingEdge | 
                                          (XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.Rxds_InitType.RxdSampleDelay & 0x000000FFU)));
        }

        /* Write back XIP_CTRL */
        xSPIx->XIP_CTRL = tmpregister;
    }
    /***************** XIP Write Mode *****************/
    else if (XSPI_XIPInitStruct->TransferMode == XSPI_TX_ONLY_MODE)
    {
        /*---------------------------- XSPI XIP_WRITE_CTRL Configuration ------------------------*/
        /* Get the XSPI XIP_WRITE_CTRL value */
        tmpregister = xSPIx->XIP_WRITE_CTRL;

        /* Clear XIP_WRITE_CTRL Register */
        tmpregister &= ~XSPI_XIP_WRITE_CTRL_MASK;

        /* Configure: SPI Frame Format */
        tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XipFrameFormat >> 22U));

        /* Configure: Address and instruction transfer format 
        * Configure: Address Length
        */
        tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipTransferType | XSPI_XIPInitStruct->XipAddrLen) << 2U));

        /* Configure: Wait cycles between control frames transmit and data reception */
        tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipWaitCycles & 0x0000001FU) << 16U));

        /* Configure: instruction length */
        tmpregister |= ((uint32_t)(XSPI_XIPInitStruct->XIP_Info.Write_XIPInitType.XipInstructLen));

        /* Configure DDR Mode */
        if (XSPI_XIPInitStruct->XipDDREable)
        {
            /* Configure: XIP mode DDR Enable (address DDR and data DDR)
             * Configure: XIP mode instruction DDR Enable
             */
            tmpregister |= ((uint32_t)((XSPI_XIPInitStruct->XipDDREable >> 16U) << 10U));

            tmpregister |= ((uint32_t)(((XSPI_XIPInitStruct->XipDDREable | XSPI_XIPInitStruct->XIP_Info.Write_XIPInitType.DDR_InitType.XipInstrucDDR) >> 16U) << 10U));

            /* Configure: DDR mode TXD Drive edge register which decided the driving edge of transmit data
             * Note: maximum value is = (BAUDR/2) -1 (BAUDR = CLKDIV [14:0] * 2)
             */
            xSPIx->DDR_TXDE = XSPI_XIPInitStruct->XIP_Info.Write_XIPInitType.DDR_InitType.TxdDriverEdge;
            
        }
        else
        {
            /* Clear DDR_TXDE register */
            xSPIx->DDR_TXDE = 0x00000000U;
        }

        /* Write back XIP_WRITE_CTRL */
        xSPIx->XIP_WRITE_CTRL = tmpregister;
        
        xSPIx->XIP_WRITE_CTRL = 0x030B;
 
    }
}

/**
*\*\name    XSPI_InitXIPStruct.
*\*\fun     Fills each XSPI_XIPInitType member with its default value.
*\*\param   XSPI_XIPInitType (The input parameters must be the following values):
*\*\          - SCPH
*\*\          - SCPOL
*\*\          - DataFrameSize
*\*\          - TransferMode
*\*\          - Baudr
*\*\          - XipFrameFormat
*\*\          - XipTransferType
*\*\          - XipAddrLen
*\*\          - XipWaitCycles
*\*\          - XipDDREable
*\*\          - XIP_Info
*\*\            XIP Read Parameter:
*\*\            - XipInstructLen
*\*\            - XipInstrctEnable
*\*\            - Info
*\*\              DDR Parameter:
*\*\              - TxdDriverEdge
*\*\              - XipInstrucDDR
*\*\              - XipRxdsSignal
*\*\            - XipDFSHC
*\*\            - XipPrefetch
*\*\            - XipModeBit
*\*\            - XipModeBitLen
*\*\            - ContinueTransfer
*\*\              Continue Transfer Parameter:
*\*\              - XipContinueTransfer
*\*\              - XipWatchDogTimeout
*\*\return  none
**/
void XSPI_InitXIPStruct(XSPI_XIPInitType* XSPI_XIPInitStruct)
{
    XSPI_XIPInitStruct->SCPH              = XSPI_SCPH_FIRST_EDGE;
    XSPI_XIPInitStruct->SCPOL             = XSPI_SCPOL_LOW_LEVEL;
    XSPI_XIPInitStruct->DataFrameSize     = XSPI_FRAME_SIZE_8_BIT;
    XSPI_XIPInitStruct->TransferMode      = XSPI_RX_ONLY_MODE;
    XSPI_XIPInitStruct->Baudr             = 0U;
    XSPI_XIPInitStruct->XipFrameFormat    = XSPI_OCTAL_LINE_MODE;
    XSPI_XIPInitStruct->XipTransferType   = XSPI_INST_ADDR_SINGLE_LINE;
    XSPI_XIPInitStruct->XipAddrLen        = XSPI_NO_ADDR_PHASE;
    XSPI_XIPInitStruct->XipWaitCycles     = XSPI_WAIT_0_CYCLES;
    XSPI_XIPInitStruct->XipDDREable       = XSPI_DDR_DISABLE;

    /* XIP Read Parameter */
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipInstructLen   = XSPI_NO_INST_PHASE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipInstrctEnable = XSPI_XIP_INSTRUCT_DISABLE;

    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.TxdDriverEdge = 0U;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.XipInstrucDDR = XSPI_INST_DDR_DISABLE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.Info.DDR_InitType.XipRxdsSignal = XSPI_RXDS_SIGNAL_DISABLE;

    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipDFSHC      = XSPI_XIP_DFSHC_DISABLE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipPrefetch   = XSPI_XIP_PREFETCH_DISABLE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipModeBit    = XSPI_XIP_MODE_BIT_DISABLE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.XipModeBitLen = XSPI_XIP_MODE_BIT_LEN_2_BIT;

    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.ContinueTransfer.XipContinueTransfer = XSPI_XIP_CONTINUE_TRANSFER_DISABLE;
    XSPI_XIPInitStruct->XIP_Info.Read_XIPInitType.ContinueTransfer.XipWatchDogTimeout  = 0U;
}


/**
*\*\name    XSPI_InitHyperbus.
*\*\fun     Initilizes the Hyperbus mode.
*\*\param   Baudr: 
*\*\          - Any even number between 2 and 65534
*\*\param   TransferMode: 
*\*\          - XSPI_TX_ONLY_MODE
*\*\          - XSPI_RX_ONLY_MODE
*\*\param   DataFrameSize: 
*\*\          - XSPI_FRAME_SIZE_4_BIT
*\*\          - XSPI_FRAME_SIZE_5_BIT
*\*\          - XSPI_FRAME_SIZE_6_BIT
*\*\          - XSPI_FRAME_SIZE_7_BIT
*\*\          - XSPI_FRAME_SIZE_8_BIT
*\*\          - XSPI_FRAME_SIZE_9_BIT
*\*\          - XSPI_FRAME_SIZE_10_BIT
*\*\          - XSPI_FRAME_SIZE_11_BIT
*\*\          - XSPI_FRAME_SIZE_12_BIT
*\*\          - XSPI_FRAME_SIZE_13_BIT
*\*\          - XSPI_FRAME_SIZE_14_BIT
*\*\          - XSPI_FRAME_SIZE_15_BIT
*\*\          - XSPI_FRAME_SIZE_16_BIT
*\*\          - XSPI_FRAME_SIZE_17_BIT
*\*\          - XSPI_FRAME_SIZE_18_BIT
*\*\          - XSPI_FRAME_SIZE_19_BIT
*\*\          - XSPI_FRAME_SIZE_20_BIT
*\*\          - XSPI_FRAME_SIZE_21_BIT
*\*\          - XSPI_FRAME_SIZE_22_BIT
*\*\          - XSPI_FRAME_SIZE_23_BIT
*\*\          - XSPI_FRAME_SIZE_24_BIT
*\*\          - XSPI_FRAME_SIZE_25_BIT
*\*\          - XSPI_FRAME_SIZE_26_BIT
*\*\          - XSPI_FRAME_SIZE_27_BIT
*\*\          - XSPI_FRAME_SIZE_28_BIT
*\*\          - XSPI_FRAME_SIZE_29_BIT
*\*\          - XSPI_FRAME_SIZE_30_BIT
*\*\          - XSPI_FRAME_SIZE_31_BIT
*\*\          - XSPI_FRAME_SIZE_32_BIT
*\*\param   WaitCycles: 
*\*\          - XSPI_WAIT_0_CYCLES
*\*\          - XSPI_WAIT_1_CYCLES
*\*\          - XSPI_WAIT_2_CYCLES
*\*\          - XSPI_WAIT_3_CYCLES
*\*\          - XSPI_WAIT_4_CYCLES
*\*\          - XSPI_WAIT_5_CYCLES
*\*\          - XSPI_WAIT_6_CYCLES
*\*\          - XSPI_WAIT_7_CYCLES
*\*\          - XSPI_WAIT_8_CYCLES
*\*\          - XSPI_WAIT_9_CYCLES
*\*\          - XSPI_WAIT_10_CYCLES
*\*\          - XSPI_WAIT_11_CYCLES
*\*\          - XSPI_WAIT_12_CYCLES
*\*\          - XSPI_WAIT_13_CYCLES
*\*\          - XSPI_WAIT_14_CYCLES
*\*\          - XSPI_WAIT_15_CYCLES
*\*\          - XSPI_WAIT_16_CYCLES
*\*\          - XSPI_WAIT_17_CYCLES
*\*\          - XSPI_WAIT_18_CYCLES
*\*\          - XSPI_WAIT_19_CYCLES
*\*\          - XSPI_WAIT_20_CYCLES
*\*\          - XSPI_WAIT_21_CYCLES
*\*\          - XSPI_WAIT_22_CYCLES
*\*\          - XSPI_WAIT_23_CYCLES
*\*\          - XSPI_WAIT_24_CYCLES
*\*\          - XSPI_WAIT_25_CYCLES
*\*\          - XSPI_WAIT_26_CYCLES
*\*\          - XSPI_WAIT_27_CYCLES
*\*\          - XSPI_WAIT_28_CYCLES
*\*\          - XSPI_WAIT_29_CYCLES
*\*\          - XSPI_WAIT_30_CYCLES
*\*\          - XSPI_WAIT_31_CYCLES
*\*\          - XSPI_WAIT_32_CYCLES
*\*\param   CAPhaseRxds: 
*\*\          - XSPI_HYPERBUS_CA_RXDS_DISABLE  
*\*\          - XSPI_HYPERBUS_CA_RXDS_ENABLE  
*\*\param   ClockStrech: 
*\*\          - XSPI_CLOCK_STRETCH_DISABLE  
*\*\          - XSPI_CLOCK_STRETCH_ENABLE  
*\*\param   TxdDriverEdge: 
*\*\          - 0 ~ 255, Max value = (BAUD/2) - 1, BAUDR = CLKDIV[14:0] * 2)
*\*\param   XIPHyperbus: 
*\*\          - XSPI_XIP_HYPERBUS_MODE
*\*\          - XSPI_NON_XIP_HYPERBUS_MODE
*\*\
*\*\param   XIP_ReadHyperbusInitType:   
*\*\          Note: XIP Hyperbus Read Parameters
*\*\param       XipFixDFS: 
*\*\              - XSPI_XIP_DFSHC_DISABLE
*\*\              - XSPI_XIP_DFSHC_ENABLE
*\*\param       XipPrefetch: 
*\*\              - XSPI_XIP_PREFETCH_DISABLE
*\*\              - XSPI_XIP_PREFETCH_ENABLE
*\*\
*\*\return  none
**/
void XSPI_InitHyperbus(XSPI_Module *xSPIx, XSPI_HyperbusInitType* XSPI_HyperbusInitStruct)
{
    uint32_t tmpregister = 0;

    /*---------------------------- XSPI BAUD Configuration ------------------------*/
    /* Set the XSPI BAUD value */
    xSPIx->BAUD = ((uint32_t)((XSPI_HyperbusInitStruct->Baudr / 2U) << 1U));

    /* Configure: DDR mode TXD Drive edge register which decided the driving edge of transmit data
     * Note: maximum value is = (BAUDR/2) -1 (BAUDR = CLKDIV [14:0] * 2)
     */
    xSPIx->DDR_TXDE = XSPI_HyperbusInitStruct->TxdDriverEdge;

    /*---------------------------- XSPI CTRL0 Configuration ------------------------*/
    /* Get the XSPI CTRL0 value */
    tmpregister = xSPIx->CTRL0;

    /* Clear CTRL0 Register */
    tmpregister &= ~XSPI_CTRL0_MASK;

    /* Configure: The SPI frame format is fixed as Octal SPI Format
     * Configure: Transfer Mode(TX_ONLY or RX_ONLY)
     * Configure: Data Frame Size
     * Configure: fixed as master
     */
    tmpregister |= ((uint32_t)(XSPI_V2_CTRL0_MST | XSPI_V2_CTRL0_SPIFRF | XSPI_HyperbusInitStruct->TransferMode | XSPI_HyperbusInitStruct->DataFrameSize));

    /* Write to XSPI CTRL0 */
    xSPIx->CTRL0 = tmpregister;

    if (XSPI_HyperbusInitStruct->TransferMode == XSPI_RX_ONLY_MODE && XSPI_HyperbusInitStruct->XIPHyperbus)
    {
        /* Hyperbus Read */
        /* Get the XSPI XIP_CTRL value */
        tmpregister = xSPIx->XIP_CTRL;

        /* Clear XIP_CTRL Register */
        tmpregister &= ~(XSPI_XIP_CTRL_BASE_MASK | XSPI_XIP_CTRL_DDR_MASK);

        /* Configure: The SPI frame format is fixed as Octal SPI Format
         * Configure: The Address and instruction transfer format is fixed as Octal SPI Format sent
         * Configure: The address length is fixed as 48-bit
         * Configure: The instruction length is fixed as no have instruction phase
         * Configure: Wait cycles between control frames transmit and data reception
         * Configure: Fix DFS for XIP transfers
         * Configure：DDR mode is fixed Enable
         * Configure：Read data strobe signal is fixed Enable
         * Configure：Enable rxds signaling during address and command phase of Hypebus transfer
         * Configure: Enables XIP pre-fetch
         */
        tmpregister |= ((XSPI_ADDR_LEN_48BIT >> 2U) << 4U);
        tmpregister |= ((uint32_t)((XSPI_HyperbusInitStruct->WaitCycles & 0x0000001FU) << 13U));
        
        tmpregister |= ((uint32_t)(XSPI_HyperbusInitStruct->XIP_ReadHyperbusInitType.XipPrefetch | 
                                   XSPI_HyperbusInitStruct->XIP_ReadHyperbusInitType.XipFixDFS));
        
        tmpregister |= ((uint32_t)(XSPI_HyperbusInitStruct->CAPhaseRxds | XSPI_V2_XIP_CTRL_RXDSEN |
                                   XSPI_V2_XIP_CTRL_DDREN | XSPI_V2_XIP_CTRL_TRANSTYPE_1 | XSPI_V2_XIP_CTRL_FRF));

        /* Write to XSPI XIP_CTRL */
        xSPIx->XIP_CTRL = tmpregister;
    }
    else if (XSPI_HyperbusInitStruct->TransferMode == XSPI_TX_ONLY_MODE && XSPI_HyperbusInitStruct->XIPHyperbus)
    {
        /* XIP Hyperbus Write */
        /* Get the XSPI XIP_WRITE_CTRL value */
        tmpregister = xSPIx->XIP_WRITE_CTRL;

        /* Clear XIP_WRITE_CTRL Register */
        tmpregister &= ~XSPI_XIP_WRITE_CTRL_MASK;

        /* Configure: The SPI frame format is fixed as Octal SPI Format
         * Configure: The Address and instruction transfer format is fixed as Octal SPI Format sent
         * Configure: The address length is fixed as 32-bit
         * Configure: The instruction length is fixed as no have instruction phase
         * Configure：DDR mode is fixed Enable
         * 
         * Configure：Read data strobe signal is fixed Enable
         * 
         * Configure：Enable rxds signaling during address and command phase of Hypebus transfer
         * Configure: Wait cycles between control frames transmit and data reception
         */
        tmpregister |= ((uint32_t)((XSPI_HyperbusInitStruct->WaitCycles & 0x0000001FU) << 16U));
        tmpregister |= ((uint32_t)((((XSPI_HyperbusInitStruct->CAPhaseRxds >> 25U) << 13U) | XSPI_V2_XIP_WRITE_CTRL_WRSPIDDREN | 
                                    XSPI_V2_XIP_WRITE_CTRL_WRADDRL_3 | XSPI_V2_XIP_WRITE_CTRL_WRTRTYPE_1 | XSPI_V2_XIP_WRITE_CTRL_WRFRF)));

        /* Write to XSPI XIP_WRITE_CTRL */
        xSPIx->XIP_WRITE_CTRL = tmpregister;
    }
    else
    {
        /* No Process */
    }


    if (XSPI_HyperbusInitStruct->XIPHyperbus != XSPI_XIP_HYPERBUS_MODE)
    {
        /* NOE-XIP Hyperbus Write/Read */
        /* Get the XSPI ENH_CTRL0 value */
        tmpregister = xSPIx->ENH_CTRL0;

        /* Clear ENH_CTRL0 Register */
        tmpregister &= ~(XSPI_ENH_CTRL0_BASE_MASK | XSPI_ENH_CTRL0_DDR_MASK);

        /* Configure: The Address and instruction transfer format is fixed as Octal SPI Format sent
         * Configure: The address length is fixed as 48-bit
         * Configure: The instruction length is fixed as no have instruction phase
         * Configure: Wait cycles between control frames transmit and data reception
         * Configure：DDR mode is fixed Enable
         * Configure：Read data strobe signal is fixed Enable
         * 
         * Configure：Data mask is fixed Enable
         * 
         * Configure：Enable rxds signaling during address and command phase of Hypebus transfer
         */
        tmpregister |= ((uint32_t)((XSPI_HyperbusInitStruct->WaitCycles & 0x0000001FU) << 11U));
        tmpregister |= ((uint32_t)(XSPI_HyperbusInitStruct->ClockStrech | XSPI_HyperbusInitStruct->CAPhaseRxds | XSPI_V2_ENH_CTRL0_SPIDMEN | 
                                   XSPI_V2_ENH_CTRL0_SPIRXDSEN | XSPI_V2_ENH_CTRL0_WRSPIDDREN | XSPI_ADDR_LEN_48BIT | XSPI_V2_ENH_CTRL0_TRANSTYPE_1));
        
        /* Write to XSPI ENH_CTRL0 */
        xSPIx->ENH_CTRL0 = tmpregister;
    }
    else
    {
        /* Configure：Data mask is fixed Enable
         * Configure: Config clock stretching capability  
         */
        tmpregister = xSPIx->ENH_CTRL0;
        tmpregister &= ~XSPI_V2_ENH_CTRL0_CLKSTREN;
        tmpregister |= ((uint32_t)(XSPI_HyperbusInitStruct->ClockStrech | XSPI_V2_ENH_CTRL0_SPIDMEN));
        xSPIx->ENH_CTRL0 = tmpregister;
    }
}

/**
*\*\name    XSPI_InitHyperbusStruct.
*\*\fun     Fills each XSPI_HyperbusInitType member with its default value.
*\*\param   XSPI_HyperbusInitType (The input parameters must be the following values):
*\*\          - Baudr
*\*\          - TransferMode
*\*\          - DataFrameSize
*\*\          - WaitCycles
*\*\          - CAPhaseRxds
*\*\          - ClockStrech
*\*\          - TxdDriverEdge
*\*\          - XIPHyperbus
*\*\
*\*\          - XIP_ReadHyperbusInitType
*\*\            XIP Hyperbus Read Parameter:
*\*\            - XipFixDFS
*\*\            - XipPrefetch
*\*\return  none
**/
void XSPI_InitHyperbusStruct(XSPI_HyperbusInitType* XSPI_HyperbusInitStruct)
{
    XSPI_HyperbusInitStruct->Baudr             = 0U;
    XSPI_HyperbusInitStruct->DataFrameSize     = XSPI_FRAME_SIZE_8_BIT;
    XSPI_HyperbusInitStruct->TransferMode      = XSPI_RX_ONLY_MODE;
    XSPI_HyperbusInitStruct->WaitCycles        = XSPI_WAIT_0_CYCLES;
    XSPI_HyperbusInitStruct->CAPhaseRxds       = XSPI_HYPERBUS_CA_RXDS_ENABLE;
    XSPI_HyperbusInitStruct->ClockStrech       = XSPI_CLOCK_STRETCH_DISABLE;
    XSPI_HyperbusInitStruct->TxdDriverEdge     = 0U;
    XSPI_HyperbusInitStruct->XIPHyperbus       = XSPI_NON_XIP_HYPERBUS_MODE;

    /* XIP Hyperbus Read Parameter */
    XSPI_HyperbusInitStruct->XIP_ReadHyperbusInitType.XipFixDFS   = XSPI_XIP_DFSHC_DISABLE;
    XSPI_HyperbusInitStruct->XIP_ReadHyperbusInitType.XipPrefetch = XSPI_XIP_PREFETCH_DISABLE;
}

/**
*\*\name    XSPI_EnableClockStrech.
*\*\fun     Control XSPI Clock Strech function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_EnableClockStrech(XSPI_Module* xSPIx, FunctionalState cmd)
{
    xSPIx->SSIENR &= ~XSPI_V2_EN_XSPIEN;

    if (cmd != DISABLE)
    {
        xSPIx->ENH_CTRL0 |= XSPI_V2_ENH_CTRL0_CLKSTREN;
    }
    else
    {
        xSPIx->ENH_CTRL0 &= ~XSPI_V2_ENH_CTRL0_CLKSTREN;
    }

    xSPIx->SSIENR |= XSPI_V2_EN_XSPIEN;
}

/**
*\*\name    XSPI_EnableDataMask.
*\*\fun     Control XSPI Data Mask function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_EnableDataMask(XSPI_Module* xSPIx, FunctionalState cmd)
{
    xSPIx->SSIENR &= ~XSPI_V2_EN_XSPIEN;

    if (cmd != DISABLE)
    {
        xSPIx->ENH_CTRL0 |= XSPI_V2_ENH_CTRL0_SPIDMEN;
    }
    else
    {
        xSPIx->ENH_CTRL0 &= ~XSPI_V2_ENH_CTRL0_SPIDMEN;
    }

    xSPIx->SSIENR |= XSPI_V2_EN_XSPIEN;
}

/**
*\*\name    XSPI_EnableHyperbus.
*\*\fun     Control XSPI Hyperbus function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_EnableHyperbus(XSPI_Module* xSPIx, FunctionalState cmd)
{
    if (cmd != DISABLE)
    {
        xSPIx->CTRL0 |= XSPI_V2_CTRL0_SPIHYPEEN;
    }
    else
    {
        xSPIx->CTRL0 &= ~XSPI_V2_CTRL0_SPIHYPEEN;
    }
}

/**
*\*\name    XSPI_EnableXIPHyperbusRead.
*\*\fun     Control XSPI XIP Hyperbus Read function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_EnableXIPHyperbusRead(XSPI_Module* xSPIx, FunctionalState cmd)
{
    if (cmd != DISABLE)
    {
        xSPIx->XIP_CTRL |= XSPI_V2_XIP_CTRL_XIPHYPEEN;
    }
    else
    {
        xSPIx->XIP_CTRL &= ~XSPI_V2_XIP_CTRL_XIPHYPEEN;
    }
}


/**
*\*\name    XSPI_EnableXIPHyperbusWrite.
*\*\fun     Control XSPI XIP Hyperbus Write function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_EnableXIPHyperbusWrite(XSPI_Module* xSPIx, FunctionalState cmd)
{
    if (cmd != DISABLE)
    {
        xSPIx->XIP_WRITE_CTRL |= XSPI_V2_XIP_WRITE_CTRL_XIPWRHYPEEN;
    }
    else
    {
        xSPIx->XIP_WRITE_CTRL &= ~XSPI_V2_XIP_WRITE_CTRL_XIPWRHYPEEN;
    }
}

/**
*\*\name    XSPI_SetNumberOfDataFrame.
*\*\fun     Set Number of Data Frames.
*\*\param   NumDataFrame
*\*\          - 1 ~ 65536
*\*\return  none
**/
void XSPI_SetNumberOfDataFrame(XSPI_Module* xSPIx, uint32_t NumDataFrame)
{
    if (NumDataFrame == 0)
    {
        xSPIx->CTRL1 = 0x00000000U;
    }
    else
    {
        xSPIx->CTRL1 = ((uint32_t)(NumDataFrame - 1U));
    }
}


/**
*\*\name    XSPI_Enable.
*\*\fun     Control XSPI function switch.
*\*\param   cmd
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none
**/
void XSPI_Enable(XSPI_Module* xSPIx, FunctionalState cmd)
{
    if (cmd != DISABLE)
    {
        xSPIx->SSIENR |= XSPI_V2_EN_XSPIEN;
    }
    else
    {
        xSPIx->SSIENR &= ~XSPI_V2_EN_XSPIEN;
    }
}


/**
*\*\name    XSPI_Slave_Select.
*\*\fun     Slave Select Enable.
*\*\param   Slave: 
*\*\          - XSPI_NO_SELECT_SLAVE
*\*\          - XSPI_SELECT_SLAVE_1
*\*\          - XSPI_SELECT_SLAVE_2
*\*\
*\*\return  none
**/
void XSPI_Slave_Select(XSPI_Module* xSPIx, uint32_t Slave)
{
    xSPIx->SLAVE_EN = Slave;
}


/**
*\*\name    XSPI_SetTXStartFIFOThreshold.
*\*\fun     Set Transfer start FIFO Threshold
*\*\param   fifo_level:
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL0
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL1
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL2
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL3
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL4
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL5
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL6
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL7
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL8
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL9
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL10
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL11
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL12
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL13
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL14
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL15
*\*\return  none
**/
void XSPI_SetTXStartFIFOThreshold(XSPI_Module* xSPIx, uint32_t fifo_level)
{
    uint32_t tempReg;

    tempReg = xSPIx->TXFT;
    tempReg &= ~XSPI_V2_TXFT_TXFTST;
    tempReg |= (fifo_level << 16U);

    xSPIx->TXFT = tempReg;
}

/**
*\*\name    XSPI_GetTXStartFIFOThreshold.
*\*\fun     Get Transfer start FIFO Threshold
*\*\param   fifo_level:
*\*\return  Transfer start FIFO Threshold
**/
uint16_t XSPI_GetTXStartFIFOThreshold(XSPI_Module* xSPIx)
{
    return (uint16_t)((xSPIx->TXFT & XSPI_V2_TXFT_TXFTST) >> 16U);
}


/**
*\*\name    XSPI_SetTXFIFOEmptyThreshold.
*\*\fun     Set Transmit FIFO Empty Threshold
*\*\not     When the value is less than or equal to this threshold, a transmit FIFO empty interrupt is triggered.
*\*\param   fifo_level:
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL0
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL1
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL2
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL3
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL4
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL5
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL6
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL7
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL8
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL9
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL10
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL11
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL12
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL13
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL14
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL15
*\*\return  none
**/
void XSPI_SetTXFIFOEmptyThreshold(XSPI_Module* xSPIx, uint32_t fifo_level)
{
    uint32_t tempReg;

    tempReg = xSPIx->TXFT;
    tempReg &= ~XSPI_V2_TXFT_TXFTTEI;
    tempReg |= fifo_level;
    xSPIx->TXFT = tempReg;
}

/**
*\*\name    XSPI_GetTXFIFOEmptyThreshold.
*\*\fun     Get Transmit FIFO Empty Threshold
*\*\param   fifo_level:
*\*\return  Transmit FIFO Empty Threshold
**/
uint16_t XSPI_GetTXFIFOEmptyThreshold(XSPI_Module* xSPIx)
{
    return (uint16_t)(xSPIx->TXFT & XSPI_V2_TXFT_TXFTTEI);
}


/**
*\*\name    XSPI_SetRXFIFOFullThreshold.
*\*\fun     Set Receive FIFO Full Threshold
*\*\not     When the data number is greater than or equal to (this threshold + 1), triggered receive FIFO full interrupt.
*\*\param   fifo_level:
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL0
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL1
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL2
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL3
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL4
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL5
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL6
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL7
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL8
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL9
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL10
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL11
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL12
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL13
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL14
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL15
*\*\return  none
**/
void XSPI_SetRXFIFOFullThreshold(XSPI_Module* xSPIx, uint32_t fifo_level)
{
    xSPIx->RXFT = fifo_level;
}


/**
*\*\name    XSPI_GetRXFIFOFullThreshold.
*\*\fun     Get Receive FIFO Full Threshold
*\*\param   fifo_level:
*\*\return  Receive FIFO Full Threshold
**/
uint16_t XSPI_GetRXFIFOFullThreshold(XSPI_Module* xSPIx)
{
    return (uint16_t)(xSPIx->RXFT);
}



/**
*\*\name    XSPI_GetTXFIFODataNum.
*\*\fun     Get Transmit FIFO Data Number
*\*\note    Contains the number of valid data entries in the transmit FIFO
*\*\param   fifo_level:
*\*\return  Transmit FIFO Data Number
**/
uint16_t XSPI_GetTXFIFODataNum(XSPI_Module* xSPIx)
{
    return (uint16_t)(xSPIx->TXFN);
}


/**
*\*\name    XSPI_GetRXFIFODataNum.
*\*\fun     Get Receive FIFO Data Number
*\*\note    Contains the number of valid data entries in the receive FIFO
*\*\param   fifo_level:
*\*\return  Receive FIFO Data Number
**/
uint16_t XSPI_GetRXFIFODataNum(XSPI_Module* xSPIx)
{
    return (uint16_t)(xSPIx->RXFN);
}


/**
*\*\name    XSPI_GetFlagStatus.
*\*\fun     Checks whether the specified XSPI flag is set or not.
*\*\param   Flag:
*\*\          - XSPI_BUSY_FLAG
*\*\          - XSPI_TXFNF_FLAG
*\*\          - XSPI_TXFE_FLAG
*\*\          - XSPI_RXFNF_FLAG
*\*\          - XSPI_RXFF_FLAG
*\*\          - XSPI_TXE_FLAG
*\*\          - XSPI_DCERR_FLAG
*\*\return  The new state of XSPI_FLAG (SET or RESET).
**/
FlagStatus XSPI_GetFlagStatus(XSPI_Module* xSPIx, uint32_t Flag)
{
    FlagStatus bitstatus;

    /* Check the status of the specified XSPI flag */
    if ((xSPIx->STS & Flag) != (uint32_t)RESET)
    {
        /* XSPI_FLAG is set */
        bitstatus = SET;
    }
    else
    {
        /* XSPI_FLAG is reset */
        bitstatus = RESET;
    }

    /* Return the XSPI_FLAG status */
    return  bitstatus;
}

/**
*\*\name    XSPI_GetDMACompletedDataFrameNum.
*\*\fun     Get Completed Data frames.
*\*\note    indicates total data frames transferred in the previous internal DMA transfer
*\*\param   Flag:
*\*\return  The new state of XSPI_FLAG (SET or RESET).
**/
uint32_t XSPI_GetDMACompletedDataFrameNum(XSPI_Module* xSPIx)
{
    return ((xSPIx->STS & XSPI_V2_STS_CMPLTDDF) >> 15U);
}

/**
*\*\name    XSPI_DisbaleAllInt.
*\*\fun     Disables the all XSPI interrupts.
*\*\param   XSPI_IT:
*\*\return  none.
**/
void XSPI_DisbaleAllInt(XSPI_Module* xSPIx)
{
    xSPIx->IMASK = 0x00000000U;
}


/**
*\*\name    XSPI_ConfigInt.
*\*\fun     Enables or disables the specified XSPI interrupts.
*\*\param   XSPI_IT:
*\*\          - XSPI_INT_TXFEMPTY
*\*\          - XSPI_INT_TXFOVER
*\*\          - XSPI_INT_RXFUNDER
*\*\          - XSPI_INT_RXFOVER
*\*\          - XSPI_INT_RXFFULL
*\*\          - XSPI_INT_MMCONTENTION
*\*\          - XSPI_INT_XIP_RXFOVER
*\*\          - XSPI_INT_TXFUNDER
*\*\param   Cmd:
*\*\          - ENABLE
*\*\          - DISABLE
*\*\return  none.
**/
void XSPI_EnableInt(XSPI_Module* xSPIx, uint32_t XSPI_IT, FunctionalState Cmd)
{
    uint16_t itmask;

    /* Get the Interupt mask */
    itmask = (uint16_t)(XSPI_IT & 0x000000FFU);

    if (Cmd != DISABLE)
    {
        /* Enable the selected XSPI interrupt */
        xSPIx->IMASK |= itmask;
    }
    else
    {
        /* Disable the selected XSPI interrupt */
        xSPIx->IMASK &= (uint16_t)~itmask;
    }
}


/**
*\*\name    XSPI_GetINTStatus.
*\*\fun     Checks whether the specified XSPI interrupt has occurred or not.
*\*\param   XSPI_IT:
*\*\          - XSPI_INT_TXFEMPTY
*\*\          - XSPI_INT_TXFOVER
*\*\          - XSPI_INT_RXFUNDER
*\*\          - XSPI_INT_RXFOVER
*\*\          - XSPI_INT_RXFFULL
*\*\          - XSPI_INT_MMCONTENTION
*\*\          - XSPI_INT_XIP_RXFOVER
*\*\          - XSPI_INT_TXFUNDER
*\*\return  The new state of XSPI_IT (SET or RESET).
**/
INTStatus XSPI_GetINTStatus(XSPI_Module* xSPIx, uint32_t XSPI_IT)
{
    uint16_t itpos = 0;
    INTStatus bitstatus = RESET;

    /* Get the Interupt position */
    itpos = (uint16_t)(XSPI_IT & 0x000000FFU);

    /* Check the status of the specified XSPI interrupt */
    if (((xSPIx->ISTS & itpos) != (uint16_t)RESET))
    {
        /* XSPI_IT is set */
        bitstatus = SET;
    }
    else
    {
        /* XSPI_IT is reset */
        bitstatus = RESET;
    }

    /* Return the XSPI_IT status */
    return bitstatus;
}


/**
*\*\name    XSPI_GetINTRawStatus.
*\*\fun     Get XSPI Raw Interrupt Status.
*\*\param   XSPI_IT:
*\*\          - XSPI_INT_TXFEMPTY
*\*\          - XSPI_INT_TXFOVER
*\*\          - XSPI_INT_RXFUNDER
*\*\          - XSPI_INT_RXFOVER
*\*\          - XSPI_INT_RXFFULL
*\*\          - XSPI_INT_MMCONTENTION
*\*\          - XSPI_INT_XIP_RXFOVER
*\*\          - XSPI_INT_TXFUNDER
*\*\return  The new state of XSPI_IT (SET or RESET).
**/
INTStatus XSPI_GetINTRawStatus(XSPI_Module* xSPIx, uint32_t XSPI_IT)
{
    uint16_t itpos = 0;
    INTStatus bitstatus = RESET;

    /* Get the Interupt position */
    itpos = (uint16_t)(XSPI_IT & 0x000000FFU);

    /* Check the status of the specified XSPI Raw interrupt */
    if (((xSPIx->RISTS & itpos) != (uint16_t)RESET))
    {
        /* XSPI_RAW_IT is set */
        bitstatus = SET;
    }
    else
    {
        /* XSPI_RAW_IT is reset */
        bitstatus = RESET;
    }

    /* Return the XSPI_RAW_IT status */
    return bitstatus;
}



/**
*\*\name    XSPI_ClrITPendingBit.
*\*\fun     Clears the XSPI interrupt pending bit.
*\*\param   XSPI_IT:
*\*\          - XSPI_INT_TXFOVER
*\*\          - XSPI_INT_RXFUNDER
*\*\          - XSPI_INT_RXFOVER
*\*\          - XSPI_INT_MMCONTENTION
*\*\          - XSPI_INT_XIP_RXFOVER
*\*\          - XSPI_INT_TXFUNDER
*\*\return  none.
**/
void XSPI_ClrITPendingBit(XSPI_Module* xSPIx, uint32_t XSPI_IT)
{
    uint16_t offset = 0;
    __IO uint32_t regAddr = 0;
    __IO uint32_t dummy = 0;

    /* Get register offset */
    offset = (uint16_t)(XSPI_IT >> 16U);
    /* Get Interrupt status clear register address */
    regAddr = ((uint32_t)(&xSPIx->CTRL0)) + offset;
    /* Clear Interrupt status */
    dummy = *((uint32_t *)regAddr);
}

/**
*\*\name    XSPI_ClrITAllPendingBit.
*\*\fun     Clears the XSPI interrupt all pending bit.
*\*\param   XSPI_IT:
*\*\return  none.
**/
void XSPI_ClrITAllPendingBit(XSPI_Module* xSPIx)
{
    __IO uint32_t dummy = 0;

    dummy = xSPIx->ICLR;
}



/**
*\*\name    XSPI_EnableDma.
*\*\fun     Enables or disables the XSPI DMA interface.
*\*\param   xSPIx :
*\*\param   XSPI_DMAReq :
*\*\         - XSPI_DMA_TX
*\*\         - XSPI_DMA_RX
*\*\param   Cmd :
*\*\         - ENABLE
*\*\         - DISABLE
*\*\return  none
**/
void XSPI_EnableDma(XSPI_Module* xSPIx, uint16_t XSPI_DMAReq, FunctionalState Cmd)
{
    if (Cmd != DISABLE)
    {
        /* Enable the selected XSPI DMA requests */
        xSPIx->DMA_CTRL |= XSPI_DMAReq;
    }
    else
    {
        /* Disable the selected XSPI DMA requests */
        xSPIx->DMA_CTRL &= (uint16_t)~XSPI_DMAReq;
    }
}


/**
*\*\name    XSPI_SetDMATxFIFOThreshold.
*\*\fun     Set DMA Tx FIFO Threshold
*\*\param   fifo_level:
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL0
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL1
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL2
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL3
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL4
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL5
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL6
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL7
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL8
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL9
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL10
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL11
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL12
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL13
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL14
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL15
*\*\return  none
**/
void XSPI_SetDMATxFIFOThreshold(XSPI_Module* xSPIx, uint32_t fifo_level)
{
    xSPIx->DMATDL_CTRL = fifo_level;
}


/**
*\*\name    XSPI_GetDMATxFIFOThreshold.
*\*\fun     Get DMA Tx FIFO Threshold
*\*\param   fifo_level:
*\*\return  DMA Tx FIFO Threshold
**/
uint16_t XSPI_GetDMATxFIFOThreshold(XSPI_Module* xSPIx)
{
    return ((uint16_t)(xSPIx->DMATDL_CTRL & XSPI_V2_DMATDL_CTRL_DMATDL));
}


/**
*\*\name    XSPI_SetDMARxFIFOThreshold.
*\*\fun     Set DMA Rx FIFO Threshold
*\*\not     dma_rx_req is generated when the number of valid data entries in the receive FIFO is equal to or above this field value + 1
*\*\param   fifo_level:
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL0
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL1
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL2
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL3
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL4
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL5
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL6
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL7
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL8
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL9
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL10
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL11
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL12
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL13
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL14
*\*\          - XSPI_FIFO_THRESHOLD_LEVEL15
*\*\return  none
**/
void XSPI_SetDMARxFIFOThreshold(XSPI_Module* xSPIx, uint32_t fifo_level)
{
    xSPIx->DMARDL_CTRL = fifo_level;
}


/**
*\*\name    XSPI_GetDMARxFIFOThreshold.
*\*\fun     Get DMA Rx FIFO Threshold
*\*\param   fifo_level:
*\*\return  DMA Rx FIFO Threshold
**/
uint16_t XSPI_GetDMARxFIFOThreshold(XSPI_Module* xSPIx)
{
    return ((uint16_t)(xSPIx->DMARDL_CTRL & XSPI_V2_DMARDL_CTRL_DMARDL));
}


/**
*\*\name    XSPI_GetIdentification.
*\*\fun     Get Identification code
*\*\param   fifo_level:
*\*\return  Identification code
**/
uint32_t XSPI_GetIdentification(XSPI_Module* xSPIx)
{
    return xSPIx->IDR;
}

/**
*\*\name    XSPI_GetComponentVersion.
*\*\fun     Get component version
*\*\param   fifo_level:
*\*\return  component version
**/
uint32_t XSPI_GetComponentVersion(XSPI_Module* xSPIx)
{
    return xSPIx->VERSION_ID;
}



/**
*\*\name    XSPI_SetXIPModeBit.
*\*\fun     Set XIP mode bits to be sent after address phase of XIP transfer
*\*\param   mode_bit:  0x0000 ~ 0xFFFF
*\*\return  none
**/
void XSPI_SetXIPModeBit(XSPI_Module* xSPIx, uint16_t mode_bit)
{
    xSPIx->XIP_MODE = ((uint32_t)mode_bit);
}



/**
*\*\name    XSPI_SetXIPReadIncrOpcode.
*\*\fun     Set instruction op-code to be sent when an INCR type transfer is requested on AHB bus.
*\*\param   opcode:  0x0000 ~ 0xFFFF
*\*\note    The effective length of the op-code is configured by XIP_CTRL.INSTL[1:0]
*\*\return  none
**/
void XSPI_SetXIPReadIncrOpcode(XSPI_Module* xSPIx, uint16_t opcode)
{
    xSPIx->XIP_INCR_TOC = ((uint32_t)opcode);
}


/**
*\*\name    XSPI_SetXIPReadWrapOpcode.
*\*\fun     Set instruction op-code to be sent when an WRAP type transfer is requested on AHB bus.
*\*\param   opcode:  0x0000 ~ 0xFFFF
*\*\note    The effective length of the op-code is configured by XIP_CTRL.INSTL[1:0]
*\*\return  none
**/
void XSPI_SetXIPReadWrapOpcode(XSPI_Module* xSPIx, uint16_t opcode)
{
    xSPIx->XIP_WRAP_TOC = ((uint32_t)opcode);
}



/**
*\*\name    XSPI_XIP_Slave_Select.
*\*\fun     XIP Mode Slave Select Enable.
*\*\param   Slave: 
*\*\          - XSPI_NO_SELECT_SLAVE
*\*\          - XSPI_SELECT_SLAVE_1
*\*\          - XSPI_SELECT_SLAVE_2
*\*\
*\*\return  none
**/
void XSPI_XIP_Slave_Select(XSPI_Module* xSPIx, uint32_t Slave)
{
    xSPIx->XIP_SLAVE_EN = Slave;
}



/**
*\*\name    XSPI_SetXIPReadIncrOpcode.
*\*\fun     Set the instruction op-code to be sent when an INCR type XIP Write transfer is requested on AHB bus.
*\*\param   opcode:  0x0000 ~ 0xFFFF
*\*\note    The effective length of the op-code is configured by XIP_CTRL.INSTL[1:0]
*\*\return  none
**/
void XSPI_SetXIPWriteIncrOpcode(XSPI_Module* xSPIx, uint16_t opcode)
{
    xSPIx->XIP_WRITE_INCR_INST = ((uint32_t)opcode);
}


/**
*\*\name    XSPI_SetXIPWriteWrapOpcode.
*\*\fun     Set the instruction op-code to be sent when an WRAP type XIP Write transfer is requested on AHB bus.
*\*\param   opcode:  0x0000 ~ 0xFFFF
*\*\note    The effective length of the op-code is configured by XIP_CTRL.INSTL[1:0]
*\*\return  none
**/
void XSPI_SetXIPWriteWrapOpcode(XSPI_Module* xSPIx, uint16_t opcode)
{
    xSPIx->XIP_WRITE_WRAP_INST = ((uint32_t)opcode);
}


/**
*\*\name    XSPIGetDataPointer.
*\*\fun     Get Pointer of XSPI DAT0 register.
*\*\return  The pointer of XSPI DAT0 register.
**/
uint32_t XSPI_GetDataPointer(XSPI_Module* xSPIx)
{
    return (uint32_t)&xSPIx->DAT0;
}

/**
*\*\name    XSPI_SendData.
*\*\fun     Write one data direct to XSPI DAT register to send.
*\*\param   SendData:
*\*\          - data to be send
*\*\return  none
**/
void XSPI_SendData(XSPI_Module* xSPIx, uint32_t SendData)
{
    /* Write in the DAT0 register the data to be sent */
    xSPIx->DAT0 = SendData;
}


/**
*\*\name    XSPI_ReceiveData.
*\*\fun     Read one data from XSPI DAT0 register.
*\*\return  The value of XSPI DAT0 register.
**/
uint32_t XSPI_ReceiveData(XSPI_Module* xSPIx)
{
    /* Return the data in the DAT0 register */
    return xSPIx->DAT0;
}


/**
*\*\name    XSPI_ClearRxFIFO.
*\*\fun     Clear RX FIFO
*\*\return  none
**/
void XSPI_ClearRxFIFO(XSPI_Module* xSPIx)
{
    volatile uint32_t dummy = 0U;

    /* Poll the RXFNE flag to completely clear the receive FIFO */
    while (xSPIx->STS & XSPI_V2_STS_RXFNE)
    {
        dummy = xSPIx->DAT0;
    }
}

