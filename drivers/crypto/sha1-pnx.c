/*
 * ============================================================================
 *
 * Filename:     sha1-pnx.c
 *
 * Description:  SHA1 Secure Hash Algorithm for ST-Ericsson PNX platforms.
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



#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <linux/types.h>
#include <crypto/sha.h>
#include <asm/byteorder.h>


#include <linux/io.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <linux/clk.h>
#include "crypto-pnx.h"


/* Uncomment the next line to have traces.*/
/*#define DEBUG_SHA1_PNX*/


static struct clk *hw_cae_clk;
static void __iomem* base_reg;

struct sha1_ctx {
        u64 count;
        u32 state[5];
        u8 buffer[64];
};



struct pnx_crypto {
	struct platform_device * device;
	void __iomem * cae_base;
};




static void sha1_init(struct crypto_tfm *tfm)
{
	struct sha1_ctx *sctx = crypto_tfm_ctx(tfm);
	static const struct sha1_ctx initstate = {
	  0,
	  { SHA1_H0, SHA1_H1, SHA1_H2, SHA1_H3, SHA1_H4 },
	  { 0, }
	};

	/* the clock status register */
	unsigned long clockReg;   

#ifdef DEBUG_SHA1_PNX
	printk("sha1_init() call for mutex_lock()\n");
#endif /* DEBUG_SHA_1_PNX */

	/* lock hardware resource */
	pnx_crypto_get_hw_acc();
	
	/* activate clock */
	clk_enable(hw_cae_clk);
	
	/* enabeling the SHA1 clock */
	/* reading the clock status */
	PNX_ReadRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) , clockReg);

	/* enabeling the AES clock */
	clockReg |= LLF_HASH_HW_CLK_ENABLE_HASH_VAL; 

	PNX_WriteRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) ,clockReg);
                                
	/* setting the CRYPTO_CTL register to SHA1 mode - the machine is now configured to work on AES */
	PNX_WriteRegister( base_reg+(CAE_CONTROL_CRYPTO_CTL_OFFSET), 
                      LLF_HASH_HW_CRYPTO_CTL_HASH_MODE_VAL );

	/* select between SHA1 and MD5 ==> SHA1=1/MD5=0 */
	PNX_WriteRegister( base_reg+(CAE_HASH_CONTROL_OFFSET) ,LLF_HASH_HW_HASH_CTL_SHA1_VAL );
                                

	/* wait end of operation */
	LLF_HASH_HW_WAIT_ON_HASH_BUSY_BIT() ;

	/* set initial key value (order is important !) */
	PNX_WriteRegister( base_reg+(CAE_HASH_H_4_OFFSET) ,SHA1_H4);
	PNX_WriteRegister( base_reg+(CAE_HASH_H_3_OFFSET) ,SHA1_H3);
	PNX_WriteRegister( base_reg+(CAE_HASH_H_2_OFFSET) ,SHA1_H2);
	PNX_WriteRegister( base_reg+(CAE_HASH_H_1_OFFSET) ,SHA1_H1);
	PNX_WriteRegister( base_reg+(CAE_HASH_H_0_OFFSET) ,SHA1_H0);

	/* wait end of operation */
	LLF_HASH_HW_WAIT_ON_HASH_BUSY_BIT() ;

	*sctx = initstate;
}


/**
 * Description: This function allows to load HASH block with the data
 * -----------  The load is done 512 bit per 512 bit.
 
 * Thanks to the last line, registers are loaded with:
 	r0 = data = data pointer to data block
	r1 = size = 512-bit data blocks count
	r2-r9 used to load data to HW block
	

 * code description:
   1:   ldmia	%0!,{r2-r9}\n\	== Load data to r2-r9

 * Loop to wait for HASH availability
   2:   ldr r10,[%3]\n\		== read content of CAE_CONTROL_HASH_BUSY_REG
       cmp r10, #0\n\		== check if busy
       bne 2b\n\		== While not busy, loop to 2

 * Transfert data to CAE_DIN_DATA_REG
       stmia	%2, {r2-r9}\n\	== Tranfert data from r2-r9 to CAE_DIN_DATA_REG
       ldmia	%0!,{r2-r9}\n\  == Load dat to r2-r9
       stmia	%2, {r2-r9}\n\	== Transfert data from r2-r9 to CAE_DIN_DATA_REG
       subs   %1,%1,#1 \n\	== Decrease size by one
       bne    1b\n\
 * Wait for transfert confirmation
  3:   ldr r10,[%3]\n\
       cmp r10, #0\n\
       bne 3b\n\
 	

 * Hash 512-bit data blocks
 * @param data pointer to data block
 * @size 512-bit data blocks count
 * @base_register point to the register to load with data
 */
void hw_sha1_data512(u8 *data,u32 size)
{

	if (size==0) return;

	/* We are processing 16*64 */
	/* hash_size += size*16;*/

  asm("\n\
  1:   ldmia	%0!,{r2-r9}\n\
  2:   ldr r10,[%3]\n\
       cmp r10, #0\n\
       bne 2b\n\
       stmia	%2, {r2-r9}\n\
       ldmia	%0!,{r2-r9}\n\
       stmia	%2, {r2-r9}\n\
       subs   %1,%1,#1 \n\
       bne    1b\n\
  3:   ldr r10,[%3]\n\
       cmp r10, #0\n\
       bne 3b\n\
        " : : "r" (data), "r" (size) , "r" (base_reg+(CAE_DIN_DATA_OFFSET)), "r"
	(base_reg+(CAE_CONTROL_HASH_BUSY_OFFSET))  : "r2","r3","r4","r5","r6","r7","r8","r9","r10" );
}


/* ---------------------------------------------------- */
/* data: is the pointer to the buffer to process. 	*/
/* len: is the number of bytes to process. 		*/
/* ---------------------------------------------------- */

static void sha1_update(struct crypto_tfm *tfm, const u8 *data,
			unsigned int len)
{
	struct sha1_ctx *sctx = crypto_tfm_ctx(tfm);
	unsigned int partial, done;
	const u8 *src;
	int	block_nb;
	
	partial = sctx->count & 0x3f;
	sctx->count += len;
	done = 0;
	src = data;

#ifdef DEBUG_SHA1_PNX
	{
		int i;

		printk("sha1_update() len = %d  sctx->count = %l \n", len, sctx->count );
		printk("sha1_update(), data = \n");
		for(i=0; i<len; i++)
		{
			printk("%x ", data[i]);
			if( i%20 == 0 )	printk("\n");
		}
		printk("\n");
	}
#endif /* DEBUG_SHA1_PNX */

	if ((partial + len) > 63) {
		u32 temp[SHA_WORKSPACE_WORDS];

		if (partial) {
			done = -partial;
			memcpy(sctx->buffer + partial, data, done + 64);
			/*src = sctx->buffer;*/
#ifdef DEBUG_SHA1_PNX
			printk("sha1_update(), call hw_sha1_data512() \n" );
#endif /* DEBUG_SHA1_PNX */
			hw_sha1_data512( (u8 *)sctx->buffer, 1 );
			done += 64;
			src = data + done;
			
			/* How many blocks to write ? */
			block_nb = (len-done)/64;

		}
		else /* Nothing waiting for completion in buffer */
		{
			block_nb = len/64;
			src = data;
		}

#ifdef DEBUG_SHA1_PNX
		printk("sha1_update(), call hw_sha1_data512() block_nb = %d = \n", block_nb );
#endif /* DEBUG_SHA1_PNX */
		if( block_nb > 0 )
		{
			hw_sha1_data512( (u8 *)src, block_nb );
			
			done += (64*block_nb);
			src = data + done;
		}

		memset(temp, 0, sizeof(temp));
		partial = 0;
	}

#ifdef DEBUG_SHA1_PNX
	printk("sha1_update(), memcpy() len-done = %d \n", len-done );
#endif /* DEBUG_SHA1_PNX */

	memcpy(sctx->buffer + partial, src, len - done);


}







/* Add padding and return the message digest. */
static void sha1_final(struct crypto_tfm *tfm, u8 *out)
{
	struct sha1_ctx *sctx = crypto_tfm_ctx(tfm);
	__be32	*dst = (__be32 *)out;
	/* the clock status register */
	unsigned long clockReg;   
	u32 i, index, padlen;
	__be64 bits;
	static const u8 padding[64] = { 0x80, };


	/* Manage the padding */

	bits = cpu_to_be64(sctx->count << 3);

	/* Pad out to 56 mod 64 */
	index = sctx->count & 0x3f;
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);
	sha1_update(tfm, padding, padlen);

	/* Append length */
	sha1_update(tfm, (const u8 *)&bits, sizeof(bits));


	/* Read result from registers */

	PNX_ReadRegister( base_reg+(CAE_HASH_H_4_OFFSET) , sctx->state[4]);
	PNX_ReadRegister( base_reg+(CAE_HASH_H_3_OFFSET) , sctx->state[3]);
	PNX_ReadRegister( base_reg+(CAE_HASH_H_2_OFFSET) , sctx->state[2]);
	PNX_ReadRegister( base_reg+(CAE_HASH_H_1_OFFSET) , sctx->state[1]);                             
	PNX_ReadRegister( base_reg+(CAE_HASH_H_0_OFFSET) , sctx->state[0]); 
	

	/* STEP 2 : Close the Hardware Clock */
	/* --------------------------------- */
 
	/* disable the AES clock */
	/* reading the clock status */
	PNX_ReadRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) , clockReg);
   
	/* disabeling the HASH clock */
	clockReg &= (~LLF_HASH_HW_CLK_ENABLE_HASH_VAL); 
   
	PNX_WriteRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) ,clockReg);

	/* de-activate the clock */
	clk_disable(hw_cae_clk);

	/* HW no more used */
	/* unlock the semaphore */
	pnx_crypto_release_hw_acc();

#ifdef DEBUG_SHA1_PNX
	printk("in sha1_final()\n" );
#endif
	/* Store state in digest */
	for (i = 0; i < 5; i++)
	{
		dst[i] = cpu_to_be32(sctx->state[i]);
#ifdef DEBUG_SHA1_PNX
		printk("        dst[%d] = %x\n", i, dst[i] );
#endif /* DEBUG_SHA1_PNX */
	}

	/* Wipe context */
	memset(sctx, 0, sizeof *sctx);


}

static struct crypto_alg alg = {
	.cra_name	=	"sha1",
	.cra_driver_name=	"sha1-pnx",
	.cra_flags	=	CRYPTO_ALG_TYPE_DIGEST,
	.cra_blocksize	=	SHA1_BLOCK_SIZE,
	.cra_ctxsize	=	sizeof(struct sha1_ctx),
	.cra_module	=	THIS_MODULE,
	.cra_alignmask	=	3,
	.cra_list       =       LIST_HEAD_INIT(alg.cra_list),
	.cra_u		=	{ .digest = {
	.dia_digestsize	=	SHA1_DIGEST_SIZE,
	.dia_init   	= 	sha1_init,
	.dia_update 	=	sha1_update,
	.dia_final  	=	sha1_final } }
};

static int __init sha1_pnx_mod_init(void)
{
	
#ifdef DEBUG_SHA1_PNX
	printk("My SHA1-PNX module by ST-STE Wireless\n");
#endif /* DEBUG_SHA1_PNX */

	base_reg = pnx_crypto_base_address();

	/* In order to use the clock, taking care off the sleep mode, */
	/* we have to get access to the clock of the CAE. The same pointer */
	/* can be used even after the clk_put() */
	hw_cae_clk = clk_get( 0, "CAE" );
	
	/* Then we have to release it for other modules. */
	/* Concurrency is managed inside clk module */
	clk_put(hw_cae_clk);
	

	return crypto_register_alg(&alg);
}

static void __exit sha1_pnx_mod_fini(void)
{
	crypto_unregister_alg(&alg);
}

module_init(sha1_pnx_mod_init);
module_exit(sha1_pnx_mod_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Franck Albesa<franck.albesa@stericsson.com>");
MODULE_DESCRIPTION("SHA1 Secure Hash Algorithm for ST-Ericsson / PNX platforms");

MODULE_ALIAS("sha1");
