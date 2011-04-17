/*
 * ============================================================================
 *
 * Filename:     crypto-pnx.c
 *
 * Description:  This file contains services that are common to different crypto
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



#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include "crypto-pnx.h"

#include <asm/io.h>
#include <mach/hardware.h>
#include <asm/mutex.h>
#include <linux/clk.h>

#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>



#include "osware/osware.h"
#include <asm/uaccess.h>
#include <asm/nkern.h>
#include <linux/semaphore.h>


/*#define HW_CRYPTO_SHARING_TEST*/ 	/* Activate this switch to have traces on dmesg */

/*extern NkDevOps nkops; *//* nano kernel reference */
struct semaphore cae_hw_share_rtk_sem;

#define CLEAR_EVENT_BEFORE_REGISTRATION 1 /* used in xos_ctrl_register */

/* XOS Control and Area variables to share the crypto hw ip */
static xos_ctrl_handle_t  cae_ctrl;
static xos_area_handle_t  cae_area;
static int hw_crypto_sharing = PNX_TRUE;

/* This mutex allows to share the PNX crypto hw accelerator */
static struct mutex pnx_crypto_hw_acc_mutex;
static void __iomem *base_reg;


struct pnx_crypto {
	struct device * device;
	void __iomem * cae_base;
};


/* Few words about access to the HW accelerators: 		*/
/* We do not have the same lock depending on the kind of algo. 	*/
/* As example, for SHA-1 when we start to process, we acquire 	*/
/* the block till the end of the processing. For AES, we take   */
/* the semaphore only to process block by block (16 bytes by 16 */
/* bytes). 							*/


void pnx_crypto_get_hw_acc( void )
{
    int i;
    volatile nku32_f* area_ptr;

    if( hw_crypto_sharing == PNX_TRUE )
    {
#ifdef HW_CRYPTO_SHARING_TEST
	printk("in pnx_crypto_get_hw_acc() \n");	
#endif

	/* Acquire hw crypto resource (with modem side) */
	area_ptr = xos_area_ptr(cae_area);
	do
	{
		i = *area_ptr;
		if(i<0)		/* Already in use by modem */
		{
			/* let's wait until hw block is released by modem */
			if( down_interruptible(&cae_hw_share_rtk_sem) == 0) goto hw_avail;
		}
		else	goto hw_avail;
	}while(1);

hw_avail:
	nkops.nk_atomic_add((volatile nku32_f*)area_ptr,-1);
#ifdef HW_CRYPTO_SHARING_TEST
	printk("in pnx_crypto_get_hw_acc() crypto hw access is ok (i=%d)\n",*area_ptr);	
#endif

    }

    /* lock hardware resource */
    mutex_lock(&pnx_crypto_hw_acc_mutex);
	
}
EXPORT_SYMBOL(pnx_crypto_get_hw_acc);


void pnx_crypto_release_hw_acc( void )
{
	/* unlock the semaphore */
	mutex_unlock(&pnx_crypto_hw_acc_mutex);

	if( hw_crypto_sharing == PNX_TRUE )
	{
		int i;
		volatile nku32_f* area_ptr;
	
		/* Release hw crypto resource (with modem side) */
		area_ptr = xos_area_ptr(cae_area);
		nkops.nk_atomic_add((volatile nku32_f*)area_ptr,1);
		i = *area_ptr;
		if(i>0)		/* No more in use */
			xos_ctrl_raise(cae_ctrl,(unsigned)0);
		
#ifdef HW_CRYPTO_SHARING_TEST
		printk("in pnx_crypto_release_hw_acc() prevent modem that we no more use the crypto block\n");	
#endif
	}
}
EXPORT_SYMBOL(pnx_crypto_release_hw_acc);



int  pnx_crypto_is_hw_acc_free(void)
{
	if( hw_crypto_sharing == PNX_TRUE )
	{
		int i;
		volatile nku32_f* area_ptr;
	
		/* Release hw crypto resource (with modem side) */
		area_ptr = xos_area_ptr(cae_area);
		i = *area_ptr;
		if(i<0)		/* In use */
			return(0); /* Already in use */
	}

	return(mutex_is_locked(&pnx_crypto_hw_acc_mutex));
}
EXPORT_SYMBOL(pnx_crypto_is_hw_acc_free);

void pnx_crypto_cae_hw_handler(unsigned event, void *cookies)
{
	/* sema.V() */
	up(&cae_hw_share_rtk_sem);

#ifdef HW_CRYPTO_SHARING_TEST
	printk("in pnx_crypto_cae_hw_handler() modem no more use the crypto block\n");	
#endif

}

/**
 * pnx_crypto_base_address - function for crypto driver to get base address
 * @mtd: MTD device structure
 *
 * 
 */


void __iomem *  pnx_crypto_base_address(void)
{
	return(base_reg);
}
EXPORT_SYMBOL(pnx_crypto_base_address);





static int __devinit  pnx_crypto_probe(struct platform_device *pdev )
{
	int 	ret, size;
	struct 	resource 	*res;
	struct 	pnx_crypto  	*crypto;


	ret = -ENOMEM;
	crypto = kzalloc(sizeof(*crypto), GFP_KERNEL);
	if (!crypto) goto out;

	crypto->device = &pdev->dev;


	ret = -ENXIO;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) goto err_kfree;
	size = res->end - res->start + 1;

	crypto->cae_base = ioremap(res->start, size);
	if (!crypto->cae_base) goto err_kfree;

	base_reg = crypto->cae_base;

	platform_set_drvdata(pdev, crypto);

	goto out;


	iounmap(crypto->cae_base);

err_kfree:
	kfree(crypto);

out:
	return ret;
}


static int __devexit
pnx_crypto_remove(struct platform_device *pdev)
{

	struct pnx_crypto  	*crypto;

	pr_debug("%s()\n", __FUNCTION__);

	crypto = platform_get_drvdata(pdev);
	if (!crypto)
		return 0;

	platform_set_drvdata(pdev, NULL);

	if (crypto->cae_base) {
		iounmap(crypto->cae_base);
	}

	kfree(crypto);

	return 0;
}

static struct platform_driver pnx_crypto_driver = {
	.probe		= pnx_crypto_probe,
	.remove		= pnx_crypto_remove,
	.driver		= {
		.name	= "pnx-crypto",
		.owner	= THIS_MODULE,
	},
};

static int __init pnx_crypto_init(void)
{
	volatile nku32_f* area_ptr;

	printk("ST-Ericsson / PNX platforms / crypto driver,	(c) 2009 ST-Ericsson\n");
	mutex_init(&pnx_crypto_hw_acc_mutex);

	
	/* Initialise inter-os protection (xosctrl+area) */
	cae_ctrl = xos_ctrl_connect ( "CAE", 1 );
	cae_area = xos_area_connect ( "CAE", sizeof(int) );

	if( (cae_area != NULL)&&(cae_ctrl != NULL))
	{
		/* Release hw crypto resource (with modem side) */
		area_ptr = xos_area_ptr(cae_area);

		sema_init(&cae_hw_share_rtk_sem, 1);      /* usage count is 1 */

		xos_ctrl_register ( cae_ctrl, 		/* Self */
				0,		/* Event */
               			pnx_crypto_cae_hw_handler,	  /* The callback that will be called */
               			NULL, CLEAR_EVENT_BEFORE_REGISTRATION );
				
	}
	else
	{
		hw_crypto_sharing = PNX_FALSE;
		printk("in pnx_crypto_init(), inter os sema NOT initialised =====> CHECK YOUR MODEM COMPATIBILITY !!!\n");	
	}

#ifdef HW_CRYPTO_SHARING_TEST
	printk("in pnx_crypto_init(), inter os semaphore initialised\n");	
#endif

	return platform_driver_register(&pnx_crypto_driver);
}

static void __exit pnx_crypto_exit(void)
{

    if( hw_crypto_sharing == PNX_TRUE )
	/* Unregister the message handling */
	xos_ctrl_unregister ( cae_ctrl, 0 );

	platform_driver_unregister(&pnx_crypto_driver);

}

module_init(pnx_crypto_init);
module_exit(pnx_crypto_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Franck Albesa<franck.albesa@stericsson.com>");
MODULE_DESCRIPTION("ST-Ericsson / PNX platforms / crypto driver");
MODULE_ALIAS("platform:pnx-crypto");

