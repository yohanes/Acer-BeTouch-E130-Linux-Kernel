/*
 * linux/arch/arm/mach-pnx67xx/devices.c
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Description:  Device specification for the PNX67XX
 *
 *     Created:  03/02/2010 02:52:27 PM
 *      Author:  Philippe Langlais (PLA), philippe.langlais@stericsson.com
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/android_pmem.h>

#include <linux/pcf506XX.h>

#include <mach/hardware.h>
#include <mach/vde.h>
#include <mach/gpio.h>
#include <mach/usb.h>
#include <mach/i2c.h>

#define CONFIG_PMEM_POOL_START_ADDR 0x24800000
#define CONFIG_PMEM_POOL_SIZE 0x2000000


/*****************************************************************************
 * EXTINT to GPIO mapping
 *****************************************************************************/
unsigned char extint_to_gpio[NR_EXTINT] = {
	/*extint 0*/ GPIO_A0,
	/*extint 1*/ GPIO_A1,
	/*extint 2*/ GPIO_A2,
	/*extint 3*/ GPIO_A3,
	/*extint 4*/ GPIO_A4,
	/*extint 5*/ GPIO_A5,
	/*extint 6*/ GPIO_A12,
	/*extint 7*/ GPIO_A13,
	/*extint 8*/ GPIO_A14,
	/*extint 9*/ GPIO_A15,
	/*extint 10*/ GPIO_A16,
	/*extint 11*/ GPIO_A17,
	/*extint 12*/ GPIO_D19,
	/*extint 13*/ GPIO_A19,
	/*extint 14*/ GPIO_A20,
	/*extint 15*/ GPIO_B11,
	/*extint 16*/ GPIO_E30,
	/*extint 17*/ GPIO_D15,
	/*extint 18*/ GPIO_D20,
	/*extint 19*/ GPIO_B9,
	/*extint 20*/ GPIO_B7,
	/*extint 21*/ GPIO_A25,
	/*extint 22*/ GPIO_D18,
	/*extint 23*/ GPIO_A6
};
EXPORT_SYMBOL(extint_to_gpio);

struct gpio_bank pnx_gpio_bank[7] = {
	{(void __iomem*) GPIOA_PINS_REG, (void __iomem*) SCON_SYSMUX0_REG},
	{(void __iomem*) GPIOB_PINS_REG, (void __iomem*) SCON_SYSMUX2_REG},
	{(void __iomem*) GPIOC_PINS_REG, (void __iomem*) SCON_SYSMUX4_REG},
	{(void __iomem*) GPIOD_PINS_REG, (void __iomem*) SCON_SYSMUX6_REG},
	{(void __iomem*) GPIOE_PINS_REG, (void __iomem*) SCON_SYSMUX8_REG},
	{(void __iomem*) GPIOF_PINS_REG, (void __iomem*) SCON_SYSMUX10_REG},
	{NULL, NULL},
};

static struct gpio_data pnx_gpio_data = {
	ARRAY_SIZE(pnx_gpio_bank), /* nb bank */
	pnx_gpio_bank
};

static struct resource pnx_wavex_gpio_resources[] = {
	[0] = {
		.start = GPIOA_BASE_ADDR, /* Physical address */
		.end   = GPIOA_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device pnx_wavex_gpio_device = {
	.name		= "pnx-gpio",
	.id		= -1,
	.dev		= {
		.platform_data = &pnx_gpio_data,
	},
	.num_resources	= ARRAY_SIZE(pnx_wavex_gpio_resources),
	.resource	= pnx_wavex_gpio_resources,
};

/******************************************************************************
 * USB devices
 ******************************************************************************/
/* PNX67XX USB platform device */
static struct resource usb_resources[] = {
	{
	 .start = USB_BASE_ADDR,	/* Physical address */
		.end = (USB_BASE_ADDR + 0x1D8),
		.flags = IORESOURCE_MEM,
	 },
	{
		.start = IRQ_USB,
		.flags = IORESOURCE_IRQ,
	 },
	{
		.start = IRQ_EXTINT(17),/* GPIO_D15 for usb_need_clock */
		.flags = IORESOURCE_IRQ,
	},
};

static u64 usb_dmamask = ~(u32)0;

static struct __devinitdata usb_platform_data pnx_usb_data = {
	.usb_register_pmu_callback   = pcf506XX_registerUsbFct,
	.usb_unregister_pmu_callback = pcf506XX_unregisterUsbFct,
	.usb_cable_plugged = pcf506XX_usbChgPlugged,
	.usb_vdd_onoff               = usb_set_vdd_onoff,
	.gpio_host_usbpwr            = GPIO_A26,
	.gpio_mux_host_usbpwr        = GPIO_MODE_MUX2,
	.usb_suspend_onoff			 = pcf506XX_SetUsbSuspend,	
};

/* PNX67XX EHCI/UDC platform device */
static struct platform_device ehci_udc_device = {
	.name = "pnx67xx_ehci_udc",
	.id = 0,
	.dev = {
		.platform_data = &pnx_usb_data,
		.dma_mask	= &usb_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(usb_resources),
	.resource = usb_resources,
};

/******************************************************************************
 * I2C devices
 ******************************************************************************/
static struct resource pnx_i2c1_resources[] = {
  [0] = {
    .name	= "i2c1 base",
    .start	= I2C1_BASE_ADDR, /* Physical address */
    .end	= I2C1_BASE_ADDR + SZ_4K - 1,
    .flags	= IORESOURCE_MEM,
  },
  [1] = {
    .name	= "i2c1 irq",
    .start	= IRQ_I2C1,
    .flags	= IORESOURCE_IRQ,
  },
};

static struct platform_device i2c1_device = {
	.name		= "pnx-i2c1",
	.id		= 0,
	.dev = {
		.platform_data = NULL,
		},
	.num_resources	= ARRAY_SIZE(pnx_i2c1_resources),
	.resource	= pnx_i2c1_resources,
};

static struct __devinitdata i2c_platform_data pnx_i2c2_data = {
	.gpio_sda		= GPIO_C30,
	.gpio_sda_mode	= GPIO_MODE_MUX0,
	.gpio_scl       = GPIO_C31,
	.gpio_scl_mode  = GPIO_MODE_MUX0,
};

static struct resource pnx_i2c2_resources[] = {
  [0] = {
    .name	= "i2c2 base",
    .start	= I2C2_BASE_ADDR, /* Physical address */
    .end	= I2C2_BASE_ADDR + SZ_4K - 1,
    .flags	= IORESOURCE_MEM,
  },
  [1] = {
    .name	= "i2c2 irq",
    .start	= IRQ_I2C2,
    .flags	= IORESOURCE_IRQ,
  },
};

static struct platform_device i2c2_device = {
	.name		= "pnx-i2c2",
	.id		= 0,
	.dev = {
		.platform_data = &pnx_i2c2_data,
		},
	.num_resources	= ARRAY_SIZE(pnx_i2c2_resources),
	.resource	= pnx_i2c2_resources,
};

/******************************************************************************
 * VDE frame buffer device
 ******************************************************************************/
static struct resource vde_resource[] = {
	[0] = {
		.start = VDE_BASE_ADDR,  /* Physical address */
		.end   = VDE_BASE_ADDR + SZ_4K -1,
		.flags = IORESOURCE_MEM,
	},
    [1] = {
        .start	= IRQ_VDE,
        .flags	= IORESOURCE_IRQ,
    },
};

static struct __devinitdata vde_platform_data vde_platform_data = {
	.clk_div = 0,		/* clock divider is 1 (ARM clock @208MHz) */
	.burst = 1,		/* 1 burst enabled */
};

static struct platform_device vde_device = {
	.name = "pnx-vde",
	.id = -1,
	.num_resources = ARRAY_SIZE(vde_resource),
	.resource = vde_resource,
	.dev = {
		.platform_data	= &vde_platform_data,
	},
};

/******************************************************************************
 * TVO frame buffer device
 ******************************************************************************/
static struct resource tvo_resource[] = {
	[0] = {
		.start = TVO_BASE_ADDR,  /* Physical address */
		.end   = TVO_BASE_ADDR + SZ_4K -1,
		.flags = IORESOURCE_MEM,
	},
    [1] = {
        .start	= IRQ_TVO,
        .flags	= IORESOURCE_IRQ,
    },
};

static struct platform_device tvo_device = {
	.name = "pnx-tvo",
	.id = -1,
	.num_resources = ARRAY_SIZE(tvo_resource),
	.resource = tvo_resource,
};

/******************************************************************************
 * Crypto cryptographic device
 ******************************************************************************/
static struct resource crypto_resource[] = {
	[0] = {
		.start = CAE_BASE_ADDR,
		.end   = CAE_BASE_ADDR + SZ_4K -1,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device crypto_device = {
	.name = "pnx-crypto",
	.id = -1,
	.num_resources = ARRAY_SIZE(crypto_resource),
	.resource = crypto_resource,
};

#ifdef ACER_L1_AUX
/*******************************************************************************
 * ACER_Ed 2009.06.08
 * TI BRF6450 Bluetooth device
 *******************************************************************************/
static struct platform_device ti_bt_power_device = {
	.name = "bt_power",
};

static int bluetooth_power(int on)
{
   printk(KERN_DEBUG "%s\n", __func__);

	printk(KERN_INFO
		"%s: current power_state=%d\n",
		__func__, on);

   if(on)
   {
		pnx_gpio_set_mode(GPIO_A10, 1);	//ACER Ed 2009.12.11, Vincent patch
		pnx_gpio_write_pin(GPIO_A7, 1); 

   } 
   else
   {
		pnx_gpio_write_pin(GPIO_A7, 0); 
		pnx_gpio_set_mode(GPIO_A10, 0);	//ACER Ed 2009.12.11, Vincent patch
   }

   return 0;
}

static int bt_detect_irq = 1;
static int (*bt_irq_call)(void);

struct hci_sleep_ops {
	int (*enter)(void);
	int (*leave)(void);
};

static struct hci_sleep_ops bt_detect_ops;

static irqreturn_t bluetooth_detect_irq(int irq, void *dev_id)
{
	printk(KERN_DEBUG "%s\n", __func__);

	if (bt_detect_irq == 0)
	{
		bt_detect_irq++;
		disable_irq(IRQ_EXTINT(2));
	}

	(bt_irq_call)();
	return IRQ_HANDLED;
}

int bluetooth_enter_sleep(void)
{
	printk(KERN_DEBUG "%s\n", __func__);

	pnx_gpio_set_mode(GPIO_A10, 0);	

	if (bt_detect_irq >= 1)
	{
		pnx_gpio_clear_irq(IRQ_EXTINT(2));
		bt_detect_irq--;
		enable_irq(IRQ_EXTINT(2));
	}

	return 0;
}

int bluetooth_leave_sleep(void)
{
	printk(KERN_DEBUG "%s\n", __func__);

	if (bt_detect_irq == 0)
	{
		bt_detect_irq++;
		disable_irq(IRQ_EXTINT(2));
	}

	pnx_gpio_set_mode(GPIO_A10, 1);	

	return 0;
}

extern int ll_device_register_sleep_ops(struct hci_sleep_ops *sleep_ops);

static int bluetooth_detect_init(void)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	/* request gpio and initialize */
	pnx_gpio_request(GPIO_A10);
	pnx_gpio_write_pin(GPIO_A10, 0);
	pnx_gpio_set_direction(GPIO_A10, 0);
	pnx_gpio_set_mode(GPIO_A10, 1);	

	/* request and initialize extint for cts pulse*/
	if (set_irq_type(IRQ_EXTINT(2), IRQ_TYPE_EDGE_BOTH))
	{
		printk(KERN_WARNING "failed setirq type\n");
	}
	ret =  request_irq(IRQ_EXTINT(2), bluetooth_detect_irq, 0, " BT (wakeup)", NULL);
	if (ret)
	{
		printk(KERN_WARNING "request_irq failed\n");
	}
	disable_irq(IRQ_EXTINT(2));

	/* register bt wakeup detection function */
	bt_detect_ops.enter = bluetooth_enter_sleep;
	bt_detect_ops.leave = bluetooth_leave_sleep;
	bt_irq_call = ll_device_register_sleep_ops(&bt_detect_ops);

   return 0;
}


static void __init bt_power_init(void)
{
   pnx_gpio_request (GPIO_A7);
   pnx_gpio_set_direction (GPIO_A7, GPIO_DIR_OUTPUT);
   ti_bt_power_device.dev.platform_data = &bluetooth_power;   

   bluetooth_detect_init();
}
#endif

#ifdef CONFIG_ANDROID_PMEM
/******************************************************************************
 * PMEM hardware memory allocator device
 ******************************************************************************/
static struct android_pmem_platform_data pmem_resource = {
	.name = "pmem",
	.start = CONFIG_PMEM_POOL_START_ADDR,
	.size =  CONFIG_PMEM_POOL_SIZE,
	.no_allocator = 0,  /* use allocator */
	.cached = 1,        /* cacheable */
};

static struct platform_device pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &pmem_resource },
};
#endif /* CONFIG_ANDROID_PMEM */

/* list of generic devices that must be registered 1st */
static struct platform_device *platform_devs_pre[] __initdata = {
	&pnx_wavex_gpio_device,
	&i2c1_device,
	&i2c2_device
};

/* list of generic devices that must be registered after board dedicated
 * devices */
static struct platform_device *platform_devs_post[] __initdata = {
	&ehci_udc_device,
#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
#endif
	&vde_device,
	&tvo_device,
#ifdef ACER_L1_AUX
	&ti_bt_power_device, //ACER_Ed 2009.6.8 add BT device.
#endif
	&crypto_device
};


/* Register generic devices that must be registered 1st */
int __init pnx67xx_devices_init_pre(void)
{
	pr_debug("%s()\n", __func__);
	platform_add_devices(platform_devs_pre, ARRAY_SIZE(platform_devs_pre));
	return 0;
}

/* Register generic devices that must be registered last */
int __init pnx67xx_devices_init_post(void)
{
	pr_debug("%s()\n", __func__);
	platform_add_devices(platform_devs_post, ARRAY_SIZE(platform_devs_post));
#ifdef ACER_L1_AUX
	bt_power_init(); //ACER_Ed 2009.6.8 add BT device.
#endif
	return 0;
}

/* Now the devices are registerd in init_machine function
 * arch_initcall(pnx67xx_devices_init); */

