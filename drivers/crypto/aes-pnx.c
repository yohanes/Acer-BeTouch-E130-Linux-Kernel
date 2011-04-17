/* 
 * Cryptographic API.
 *
 * AES Cipher Algorithm.
 *
 * Based on Brian Gladman's code.
 *
 * Linux developers:
 *  Alexander Kjeldaas <astor@fast.no>
 *  Herbert Valerio Riedel <hvr@hvrlab.org>
 *  Kyle McMartin <kyle@debian.org>
 *  Adam J. Richter <adam@yggdrasil.com> (conversion to 2.5 API).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 */

/* Some changes from the Gladman version:
    s/RIJNDAEL(e_key)/E_KEY/g
    s/RIJNDAEL(d_key)/D_KEY/g
*/


/* 
 * Description:
 *	This files is derived from the original aes.c one 	  
 * Where we have added the use of the PNX HW Accelerator  
 * The software part is used when the hardware is already 
 * (busy) in use by another session.
 *
 * Author:       Franck Albesa (FAL), <franck.albesa@stericsson.com>
 * Company:      ST-Ericsson Le Mans
 *
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/compiler.h>

#include <linux/crypto.h>
#include <crypto/aes.h>
#include <asm/byteorder.h>


/* Compile switch to activate HW */
#define SUPPORT_PNX_CRYPTO_ACC_HW
/* Compile swith to activate debug (printk) */
/*#define DEBUG_AES_PNX*/


#ifdef CONFIG_NET_9P 
#include <net/9p/9p.h>
#include <net/9p/client.h>
#endif

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
#include <linux/io.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <linux/clk.h>
#include "crypto-pnx.h"
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */

#undef  AES_MIN_KEY_SIZE
#define AES_MIN_KEY_SIZE	0
#define AES_MAX_KEY_SIZE	32

#define AES_BLOCK_SIZE		16


#ifdef SUPPORT_PNX_CRYPTO_ACC_HW

static struct clk *hw_cae_clk;
void __iomem *base_reg;

#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */


#define AES_SSK_SIZE 16



/*
 * #define byte(x, nr) ((unsigned char)((x) >> (nr*8))) 
 */
static inline u8
byte(const u32 x, const unsigned n)
{
	return x >> (n << 3);
}

struct aes_ctx {
	u32 key_length;
	u32 key_enc[AES_MAX_KEYLENGTH_U32];
	u32 key_dec[AES_MAX_KEYLENGTH_U32];

	u32 buf[120];
#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
	unsigned long	aesSsk;		/* PNX_TRUE = AES_SSK */
	u32 		in_key[AES_MAX_KEY_SIZE/sizeof(u32)];	/* To store key from setkey to enc/dec */
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */

};

#define E_KEY (&ctx->buf[0])
#define D_KEY (&ctx->buf[60])

static u8 pow_tab[256] __initdata;
static u8 log_tab[256] __initdata;
static u8 sbx_tab[256] __initdata;
static u8 isb_tab[256] __initdata;
static u32 rco_tab[10];
static u32 ft_tab[4][256];
static u32 it_tab[4][256];

static u32 fl_tab[4][256];
static u32 il_tab[4][256];

static inline u8 __init
f_mult (u8 a, u8 b)
{
	u8 aa = log_tab[a], cc = aa + log_tab[b];

	return pow_tab[cc + (cc < aa ? 1 : 0)];
}

#define ff_mult(a,b)    (a && b ? f_mult(a, b) : 0)

#define f_rn(bo, bi, n, k)					\
    bo[n] =  ft_tab[0][byte(bi[n],0)] ^				\
             ft_tab[1][byte(bi[(n + 1) & 3],1)] ^		\
             ft_tab[2][byte(bi[(n + 2) & 3],2)] ^		\
             ft_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rn(bo, bi, n, k)					\
    bo[n] =  it_tab[0][byte(bi[n],0)] ^				\
             it_tab[1][byte(bi[(n + 3) & 3],1)] ^		\
             it_tab[2][byte(bi[(n + 2) & 3],2)] ^		\
             it_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

#define ls_box(x)				\
    ( fl_tab[0][byte(x, 0)] ^			\
      fl_tab[1][byte(x, 1)] ^			\
      fl_tab[2][byte(x, 2)] ^			\
      fl_tab[3][byte(x, 3)] )

#define f_rl(bo, bi, n, k)					\
    bo[n] =  fl_tab[0][byte(bi[n],0)] ^				\
             fl_tab[1][byte(bi[(n + 1) & 3],1)] ^		\
             fl_tab[2][byte(bi[(n + 2) & 3],2)] ^		\
             fl_tab[3][byte(bi[(n + 3) & 3],3)] ^ *(k + n)

#define i_rl(bo, bi, n, k)					\
    bo[n] =  il_tab[0][byte(bi[n],0)] ^				\
             il_tab[1][byte(bi[(n + 3) & 3],1)] ^		\
             il_tab[2][byte(bi[(n + 2) & 3],2)] ^		\
             il_tab[3][byte(bi[(n + 1) & 3],3)] ^ *(k + n)

static void __init
gen_tabs (void)
{
	u32 i, t;
	u8 p, q;

	/* log and power tables for GF(2**8) finite field with
	   0x011b as modular polynomial - the simplest primitive
	   root is 0x03, used here to generate the tables */

	for (i = 0, p = 1; i < 256; ++i) {
		pow_tab[i] = (u8) p;
		log_tab[p] = (u8) i;

		p ^= (p << 1) ^ (p & 0x80 ? 0x01b : 0);
	}

	log_tab[1] = 0;

	for (i = 0, p = 1; i < 10; ++i) {
		rco_tab[i] = p;

		p = (p << 1) ^ (p & 0x80 ? 0x01b : 0);
	}

	for (i = 0; i < 256; ++i) {
		p = (i ? pow_tab[255 - log_tab[i]] : 0);
		q = ((p >> 7) | (p << 1)) ^ ((p >> 6) | (p << 2));
		p ^= 0x63 ^ q ^ ((q >> 6) | (q << 2));
		sbx_tab[i] = p;
		isb_tab[p] = (u8) i;
	}

	for (i = 0; i < 256; ++i) {
		p = sbx_tab[i];

		t = p;
		fl_tab[0][i] = t;
		fl_tab[1][i] = rol32(t, 8);
		fl_tab[2][i] = rol32(t, 16);
		fl_tab[3][i] = rol32(t, 24);

		t = ((u32) ff_mult (2, p)) |
		    ((u32) p << 8) |
		    ((u32) p << 16) | ((u32) ff_mult (3, p) << 24);

		ft_tab[0][i] = t;
		ft_tab[1][i] = rol32(t, 8);
		ft_tab[2][i] = rol32(t, 16);
		ft_tab[3][i] = rol32(t, 24);

		p = isb_tab[i];

		t = p;
		il_tab[0][i] = t;
		il_tab[1][i] = rol32(t, 8);
		il_tab[2][i] = rol32(t, 16);
		il_tab[3][i] = rol32(t, 24);

		t = ((u32) ff_mult (14, p)) |
		    ((u32) ff_mult (9, p) << 8) |
		    ((u32) ff_mult (13, p) << 16) |
		    ((u32) ff_mult (11, p) << 24);

		it_tab[0][i] = t;
		it_tab[1][i] = rol32(t, 8);
		it_tab[2][i] = rol32(t, 16);
		it_tab[3][i] = rol32(t, 24);
	}
}

#define star_x(x) (((x) & 0x7f7f7f7f) << 1) ^ ((((x) & 0x80808080) >> 7) * 0x1b)

#define imix_col(y,x)       \
    u   = star_x(x);        \
    v   = star_x(u);        \
    w   = star_x(v);        \
    t   = w ^ (x);          \
   (y)  = u ^ v ^ w;        \
   (y) ^= ror32(u ^ t,  8) ^ \
          ror32(v ^ t, 16) ^ \
          ror32(t,24)

/* initialise the key schedule from the user supplied key */

#define loop4(i)                                    \
{   t = ror32(t,  8); t = ls_box(t) ^ rco_tab[i];    \
    t ^= E_KEY[4 * i];     E_KEY[4 * i + 4] = t;    \
    t ^= E_KEY[4 * i + 1]; E_KEY[4 * i + 5] = t;    \
    t ^= E_KEY[4 * i + 2]; E_KEY[4 * i + 6] = t;    \
    t ^= E_KEY[4 * i + 3]; E_KEY[4 * i + 7] = t;    \
}

#define loop6(i)                                    \
{   t = ror32(t,  8); t = ls_box(t) ^ rco_tab[i];    \
    t ^= E_KEY[6 * i];     E_KEY[6 * i + 6] = t;    \
    t ^= E_KEY[6 * i + 1]; E_KEY[6 * i + 7] = t;    \
    t ^= E_KEY[6 * i + 2]; E_KEY[6 * i + 8] = t;    \
    t ^= E_KEY[6 * i + 3]; E_KEY[6 * i + 9] = t;    \
    t ^= E_KEY[6 * i + 4]; E_KEY[6 * i + 10] = t;   \
    t ^= E_KEY[6 * i + 5]; E_KEY[6 * i + 11] = t;   \
}

#define loop8(i)                                    \
{   t = ror32(t,  8); ; t = ls_box(t) ^ rco_tab[i];  \
    t ^= E_KEY[8 * i];     E_KEY[8 * i + 8] = t;    \
    t ^= E_KEY[8 * i + 1]; E_KEY[8 * i + 9] = t;    \
    t ^= E_KEY[8 * i + 2]; E_KEY[8 * i + 10] = t;   \
    t ^= E_KEY[8 * i + 3]; E_KEY[8 * i + 11] = t;   \
    t  = E_KEY[8 * i + 4] ^ ls_box(t);    \
    E_KEY[8 * i + 12] = t;                \
    t ^= E_KEY[8 * i + 5]; E_KEY[8 * i + 13] = t;   \
    t ^= E_KEY[8 * i + 6]; E_KEY[8 * i + 14] = t;   \
    t ^= E_KEY[8 * i + 7]; E_KEY[8 * i + 15] = t;   \
}

static int aes_set_key(struct crypto_tfm *tfm, const u8 *in_key,
		       unsigned int key_len)
{
	struct aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __le32 *key = (const __le32 *)in_key;
	u32 *flags = &tfm->crt_flags;
	u32 i, t, u, v, w;

#ifdef DEBUG_AES_PNX
	printk("AES-PNX aes_set_key() \n");
#endif

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW

	/* If key_len ==0 ==> we select SSK */
	if ( key_len == 0 )
	{
 
	   /* if key_len == 0 that means we have to use ssk */
	   ctx->aesSsk = PNX_TRUE;
#ifdef DEBUG_AES_PNX
	   printk("AES-PNX Key is SSK used\n");
#endif /* DEBUG_AES_PNX */

#if defined(CONFIG_NET_9P) && defined(CONFIG_NET_9P_XOSCORE)

	{
		struct 	p9_client *p_client=NULL;
		struct 	p9_fid *fid = NULL,*fid_root=NULL;
		int 	retval;
		char 	*path[] = {"sec","0"};
		char	*read_val;

		/* The modem derives a key to present it through      */
		/* /mnt/np/sec. The exported value is about 16 bytes  */

		/*struct p9_client *p9_client_create(const char *dev_name, char *options);*/
		p_client = p9_client_create("", "trans=xoscore,noextend");
		if (IS_ERR(p_client)) {
			retval = PTR_ERR(p_client);
			p_client = NULL;
			goto err;
		}
		fid_root = p9_client_attach(p_client, NULL, "nobody", 0,"");
		if (IS_ERR(fid_root)) {
			printk("AES-PNX cannot attach to 9P server\n");
			p9_client_destroy(p_client);
			retval = PTR_ERR(fid_root);
			goto err;
		}
		fid = p9_client_walk(fid_root, 2, path,0);

		if (IS_ERR(fid)) {
			printk("AES-PNX cannot walk 9P file\n");
			retval = PTR_ERR(fid);
			goto err;
		}

		retval = p9_client_open(fid, P9_OREAD);
		if (retval) {
			printk("AES-PNX cannot open 9P file\n");
			goto err;
		}

		/* We read the 16 bytes from modem.*/
		read_val = (char *)&ctx->in_key[0];
		retval = p9_client_read(fid, read_val, 0, 0, AES_SSK_SIZE);
		if (retval<=0) {		
			printk("P9 Client read failed (for AES-SSK): %d\n",retval);
			goto err;
		}
		ctx->key_length = AES_SSK_SIZE;

err:
		if( fid != NULL )	p9_client_clunk(fid);
		if( p_client != NULL )	p9_client_destroy(p_client);


#ifdef DEBUG_AES_PNX
		if( read_val != NULL )		
		{
			printk("AES retrieve SSK step OK !!! \n");
			{
				int j;
				for(j=0;j<AES_SSK_SIZE;j++) printk("%2x",*(read_val+j));
				printk("\n");
			}
		}
#endif

	}
#else	/* No key provided by modem */

		/* ==> use a static value !!!!!! SECURITY ISSUE !!!!*/
		/* Force to use a Static value (waiting for key derivation from modem side) */
		ctx->key_length = 16;
		ctx->in_key[0]=0xAAAAAAAA;
		ctx->in_key[1]=0xBBBBBBBB;
		ctx->in_key[2]=0xCCCCCCCC;
		ctx->in_key[3]=0xDDDDDDDD;	

#endif

	}
	else
	{
	
	   ctx->aesSsk = PNX_FALSE;
	   /* Store the key in the context */
	   ctx->in_key[0] = le32_to_cpu(key[0]);
	   ctx->in_key[1] = le32_to_cpu(key[1]);
	   ctx->in_key[2] = le32_to_cpu(key[2]);
	   ctx->in_key[3] = le32_to_cpu(key[3]);

	   if( key_len >=24 )
	   {
		ctx->in_key[4] = le32_to_cpu(key[4]);
		ctx->in_key[5] = le32_to_cpu(key[5]);
	   }
	   if( key_len >=32 )
	   {
		ctx->in_key[6] = le32_to_cpu(key[6]);
		ctx->in_key[7] = le32_to_cpu(key[7]);
	   }
	}


	/* In all cases, we let the key_schedule_blob calculation  */
	/* even if we know that we will try to use HW accel. later */
	/* in encrypt / decrypt functions 			   */

#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */

	if (key_len % 8) {
		*flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		return -EINVAL;
	}

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
	if(ctx->aesSsk == PNX_FALSE)
		ctx->key_length = key_len;
	else
		ctx->key_length = AES_SSK_SIZE;
#else
	ctx->key_length = key_len;
#endif

	E_KEY[0] = le32_to_cpu(key[0]);
	E_KEY[1] = le32_to_cpu(key[1]);
	E_KEY[2] = le32_to_cpu(key[2]);
	E_KEY[3] = le32_to_cpu(key[3]);

	switch (key_len) {
	case 16:
		t = E_KEY[3];
		for (i = 0; i < 10; ++i)
			loop4 (i);
		break;

	case 24:
		E_KEY[4] = le32_to_cpu(key[4]);
		t = E_KEY[5] = le32_to_cpu(key[5]);
		for (i = 0; i < 8; ++i)
			loop6 (i);
		break;

	case 32:
		E_KEY[4] = le32_to_cpu(key[4]);
		E_KEY[5] = le32_to_cpu(key[5]);
		E_KEY[6] = le32_to_cpu(key[6]);
		t = E_KEY[7] = le32_to_cpu(key[7]);
		for (i = 0; i < 7; ++i)
			loop8 (i);
		break;
	}

	D_KEY[0] = E_KEY[0];
	D_KEY[1] = E_KEY[1];
	D_KEY[2] = E_KEY[2];
	D_KEY[3] = E_KEY[3];

	for (i = 4; i < key_len + 24; ++i) {
		imix_col (D_KEY[i], E_KEY[i]);
	}


	return 0;
}


#ifdef SUPPORT_PNX_CRYPTO_ACC_HW

static int aes_set_key_hw(struct crypto_tfm *tfm, unsigned long Enc )
{
	struct aes_ctx *ctx = crypto_tfm_ctx(tfm);
	u32 i;
	
	/* the AES control value */
	unsigned long aesControlVal;
   
	/* the hardware version value */
	unsigned long hw_version;
   
	/* the clock status register */
	unsigned long clockReg;   

	/* defining the number of key registers to load value */
	unsigned long numOfKeyRegsToLoad;                     
      


	/* activate clock */
	clk_enable(hw_cae_clk);


	/* ................. check the version & content ........................... */
	/* ------------------------------------------------------------------------- */

	/* checking access to the hardware by reading the version */   
	PNX_ReadRegister(base_reg+(CAE_CONTROL_VERSION_OFFSET) , hw_version);

	if( hw_version != LLF_AES_HW_VERSION_VAL )

	  return LLF_AES_HW_VERSION_NOT_CORRECT_ERROR; 

	/* ................. general registers initializations ..................... */
	/* ------------------------------------------------------------------------- */
   

	/* enabeling the AES clock */
	/* reading the clock status */
	PNX_ReadRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) , clockReg);
   
	/* enabeling the AES clock */
	clockReg |= LLF_AES_HW_CLK_ENABLE_AES_VAL; 
   
	PNX_WriteRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) ,clockReg);
                                
	/* setting the CRYPTO_CTL register to AES mode - the machine is now configured to work on AES */
	PNX_WriteRegister( base_reg+(CAE_CONTROL_CRYPTO_CTL_OFFSET), 
                      LLF_AES_HW_CRYPTO_CTL_AES_MODE_VAL );
                                       
	/* On CTR the machine does not work with the data out buffer directly
           and data in buffer directly there for the read and write alignment are 0 */         
 
	PNX_WriteRegister( base_reg+(CAE_ALIGN_CONTROL_OFFSET) ,           
            (0UL << LLF_AES_HW_DOUT_READ_ALIGN_VAL_POS) |
            (0UL << LLF_AES_HW_DOUT_WRITE_ALIGN_VAL_POS) |
            (0x1UL << LLF_AES_HW_DOUT_CTL_ALIGN_EN_POS) );
            
 
	/* .................. AES registers initialization .......................... */
	/* -------------------------------------------------------------------------- */ 
          
	/* ....... setting the AES control register according to the values in the AES context */
   
	/* building the AES control value */
	aesControlVal = 0;
 
	/* loading the DEC mode */
	if( Enc == PNX_TRUE )
	     aesControlVal |= LLF_AES_HW_AES_CTL_ENCRYPT_VAL;
        else             
  	     aesControlVal |= LLF_AES_HW_AES_CTL_DECRYPT_VAL;


	/* loading the KEY size value */

	/* 128 bits */     
	if( ctx->key_length == 16 )
	{
	    aesControlVal |= LLF_AES_HW_AES_CTL_KEY128_VAL;
	    numOfKeyRegsToLoad = 4;
	}
	else
	{

		/* 192 bits */     
		if( ctx->key_length == 24 )
		{
		    aesControlVal |= LLF_AES_HW_AES_CTL_KEY192_VAL;
	            numOfKeyRegsToLoad = 6;
		}
		else
		{
		   
		    if( ctx->key_length == 32 )
		    {
		        /* 256 bits */
			aesControlVal |= LLF_AES_HW_AES_CTL_KEY256_VAL;
			numOfKeyRegsToLoad = 8;
		    }
		    else
			return LLF_AES_HW_INTERNAL_ERROR_3;
		}
       
	}/* end of setting the KEY size switch case */    


	/* loading the operation mode */

	/* by default, we set the ECB/CTR one */
	aesControlVal |= LLF_AES_HW_AES_CTL_ECB_MODE_VAL;

	/* setting the register AES control register with the calculated value */ 
	PNX_WriteRegister( base_reg+(CAE_AES_CONTROL_OFFSET) , aesControlVal );


	/* .............. loading the key registers ........... */

	for( i = 0 ; i < numOfKeyRegsToLoad ; i++ )
        {
		PNX_WriteRegister( (base_reg+(CAE_AES_KEY_0_OFFSET)) + (i*sizeof(u32)) , ctx->in_key[i] );

#ifdef DEBUG_AES_PNX
	      printk("\n aes_set_key_hw()/ Key key[%d] = %x\n", i, ctx->in_key[i]);
#endif /* DEBUG_AES_PNX */

	}

   
	/* ................ waiting until the HW machine is enabled */
    
	LLF_AES_HW_WAIT_ON_AES_BUSY_BIT();

	/* At this stage, the keys are loaded */

	return 0;

}

#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */










/* encrypt a block of text */

#define f_nround(bo, bi, k) \
    f_rn(bo, bi, 0, k);     \
    f_rn(bo, bi, 1, k);     \
    f_rn(bo, bi, 2, k);     \
    f_rn(bo, bi, 3, k);     \
    k += 4

#define f_lround(bo, bi, k) \
    f_rl(bo, bi, 0, k);     \
    f_rl(bo, bi, 1, k);     \
    f_rl(bo, bi, 2, k);     \
    f_rl(bo, bi, 3, k)

static void aes_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	const struct aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __be32 *src = (const __be32 *)in;
	__be32 *dst = (__be32 *)out;
	__be32 read_val;
	u32 b0[4], b1[4];
	const u32 *kp = E_KEY + 4;
	

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW

	/* By default the HW accel have to be used. */
	unsigned long UseSwCrypto = PNX_FALSE;

	/* getting the hardware sempaphre */
#ifdef DEBUG_AES_PNX
	printk("AES-PNX aes_encrypt() \n");
#endif

   
	/* In order to improve perf, we propose to: */
	/* Check if the semaphore is already locked with non blocking call */
	if( pnx_crypto_is_hw_acc_free() )
	{
		/* Here we consider that waiting for hw would takes */
		/* more time than calculating in SW */
	
		/* Use Software */
		UseSwCrypto = PNX_TRUE;

	}
	else
	{
		unsigned long clockReg;   
		u32 i;

		/* Semaphore not already used ==> take it */
		/* lock hardware resource */
		pnx_crypto_get_hw_acc();
			
		/* Set the key */
		aes_set_key_hw( tfm, PNX_TRUE );

		/* Then load the data to encrypt */

		/* load the block of data from in buffer to the hw  */
		/* ------------------------------------------------ */

		/* load the DATA_IN registers */
		for( i = 0 ; i < (AES_BLOCK_SIZE/4) ; i++ )
		{
			PNX_WriteRegister( (base_reg+(CAE_DIN_DATA_OFFSET)) + (i*sizeof(u32)), src[i] );
		}

		/* waiting until the AES busy register is 0 */
		LLF_AES_HW_WAIT_ON_AES_BUSY_BIT();
          

  		/* Then read the result */

		/* load the block of data from hw to the buffer  */
		/* --------------------------------------------- */
      
		/* load the DATA_IN registers */
		for( i = 0 ; i < (AES_BLOCK_SIZE/4) ; i++ )
		{
			PNX_ReadRegister( (base_reg+(CAE_DOUT_DATA_OFFSET)) + (i*sizeof(u32)), read_val );
			dst[i] = le32_to_cpu(read_val);
		}


		/* disable the AES clock */
		/* reading the clock status */
		PNX_ReadRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) , clockReg);
   
		/* disabeling the AES clock */
		clockReg &= (~LLF_AES_HW_CLK_ENABLE_AES_VAL); 
   
		PNX_WriteRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) ,clockReg);

		/* de-activate the clock */
		clk_disable(hw_cae_clk);

		
		/* HW no more used */
		/* unlock the semaphore */
		pnx_crypto_release_hw_acc();


	}

	if( UseSwCrypto == PNX_TRUE )
	{
	
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */


	b0[0] = le32_to_cpu(src[0]) ^ E_KEY[0];
	b0[1] = le32_to_cpu(src[1]) ^ E_KEY[1];
	b0[2] = le32_to_cpu(src[2]) ^ E_KEY[2];
	b0[3] = le32_to_cpu(src[3]) ^ E_KEY[3];

	if (ctx->key_length > 24) {
		f_nround (b1, b0, kp);
		f_nround (b0, b1, kp);
	}

	if (ctx->key_length > 16) {
		f_nround (b1, b0, kp);
		f_nround (b0, b1, kp);
	}

	f_nround (b1, b0, kp);
	f_nround (b0, b1, kp);
	f_nround (b1, b0, kp);
	f_nround (b0, b1, kp);
	f_nround (b1, b0, kp);
	f_nround (b0, b1, kp);
	f_nround (b1, b0, kp);
	f_nround (b0, b1, kp);
	f_nround (b1, b0, kp);
	f_lround (b0, b1, kp);

	dst[0] = cpu_to_le32(b0[0]);
	dst[1] = cpu_to_le32(b0[1]);
	dst[2] = cpu_to_le32(b0[2]);
	dst[3] = cpu_to_le32(b0[3]);

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
	}
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */


}

/* decrypt a block of text */

#define i_nround(bo, bi, k) \
    i_rn(bo, bi, 0, k);     \
    i_rn(bo, bi, 1, k);     \
    i_rn(bo, bi, 2, k);     \
    i_rn(bo, bi, 3, k);     \
    k -= 4

#define i_lround(bo, bi, k) \
    i_rl(bo, bi, 0, k);     \
    i_rl(bo, bi, 1, k);     \
    i_rl(bo, bi, 2, k);     \
    i_rl(bo, bi, 3, k)

static void aes_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	const struct aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const __be32 *src = (const __be32 *)in;
	__be32 *dst = (__be32 *)out;
	u32 read_val;
	u32 b0[4], b1[4];
	const int key_len = ctx->key_length;
	const u32 *kp = D_KEY + key_len + 20;

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW

	/* By default the HW accel have to be used. */
	unsigned long UseSwCrypto = PNX_FALSE;

	/* getting the hardware sempaphre */
#ifdef DEBUG_AES_PNX
	printk("AES-PNX aes_decrypt() \n");
#endif
   
	/* In order to improve perf, we propose to: */
	/* Check if the semaphore is already locked with non blocking call */
	if( pnx_crypto_is_hw_acc_free() )
	{
		/* Here we consider that waiting for hw would takes */
		/* more time than calculating in SW */
	
		/* Use Software */
		UseSwCrypto = PNX_TRUE;
	}
	else
	{
		unsigned long clockReg;   
		u32 i;

		/* Semaphore not already used ==> take it */
		/* lock hardware resource */
		pnx_crypto_get_hw_acc();
			
		/* Set the key */
		aes_set_key_hw( tfm, PNX_FALSE );

		/* Then load the data to encrypt */

		/* load the block of data from in buffer to the hw  */
		/* ------------------------------------------------ */
      
		/* load the DATA_IN registers */
		for( i = 0 ; i < (AES_BLOCK_SIZE/4) ; i++ )
		{
			PNX_WriteRegister( (base_reg+(CAE_DIN_DATA_OFFSET)) + (i*sizeof(u32)), src[i] );
		}
		
		/* waiting until the AES busy register is 0 */
		LLF_AES_HW_WAIT_ON_AES_BUSY_BIT();
          

  		/* Then read the result */

		/* load the block of data from hw to the buffer  */
		/* --------------------------------------------- */
      
		/* load the DATA_IN registers */
		for( i = 0 ; i < (AES_BLOCK_SIZE/4) ; i++ )
		{
			PNX_ReadRegister( (base_reg+(CAE_DOUT_DATA_OFFSET)) + (i*sizeof(u32)), read_val );
			dst[i] = le32_to_cpu(read_val);
		}

		/* disable the AES clock */
		/* reading the clock status */
		PNX_ReadRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) , clockReg);
   
		/* disabeling the AES clock */
		clockReg &= (~LLF_AES_HW_CLK_ENABLE_AES_VAL); 
   
		PNX_WriteRegister( base_reg+(CAE_MISC_CLK_ENABLE_OFFSET) ,clockReg);

		/* de-activate the clock */
		clk_disable(hw_cae_clk);

		
		/* HW no more used */
		/* unlock the semaphore */
		pnx_crypto_release_hw_acc();
		
	}

	if( UseSwCrypto == PNX_TRUE )
	{
	
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */



	b0[0] = le32_to_cpu(src[0]) ^ E_KEY[key_len + 24];
	b0[1] = le32_to_cpu(src[1]) ^ E_KEY[key_len + 25];
	b0[2] = le32_to_cpu(src[2]) ^ E_KEY[key_len + 26];
	b0[3] = le32_to_cpu(src[3]) ^ E_KEY[key_len + 27];

	if (key_len > 24) {
		i_nround (b1, b0, kp);
		i_nround (b0, b1, kp);
	}

	if (key_len > 16) {
		i_nround (b1, b0, kp);
		i_nround (b0, b1, kp);
	}

	i_nround (b1, b0, kp);
	i_nround (b0, b1, kp);
	i_nround (b1, b0, kp);
	i_nround (b0, b1, kp);
	i_nround (b1, b0, kp);
	i_nround (b0, b1, kp);
	i_nround (b1, b0, kp);
	i_nround (b0, b1, kp);
	i_nround (b1, b0, kp);
	i_lround (b0, b1, kp);

	dst[0] = cpu_to_le32(b0[0]);
	dst[1] = cpu_to_le32(b0[1]);
	dst[2] = cpu_to_le32(b0[2]);
	dst[3] = cpu_to_le32(b0[3]);

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
	}
#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */


}


static struct crypto_alg aes_alg = {
	.cra_name		=	"aes",
	.cra_driver_name	=	"aes-pnx",
	.cra_priority		=	100,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct aes_ctx),
	.cra_alignmask		=	3,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(aes_alg.cra_list),
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	AES_MIN_KEY_SIZE,
			.cia_max_keysize	=	AES_MAX_KEY_SIZE,
			.cia_setkey	   	= 	aes_set_key,
			.cia_encrypt	 	=	aes_encrypt,
			.cia_decrypt	  	=	aes_decrypt
		}
	}
};




static int __init aes_init(void)
{

#ifdef SUPPORT_PNX_CRYPTO_ACC_HW
	
#ifdef DEBUG_AES_PNX
	printk("My AES-PNX module by ST-Ericsson\n");
#endif

	/* Load base address of crypto block */
	base_reg = pnx_crypto_base_address();

	/* In order to use the clock, taking care off the sleep mode, */
	/* we have to get access to the clock of the CAE. The same pointer */
	/* can be used even after the clk_put() */
	hw_cae_clk = clk_get( 0, "CAE" );
	
	/* Then we have to release it for other modules. */
	/* Concurrency is managed inside clk module */
	clk_put(hw_cae_clk);
	

#endif /* SUPPORT_PNX_CRYPTO_ACC_HW */

	gen_tabs();
	return crypto_register_alg(&aes_alg);
}

static void __exit aes_fini(void)
{
	crypto_unregister_alg(&aes_alg);
}

module_init(aes_init);
module_exit(aes_fini);

MODULE_AUTHOR("Franck Albesa<franck.albesa@stericsson.com>");
MODULE_DESCRIPTION("Rijndael (AES) Cipher Algorithm");
MODULE_DESCRIPTION("ST-Ericsson / PNX platforms / AES driver ");
MODULE_LICENSE("Dual BSD/GPL");

