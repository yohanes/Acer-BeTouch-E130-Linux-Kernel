/*
 * ============================================================================
 *
 * Filename:     crypto-pnx.h
 *
 * Description:  
 *
 * Version:      1.0
 * Created:      03.03.2009 15:15:20
 * Revision:     none
 * Compiler:     gcc
 *
 * Author:       Franck Albesa (FAL), <franck.albesa@stericsson.com>
 * Company:      ST-Ericsson Le Mans
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * changelog:
 *
 * ============================================================================
 */




/**
 * struct mv_xor_device - internal representation of a XOR device
 * @pdev: Platform device
 * @id: HW XOR Device selector
 * @dma_desc_pool: base of DMA descriptor region (DMA address)
 * @dma_desc_pool_virt: base of DMA descriptor region (CPU address)
 * @common: embedded struct dma_device
 */

void pnx_crypto_get_hw_acc( void );
void pnx_crypto_release_hw_acc( void );
int  pnx_crypto_is_hw_acc_free(void);
void __iomem *  pnx_crypto_base_address(void);



#define 	PNX_FALSE  0
#define		PNX_TRUE   1






/************************ Defines ******************************/

/* the definitions of the AES hardware registers offset above the base address 
   determaind by the user and the relevant bits */
   
/* spesific AES registers */   
#define LLF_AES_HW_AES_KEY_0_ADDR   0x0410UL   
#define LLF_AES_HW_AES_KEY_1_ADDR   0x0414UL   
#define LLF_AES_HW_AES_KEY_2_ADDR   0x0418UL   
#define LLF_AES_HW_AES_KEY_3_ADDR   0x041CUL   
#define LLF_AES_HW_AES_KEY_4_ADDR   0x0420UL   
#define LLF_AES_HW_AES_KEY_5_ADDR   0x0424UL   
#define LLF_AES_HW_AES_KEY_6_ADDR   0x0428UL   
#define LLF_AES_HW_AES_KEY_7_ADDR   0x042CUL
#define LLF_AES_HW_AES_SK_ADDR      0x0434UL

#define LLF_AES_HW_AES_SK_LOAD_VAL  0x0001UL

#define LLF_AES_HW_AES_IV_0_ADDR    0x0438UL   
#define LLF_AES_HW_AES_IV_1_ADDR    0x043CUL   
#define LLF_AES_HW_AES_IV_2_ADDR    0x0440UL   
#define LLF_AES_HW_AES_IV_3_ADDR    0x0444UL
#define LLF_AES_HW_AES_CTL_ADDR     0x05C0UL

#define LLF_AES_HW_AES_CTL_ENCRYPT_VAL  0x0000UL
#define LLF_AES_HW_AES_CTL_DECRYPT_VAL  0x0001UL
#define LLF_AES_HW_AES_CTL_KEY128_VAL   (0x0000UL << 1 )
#define LLF_AES_HW_AES_CTL_KEY192_VAL   (0x0001UL << 1 )
#define LLF_AES_HW_AES_CTL_KEY256_VAL   (0x0002UL << 1 )
#define LLF_AES_HW_AES_CTL_ECB_MODE_VAL (0x0000UL << 3 )
#define LLF_AES_HW_AES_CTL_CBC_MODE_VAL (0x0001UL << 3 )
#define LLF_AES_HW_AES_CTL_MAC_MODE_VAL (0x0002UL << 3 )
 
#define LLF_AES_HW_AES_BUSY_ADDR    0x0A14UL

/* data interface registers */
#define LLF_AES_HW_DIN_DOUT_ADDR          0x0C00UL
#define LLF_AES_HW_DOUT_ALIGN_CTL_ADDR    0x0E04UL

#define LLF_AES_HW_DOUT_CTL_ALIGN_EN_POS    0UL 
#define LLF_AES_HW_DOUT_READ_ALIGN_VAL_POS  1UL
#define LLF_AES_HW_DOUT_WRITE_ALIGN_VAL_POS 3UL
  
#define LLF_AES_HW_READ_LAST_DATA_ADDR    0x0E08UL

/* general CRYPTOCELL interface registers */
#define LLF_AES_HW_CLK_ENABLE_ADDR         0x09D0UL

#define LLF_AES_HW_CLK_DISABLE_AES_VAL     0x0000UL
#define LLF_AES_HW_CLK_ENABLE_AES_VAL      0x0001UL

#define LLF_AES_HW_CRYPTO_CTL_ADDR         0x0A00UL

#define LLF_AES_HW_CRYPTO_CTL_AES_MODE_VAL 0x0001UL

#define LLF_AES_HW_VERSION_ADDR            0x0A28UL

#define LLF_AES_HW_VERSION_VAL             0x0002UL

#define LLF_AES_SW_RESET                   0x0A2CUL


/* ........... Error base numeric mapping definitions ................... */
/* ----------------------------------------------------------------------- */
 
 /* The global error base number */
#define CRYS_ERROR_BASE          0x00F00000UL


/* AES module on the LLF layer base address -  0x00F10000 */
#define LLF_AES_MODULE_ERROR_BASE   (CRYS_ERROR_BASE+0)   


#define LLF_AES_HW_VERSION_NOT_CORRECT_ERROR   (LLF_AES_MODULE_ERROR_BASE + 0x0UL)
#define LLF_AES_HW_INTERNAL_ERROR_1            (LLF_AES_MODULE_ERROR_BASE + 0x11UL)
#define LLF_AES_HW_INTERNAL_ERROR_2            (LLF_AES_MODULE_ERROR_BASE + 0x12UL)
#define LLF_AES_HW_INTERNAL_ERROR_3            (LLF_AES_MODULE_ERROR_BASE + 0x13UL)
#define LLF_AES_HW_INTERNAL_ERROR_4            (LLF_AES_MODULE_ERROR_BASE + 0x14UL)
#define LLF_AES_HW_INTERNAL_ERROR_5            (LLF_AES_MODULE_ERROR_BASE + 0x15UL)

/* Waiting for semaphore has been interrupted */
#define LLF_AES_HW_NOT_AVAILABLE_ERROR   (LLF_AES_MODULE_ERROR_BASE + 0x20UL)


/* the HW address space */
#define LLF_AES_HW_CRYPTO_ADDR_SPACE ( 1UL << 10 )

/* ********************** Macros ******************************* */

/*#define PNX_WriteRegister(addr,val)  ( (*((volatile unsigned long*)(addr))) = (unsigned long)(val) )
#define PNX_ReadRegister(addr,val)  ( (val) = (*((volatile unsigned long*)(addr))) )
*/
#define PNX_WriteRegister(addr,val)  ( writel(val,addr) )
#define PNX_ReadRegister(addr,val)  ( val = readl(addr) )

/* defining a macro for waitiong to the AES busy register */
#define LLF_AES_HW_WAIT_ON_AES_BUSY_BIT( ) \
do \
{ \
   volatile unsigned long  output_reg_val; \
   do \
   { \
      PNX_ReadRegister( base_reg+(CAE_CONTROL_AES_BUSY_OFFSET/4) , output_reg_val ); \
   }while( output_reg_val ); \
}while(0)    






/************************ Defines ******************************/

/* the definitions of the HASH hardware registers offset above the base address 
   determaind by the user and the relevant bits */

/* spesific HASH values */   


#define LLF_HASH_HW_HASH_CTL_MD5_VAL  0x0000UL
#define LLF_HASH_HW_HASH_CTL_SHA1_VAL 0x0001UL
 

#define LLF_HASH_HW_CLK_DISABLE_HASH_VAL     0x0000UL
#define LLF_HASH_HW_CLK_ENABLE_HASH_VAL      0x0004UL

#define LLF_HASH_HW_CRYPTO_CTL_HASH_MODE_VAL 0x0007UL



/* ********************** Macros ******************************* */

/* defining a macro for waitiong to the HASH busy register */
#define LLF_HASH_HW_WAIT_ON_HASH_BUSY_BIT( ) \
do \
{ \
   volatile unsigned long output_reg_val; \
   do \
   { \
      PNX_ReadRegister( base_reg+(CAE_CONTROL_HASH_BUSY_OFFSET/4) , output_reg_val ); \
   }while( output_reg_val ); \
}while(0)    


