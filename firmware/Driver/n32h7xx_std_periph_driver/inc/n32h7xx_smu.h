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
 * @file n32h7xx_smu.h
 * @author Nations
 * @version From N32H7xx_Library.1.2.0
 *
 * @copyright Copyright (c) 2025, Nations Technologies Inc. All rights reserved.
 */
#ifndef __N32H7XX_SMU_H__
#define __N32H7XX_SMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "n32h7xx.h"

/** n32h7xx_StdPeriph_Driver **/

/** SMU_Exported_Constants **/

/** Flash Operation Error Code Macro Definition **/
typedef enum{
    FLASH_SUCCESS           = 0U,
    FLASH_BUS_ADDR_ERROR    = 1U,
    FLASH_LOGIC_ADDR_ERROR  = 2U,
    FLASH_RESTRICTED        = 3U,
    FLASH_RDP_PROTECTED     = 4U,
    FLASH_SECURE_AREA       = 5U,
    FLASH_PFOER_AREA        = 6U,
    FLASH_WRP_PROTECTED     = 7U,
    FLASH_FAILED            = 8U
}FLASH_ErrCode;


typedef enum{
    RDP_L0 = 0U,
    RDP_L1 = 1U,
    RDP_L2 = 2U,
}RDPLEVEL;



/*API Status Code*/
#define  SMU_SUCCESS                       ((uint32_t)0x1000U)
#define  SMU_INVAILED                      ((uint32_t)0x1002U)
#define  SMU_RDP_L2_ERR                    ((uint32_t)0x1003U)
#define  SMU_UNCHANGE_ERR                  ((uint32_t)0x1004U)
#define  SMU_PARA_ERR                      ((uint32_t)0x1005U)
#define  SMU_OTHER_ERR                     ((uint32_t)0x101BU)





/*Boot API Address Pointer adresss*/
#define  GET_M4ADDR                         (0x1ff00601)
#define  SET_M4ADDR                         (0x1ff00631)
#define  GET_M7ADDR                         (0x1ff005c1)
#define  SET_M7ADDR                         (0x1ff00501)
#define  GET_RDPLevel                       (0x1ff00011)
#define  SET_RDPLevel                       (0x1ff00081)
#define  SET_AHBSRAMProtection              (0x1ff01701)
#define  SET_ITCMProtection                 (0x1ff01601)
#define  GET_FLASHWRProtection              (0x1ff001a1)
#define  SET_FLASHWRProtection              (0x1ff00201)
#define  WR_FLASH                           (0x1fff7b81)
#define  ER_FLASH                           (0x1fff7c81)






uint32_t SMU_GetM4BootAddr( void );
ErrorStatus SMU_SetM4BootAddr( uint32_t addr );
uint32_t SMU_GetM7BootAddr( void );
ErrorStatus SMU_SetM7BootAddr( uint32_t addr );

/*Set RDP Function*/
RDPLEVEL SMU_GetRDPLevel( void );
ErrorStatus SMU_SetRDPLevel( RDPLEVEL RDPLevel );

/*Set Write Protectio Area*/
ErrorStatus SMU_EnWriteProtection(uint32_t WRPSector);
uint32_t SMU_GetWriteProtection( void );

/*Set Protectio Area*/
ErrorStatus SMU_SetITCMProtection(uint32_t StrAddr, uint32_t EndAddr);
ErrorStatus SMU_SetSRAMProtection(uint32_t StrAddr, uint32_t EndAddr);

/*Flash operation*/
uint32_t SMU_EraseFlash(uint32_t StrAddr);
uint32_t SMU_WriteFlash(uint32_t StrAddr, uint8_t *SrcBuf, uint32_t Len);   
    
#ifdef __cplusplus
}
#endif

#endif /*__N32H7XX_SMU_H__ */
