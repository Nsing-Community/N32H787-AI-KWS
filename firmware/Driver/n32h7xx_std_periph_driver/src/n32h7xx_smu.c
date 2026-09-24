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
 * @file n32h7xx_smu.c
 * @author Nations
 * @version From N32H7xx_Library.1.2.0
 *
 * @copyright Copyright (c) 2025, Nations Technologies Inc. All rights reserved.
 */
#include "n32h7xx_smu.h"

/* SMU Driving Functions Declaration */

/* SMU_Private_Defines  */

/**
*\*\name    SMU_GetM4BootAddr.
*\*\fun     Read boot address of CM4
*\*\param   none
*\*\return  CM4 boot address
**/
uint32_t SMU_GetM4BootAddr( void )
{
    return (*(uint32_t (*)(void))GET_M4ADDR)();
}

/**
*\*\name    SMU_SetM4BootAddr.
*\*\fun     Setting boot address of CM4
*\*\param   CM4 boot address
*\*\return  ErrorStatus:
 *\*\         - SUCCESS   Set M4 boot addr success
 *\*\         - ERROR     Set M4 boot addr failed
**/
ErrorStatus SMU_SetM4BootAddr( uint32_t addr )
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t))(SET_M4ADDR))( addr );
    if((ErrCode == SMU_UNCHANGE_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}

/**
*\*\name    SMU_GetM7BootAddr.
*\*\fun     Read boot address of CM7
*\*\param   none
*\*\return  CM7 boot address
**/
uint32_t SMU_GetM7BootAddr( void )
{
    return (*(uint32_t (*)(void))GET_M7ADDR)();
}

/**
*\*\name    SMU_SetM7BootAddr.
*\*\fun     Setting boot address of CM7
*\*\param   CM7 boot address
*\*\return  ErrorStatus:
 *\*\         - SUCCESS   Set M7 boot addr success
 *\*\         - ERROR     Set M7 boot addr failed
**/
ErrorStatus SMU_SetM7BootAddr( uint32_t addr )
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t))(SET_M7ADDR))( addr );
    if((ErrCode == SMU_UNCHANGE_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}

/**
*\*\name    SMU_GetRDPLevel.
*\*\fun     Get RDP level status
*\*\param   none
*\*\return  RDPLEVEL: RDP Level status
*\*\          - RDP_L0
*\*\          - RDP_L1
*\*\          - RDP_L2
**/
RDPLEVEL SMU_GetRDPLevel( void )
{
    return (RDPLEVEL)(*(uint32_t (*)(void))GET_RDPLevel)();
}

/**
*\*\name    SMU_SetRDPLevel.
*\*\fun     Config RDP level.
*\*\param   RDPLevel:
*\*\          - RDP_L0   : L0
*\*\          - RDP_L1   : L1
*\*\          - RDP_L2   : L2
*\*\return  ErrorStatus:
*\*\          - SUCCESS   Set RDP level success
*\*\          - ERROR     Set RDP level failed
*\*\Note:   when it configs success for the first time, this function will actively triggers a system reset. 
**/
ErrorStatus SMU_SetRDPLevel( RDPLEVEL RDPLevel )
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t))(SET_RDPLevel))( (uint32_t)RDPLevel );
    if((ErrCode == SMU_UNCHANGE_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}

/**
*\*\name    SMU_SetITCMProtection.
*\*\fun     Set ITCM protection area.
*\*\param   StrAddr: range from 0 ~ 0x00100000
*\*\param   EndAddr: range from 0 ~ 0x00100000 
*\*\return  ErrorStatus:
 *\*\         - SUCCESS   Set TCM Protection success
 *\*\         - ERROR     Set TCM Protection failed
*\*\Note1:   EndAddr must bigger than StrAddr; 
*\*\Note2:   when it configs success for the first time, this function will actively triggers a system reset. 
**/
ErrorStatus SMU_SetITCMProtection(uint32_t StrAddr, uint32_t EndAddr)
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t, uint32_t))(SET_ITCMProtection))( StrAddr, EndAddr);
    
    if((ErrCode == SMU_UNCHANGE_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}

/**
*\*\name    SMU_SetSRAMProtection.
*\*\fun     Set AHB-SRAM protection area.
*\*\param   StrAddr: range from 0x30000000 ~ 0x30058000
*\*\param   EndAddr: range from 0x30000000 ~ 0x30058000
*\*\return  ErrorStatus:
 *\*\         - SUCCESS   Set AHB-SRAM Protection success
 *\*\         - ERROR     Set AHB-SRAM Protection failed
*\*\Note1:   EndAddr must bigger than StrAddr; 
*\*\Note2:   when it configs success for the first time, this function will actively triggers a system reset.
**/
ErrorStatus SMU_SetSRAMProtection(uint32_t StrAddr, uint32_t EndAddr)
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t, uint32_t))(SET_AHBSRAMProtection))( StrAddr, EndAddr);
    
    if((ErrCode == SMU_PARA_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}


/**
*\*\name    SMU_EnWriteProtection.
*\*\fun     Config Flash Write Protection Area.
*\*\param   WRPSector: set Bitx = 0 means enable AREAx Write Protection. Every bit stand for 128k size of Flash
 *\*\         -BIT0 : Reserved
 *\*\         -BIT1 : AREA1  0x15000000 ~ 0x1501FFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         -BIT2 : AREA2  0x15020000 ~ 0x1503FFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         - ......
 *\*\         -BIT15 :AREA15 0x151C0000 ~ 0x151DFFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         -BIT16 :AREA16 0x151E0000 ~ 0x151FFFFF : Size = 128K [FLASH SIZE  =4M]
 *\*\         -BIT17 :AREA17 0x15200000 ~ 0x1521FFFF : Size = 128K [FLASH SIZE  =4M]
 *\*\         - ......
 *\*\         -BIT31 :AREA31 0x153C0000 ~ 0x153DFFFF : Size = 128K [FLASH SIZE  =4M]
*\*\return  ErrorStatus:
 *\*\         - SUCCESS   Config Flash Write Protection Area success
 *\*\         - ERROR     Config Flash Write Protection Area failed
*\*\Note:   WRPSector = 0xFFFE; when it configs success for the first time, this function will actively triggers a system reset.  
**/
ErrorStatus SMU_EnWriteProtection(uint32_t WRPSector)
{
    uint32_t ErrCode = (*(uint32_t (*)(uint32_t))(SET_FLASHWRProtection))( WRPSector);
    
    if((ErrCode == SMU_UNCHANGE_ERR) || (ErrCode == SMU_SUCCESS))
    {
        return SUCCESS;
    }
    else
    {
        /* no process */
    }
    return ERROR;
}

/**
*\*\name    SMU_GetWriteProtection.
*\*\fun     Get Flash Write Protection Area.
*\*\param   none
*\*\return  WRPSector: 
 *\*\         -BIT0 : Reserved
 *\*\         -BIT1 : 0x15000000 ~ 0x1501FFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         -BIT2 : 0x15020000 ~ 0x1503FFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         - ......
 *\*\         -BIT15 :0x151C0000 ~ 0x151DFFFF : Size = 128K [FLASH SIZE  =2M]
 *\*\         -BIT16 :0x151E0000 ~ 0x151FFFFF : Size = 128K [FLASH SIZE  =4M]
 *\*\         -BIT17 :0x15200000 ~ 0x1521FFFF : Size = 128K [FLASH SIZE  =4M]
 *\*\         - ......
 *\*\         -BIT31 :0x153C0000 ~ 0x153DFFFF : Size = 128K [FLASH SIZE  =4M]
**/
uint32_t SMU_GetWriteProtection( void )
{
    return (uint32_t)(*(uint32_t (*)(void))GET_FLASHWRProtection)();
}
/**
*\*\name    SMU_EraseFlash.
*\*\fun     Flash erase
*\*\param   Flash start address, erase the 4KB sector where the start address is located
*\*\return  Error flag
 *\*\         - FLASH_SUCCESS  : Flash operate success
 *\*\         - Other Value    : Flash operate failure
*\*\note    Erase in 4KB units
**/
uint32_t SMU_EraseFlash(uint32_t StrAddr)
{
    return (*(uint32_t (*)(uint32_t))(ER_FLASH))( StrAddr );
}

/**
*\*\name    SMU_WriteFlash.
*\*\fun     Write data to internal xSPI flash
*\*\param   Flash start address
*\*\param   The pointer of data buffer
*\*\param   Data lenght
*\*\return  Error flag
 *\*\         - FLASH_SUCCESS  : Flash operate success
 *\*\         - Other Value    : Flash operate failure
**/
uint32_t SMU_WriteFlash(uint32_t StrAddr, uint8_t *SrcBuf, uint32_t Len)
{
    return (*(uint32_t (*)(uint32_t, uint8_t*, uint32_t))(WR_FLASH))( StrAddr, SrcBuf, Len );
}
