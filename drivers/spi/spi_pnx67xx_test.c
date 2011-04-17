/*
 * Test Driver for PNX67XX SPI Controllers
 *
 * Copyright (C) 2009 ST-Ericsson
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <mach/gpio.h>

#include "spi_pnx67xx.h"



/* the spi->mode bits understood by this driver: */
#define MODEBITS (SPI_CPOL | SPI_CPHA | SPI_CS_HIGH)

/* these define are specific to BGW211 WLAN CHIP */
#define	HOST_S2M_MAIL_BOX_REG2	0x44
#define BOOTLOAD_FROM_HOST		0x24

static int reset_wlan_chip(void)
{
		/* reset BGW211 WLAN chip */
	/* get GPIO_A13 for BGW211 reset */

	if (pnx_request_gpio(GPIO_A13))
	{
		printk("Can't get GPIO_A13 for BGW211 reset\n");
		return -EBUSY;
	}

	pnx_set_gpio_mode(GPIO_A13,GPIO_MODE_MUX0 );

	pnx_set_gpio_direction(GPIO_A13, GPIO_DIR_OUTPUT);
	pnx_write_gpio_pin(GPIO_A13,1);
	udelay(200); 

	/* set reset to LOW */
	pnx_write_gpio_pin(GPIO_A13,0);

    udelay(8); /* at least 3.76ns */

	/* set reset to HIGH */
	pnx_write_gpio_pin(GPIO_A13,1);

	/* delay after reset to give boot code time to initialize SPI block (no transfer must be done before!) */
	udelay(200); 

	return 0;
}

static struct timer_list		spi_test_timer;
static int test_length = HZ;

static struct spi_device * spi_pnx67xx_test;

static struct workqueue_struct *workqueue;
static void spi_pnx67xx_test_timeout(struct work_struct *work);
DECLARE_DELAYED_WORK(spi_test_work, spi_pnx67xx_test_timeout);

static void spi_pnx67xx_test_timeout(struct work_struct *work)
{
	struct spi_device *spi = (struct spi_device *)spi_pnx67xx_test;
	const u8 txbuf[2] = {HOST_S2M_MAIL_BOX_REG2,0x00};
	u8 rxbuf[2]; 

	spi_write_then_read(spi,txbuf, 2, rxbuf, 2);
	printk(KERN_INFO "Wlan data read 0x%x 0x%x\n", rxbuf[0], rxbuf[1]);

	if (rxbuf[1] == BOOTLOAD_FROM_HOST )
	{
		printk("Success : BGW211 WLAN chip is present !!\n");
	}
	else
	{
		printk(KERN_ERR "Failure : BGW211 WLAN chip doesn't respond, check SPI bus configuration ...\n");
	}

	queue_delayed_work(workqueue, &spi_test_work, test_length);

	if (test_length < 1)
		test_length = HZ;
	else
		test_length--;

	printk("Next test in %d \n", test_length);

}

static int __devinit spi_pnx67xx_test_probe(struct spi_device *spi)
{
	int ret=0;
	const u8 txbuf[2] = {HOST_S2M_MAIL_BOX_REG2,0x00};
	u8 rxbuf[2]; 

	printk(KERN_INFO "spi_pnx67xx_test_probe\n");

	spi->bits_per_word = 8;
    spi->mode = SPI_MODE_3;
	spi->chip_select = GPIO_B14;
    if( spi_setup(spi) < 0 )
	{
      printk(KERN_ERR "SPI SETUP FAILED\n");
	}

	/* First of all, reset the WLAN chip*/
	if (reset_wlan_chip())
	{
		printk(KERN_ERR "Can't reset WLAN chip\n");
		ret=-EPERM;
	}

	/* Read the scratch pad value from the chip from mailbox register 2	(0x44)	*/
	/* if BGW211 WLAN chip is present and SPI bus is correctly configured,		*/
	/* we should read BOOTLOAD_FROM_HOST (0x24) from the WLAN chip				*/

	spi_write_then_read(spi,txbuf, 2, rxbuf, 2);
	printk(KERN_INFO "Wlan data read 0x%x 0x%x\n", rxbuf[0], rxbuf[1]);

	if (rxbuf[1] == BOOTLOAD_FROM_HOST )
	{
		printk("Success : BGW211 WLAN chip is present !!\n");

		printk("Starting cyclic test\n");
		spi_pnx67xx_test = spi;
		workqueue = create_singlethread_workqueue("kspitest");
		if (!workqueue)
			printk("Error with workqueue\n");

		queue_delayed_work(workqueue, &spi_test_work, HZ);
	}
	else
	{
		printk(KERN_ERR "Failure : BGW211 WLAN chip doesn't respond, check SPI bus configuration ...\n");
	}
	return ret;
}

static int __devexit spi_pnx67xx_test_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "spi_pnx67xx_test_remove\n");
	pnx_free_gpio(GPIO_A13);
	del_timer(&(spi_test_timer));
	
	return 0;
}

static struct spi_driver spi_pnx67xx_test_driver = {
	.driver = {
		.name	= "spi_pnx67xx_test",
		.owner	= THIS_MODULE,
	},
	.probe		= spi_pnx67xx_test_probe,
	.remove		= spi_pnx67xx_test_remove,
//	.remove		= __devexit_p(spi_pnx67xx_test_remove),
};


static int __init spi_pnx67xx_test_init(void)
{
	return spi_register_driver(&spi_pnx67xx_test_driver);

}
module_init(spi_pnx67xx_test_init);

static void __exit spi_pnx67xx_test_exit(void)
{
	return spi_unregister_driver(&spi_pnx67xx_test_driver);
	
}
module_exit(spi_pnx67xx_test_exit);

MODULE_DESCRIPTION("TEST PNX67xx SPI Driver bus 1");
MODULE_LICENSE("GPL");
