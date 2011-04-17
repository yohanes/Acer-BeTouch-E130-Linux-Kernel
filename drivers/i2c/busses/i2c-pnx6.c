/*
 *
 * i2c-pnx.c: I2C driver for PNX
 *
 * Copyright (C) 2010 ST-Ericsson
 * Copyright (c) Purple Labs SA 2006  <freesoftware@purplelabs.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_qos_params.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/i2c.h>
#include <linux/wakelock.h>

#if defined(CONFIG_NKERNEL)
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>

struct pnx_i2c_data {
	xos_area_handle_t shared;
	struct  i2c_pnx_intf_var *base_adr;
};

static struct pnx_i2c_data pnx_i2c_data;

static u32 i2c_ctl_save;
static u32 i2c_hloddat_save;
static u32 i2c_clkho_save;
static u32 i2c_clkhi_save;
#endif

#define IIC_RX      0x00
#define IIC_TX      0x00
#define IIC_STS     0x04
#define IIC_CTL     0x08
#define IIC_CLKHI   0x0C
#define IIC_CLKLO   0x10
#define IIC_ADR     0x14
#define IIC_HOLDDAT 0x18
#define IIC_TXS     0x28

/** I2C bus clock frequency in KHz */
static int clock = 400;

/** Possible status for an on going transaction */
enum t_i2c_status {
	I2C_IDLE,
	I2C_WAIT_COMPLETION,
	I2C_END_OK,
	I2C_NO_ACK
};

enum t_i2c_pm_qos_status {
	PNXI2C_PM_QOS_DOWN,
	PNXI2C_PM_QOS_HALT,
	PNXI2C_PM_QOS_UP,
};


enum t_i2C_pm_qos_rate {
	PNXI2C_PM_QOS_STOP,
	PNXI2C_PM_QOS_XFER
};


struct pnx_i2c {
	void __iomem *base;
	int irq;
	struct clk *clk;
	struct i2c_adapter adap;
	/** Status of the current I2C transaction */
	enum t_i2c_status current_status;
	/** End of transaction event */
	wait_queue_head_t wq;
	char *name;

	/* timer for pm_qos */
	struct timer_list	pm_qos_timer;
	int			pm_qos_status;
	int			pm_qos_rate;
	struct wake_lock	pm_qos_lock;
};

#ifdef CONFIG_I2C_NEW_PROBE
static struct platform_driver i2c_pnx_driver1;
static struct platform_driver i2c_pnx_driver2;
#endif


#define I2C_VAR_AREA_NAME	"I2C_FRAME"

struct i2c_pnx_intf_var {
	volatile signed long suspend_state;
};

/*
 * ============================================================================
 * QOS & timer
 * ============================================================================
 */
static void i2c_pnx_pm_qos_up(struct pnx_i2c *i2c)
{
	/* transfer will start */
	i2c->pm_qos_rate = PNXI2C_PM_QOS_XFER;
	wmb();
	/* increase hclk2 if needed */
	if (i2c->pm_qos_status == PNXI2C_PM_QOS_DOWN) {

		wake_lock(&(i2c->pm_qos_lock));

		pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
				(char *) i2c->name, 52);

		clk_enable(i2c->clk);
		mod_timer(&(i2c->pm_qos_timer), (jiffies + HZ/50));
	}
	i2c->pm_qos_status = PNXI2C_PM_QOS_UP;
}


static void i2c_pnx_pm_qos_down(struct pnx_i2c *i2c)
{
	del_timer(&(i2c->pm_qos_timer));

	if (i2c->pm_qos_status != PNXI2C_PM_QOS_DOWN) {

		pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
				(char *) i2c->name, PM_QOS_DEFAULT_VALUE);

		i2c->pm_qos_status = PNXI2C_PM_QOS_DOWN;

		clk_disable(i2c->clk);

		wake_unlock(&(i2c->pm_qos_lock));

		/* printk("%s Set PCLK2 down to 13Mhz\n"
		 ,(char *) i2c->name ); */
	}

}

static void pnx67xx_i2c_pm_qos_halt(struct pnx_i2c *i2c)
{
	/* transfer is stopped */
	i2c->pm_qos_rate = PNXI2C_PM_QOS_STOP;
}


static void i2c_pnx_pm_qos_timeout(unsigned long arg)
{

	struct pnx_i2c *i2c = (struct pnx_i2c *) arg;

	if (i2c->pm_qos_rate == PNXI2C_PM_QOS_STOP) {

		if (i2c->pm_qos_status == PNXI2C_PM_QOS_HALT) {

			pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
					(char *) i2c->name,
					PM_QOS_DEFAULT_VALUE);

			clk_disable(i2c->clk);
			/* printk("%s Set PCLK2 down to 13Mhz\n",
				 (char *) i2c->name ); */
			i2c->pm_qos_status = PNXI2C_PM_QOS_DOWN;

			wake_unlock(&(i2c->pm_qos_lock));
		} else{
			i2c->pm_qos_status = PNXI2C_PM_QOS_HALT;
			mod_timer(&(i2c->pm_qos_timer), (jiffies + HZ/50));
		}
	} else
		mod_timer(&(i2c->pm_qos_timer), (jiffies + HZ/50));
}


/** Functionalities supported by this I2C driver */
static u32 i2c_pnx_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/** Send one byte to I2C controller as soon as FIFO has room */
static inline void i2c_pnx_send_byte(struct pnx_i2c *i2c, u8 value, u8 start
								, u8 stop)
{
	/* Wait for room in TX FIFO */
	while (readl(i2c->base+IIC_STS) & (1<<10))
		; /* tff */
	/* Send the byte */
	writel(value  | (stop ? 0x200 : 0) | (start ? 0x100 : 0)
			, i2c->base+IIC_TX);
}

/** Wait for a byte in receive FIFO and read it */
static inline u8 i2c_pnx_read_byte(struct pnx_i2c *i2c)
{
	/* Wait for char in RX FIFO */
	while (readl(i2c->base+IIC_STS) & (1<<9))
		; /*  rfe */
	/* Read one byte */
	return readl(i2c->base+IIC_RX);
}

/** Effective hardware transfer on I2C bus */
static int i2c_pnx_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[]
								, int num)
{
	int i, j;
	struct pnx_i2c *i2c = i2c_get_adapdata(adap);

	i2c_pnx_pm_qos_up(i2c);

	/* LMSqc05901:Soft reset the IIC controller to force flush RX buffer */
	writel(readl(i2c->base+IIC_CTL) | 1<<8 , i2c->base+IIC_CTL);

	i2c->current_status = I2C_WAIT_COMPLETION;
	for (i = 0; i < num; i++) {
#ifdef DEBUG
		printk(KERN_DEBUG "I2C message %d : addr:%02x len=%d %s\n", i,
				msgs[i].addr,
				msgs[i].len, (msgs[i].flags & I2C_M_RD) ? "RD"
				: "WR");
#endif /* DEBUG */
		/* START+Send address:address in the 7 upper bits,R/W in bit0 */
		i2c_pnx_send_byte(i2c, ((msgs[i].addr & 0x7F) << 1)
				| ((msgs[i].flags & I2C_M_RD) ? 1 : 0)
				, 1, (msgs[i].len == 0) ? 1 : 0);

		/* Send message bytes (dummy bytes if reading) */
		for (j = 0; j < msgs[i].len; j++)
			i2c_pnx_send_byte(i2c, msgs[i].buf[j], 0,
					(j == msgs[i].len-1)&(i == num-1));
	}

	/* wait end of transfer */
	wait_event(i2c->wq, (i2c->current_status == I2C_END_OK) ||
			(i2c->current_status == I2C_NO_ACK));

	/* Error : no acknowledge from slave */
	if (i2c->current_status == I2C_NO_ACK) {

		printk(KERN_WARNING "I2C ERROR: NACK from slave\n");
		/* Soft reset the IIC controller */
		writel(readl(i2c->base+IIC_CTL) | 1<<8 , i2c->base+IIC_CTL);
		pnx67xx_i2c_pm_qos_halt(i2c);

		return -ETIMEDOUT;
	}

	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD) {

			/* Read data from RX FIFO */
			for (j = 0; j < msgs[i].len; j++)
				msgs[i].buf[j] = i2c_pnx_read_byte(i2c);
		}
	}
	i2c->current_status = I2C_IDLE;

	pnx67xx_i2c_pm_qos_halt(i2c);
	return num;
}

/** I2C controller IRQ callback */
static int i2c_pnx_callback(int irq, void *dev_id)
{
	u32 status;
	struct pnx_i2c *i2c = dev_id;

	status = readl(i2c->base+IIC_STS);

#ifdef DEBUG
	printk(KERN_DEBUG "I2C callback status=%08x\n", status);
#endif /* DEBUG */

	/* Reset transaction done */
	if (status & 1)
		writel(1, i2c->base+IIC_STS);

	/* No acknowledge from slave */
	if (status & (1<<2)) {
		i2c->current_status = I2C_NO_ACK;
		/* validate completion */
		wake_up(&i2c->wq);
	}
	/* Transaction done */
	else if (status & 1) {
		i2c->current_status = I2C_END_OK;
		/* validate completion */
		wake_up(&i2c->wq);
	}
	return IRQ_HANDLED;
}

/* This is the actual algorithm we define */
static struct i2c_algorithm pnx_algorithm = {
	.master_xfer = i2c_pnx_xfer,
	.functionality = i2c_pnx_func,
};

/* There can only be one... */
static struct i2c_adapter pnx_adapter = {
	.owner  = THIS_MODULE,
	.class  = I2C_CLASS_HWMON,
	.algo   = &pnx_algorithm,
	.name   = "PNX I2C adapter",
};

/** I2C controller initialization and registration */
static int i2c_pnx_probe(struct platform_device *pdev)
{
	struct pnx_i2c *i2c;
	struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct i2c_platform_data *pdata =  pdev->dev.platform_data;

	i2c = kzalloc(sizeof(struct pnx_i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->name = kzalloc(strlen(pdev->name)+1, GFP_KERNEL);
	if (!(i2c->name))
		return -ENOMEM;

	strncpy(i2c->name, pdev->name, strlen(pdev->name)+1);

	i2c->base = ioremap_nocache(r->start, r->end - r->start + 1);
	if (!(i2c->base)) {
		printk(KERN_INFO "Couldn't ioremap I2C I/O region\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&i2c->wq);
	i2c->irq = platform_get_irq(pdev, 0);

	printk(KERN_INFO "PNX I2C driver (base:0x%08x irq:%d).\n",
			(u32)i2c->base, i2c->irq);

	/* Reserve I2C controller irq */
	if (request_irq(i2c->irq, i2c_pnx_callback, IRQF_DISABLED, "I2C",
				i2c)) {

		printk(KERN_WARNING "can't reserve I2C controller irq %d !\n",
				i2c->irq);
		return -EBUSY;
	}

	/* get platform data to configure bus */
	if (pdata) {
		int err = 0;
		err =  gpio_request(pdata->gpio_sda, pdev->name);
		err |= gpio_request(pdata->gpio_scl, pdev->name);
		/*if (err)
		  return -1; TODO */

		pnx_gpio_set_mode(pdata->gpio_sda, pdata->gpio_sda_mode);
		pnx_gpio_set_mode(pdata->gpio_scl, pdata->gpio_scl_mode);
	}

	/* Enable IIC controller clock : CGU_gateScX |= CGU_gateScX_iicXEn */
	if (i2c->irq == 8)
		i2c->clk = clk_get(0, "IIC2");
	else
		i2c->clk = clk_get(0, "IIC1");

	clk_enable(i2c->clk);

	/* Soft reset the IIC controller */
	writel(1 << 8 , i2c->base+IIC_CTL);

	/* Set clock divider : 100khz(default) or 400kHz */
	/* PNX67XX ES2 , following table 525 */
	if (clock == 400) {

		writel(38, i2c->base+IIC_CLKHI);
		writel(84, i2c->base+IIC_CLKLO);
		writel(21, i2c->base+IIC_HOLDDAT);
	} else {
		writel(244, i2c->base+IIC_CLKHI);
		writel(268, i2c->base+IIC_CLKLO);
		writel(33, i2c->base+IIC_HOLDDAT);
	}

	/* Enable interrupts : transaction done */
	writel((1<<0), i2c->base+IIC_CTL);

	wake_lock_init(&(i2c->pm_qos_lock), WAKE_LOCK_SUSPEND
			, (char *) pdev->name);

	i2c->pm_qos_status = PNXI2C_PM_QOS_UP;

	init_timer(&(i2c->pm_qos_timer));
	i2c->pm_qos_timer.function = &i2c_pnx_pm_qos_timeout;
	i2c->pm_qos_timer.data = (unsigned long)i2c;

	pm_qos_add_requirement(PM_QOS_PCLK2_THROUGHPUT, (char *) pdev->name
			, PM_QOS_DEFAULT_VALUE);
	i2c_pnx_pm_qos_down(i2c);

	/* create new I2C adapter instance */
	memcpy(&i2c->adap, &pnx_adapter, sizeof(pnx_adapter));

#ifdef CONFIG_I2C_NEW_PROBE
	if (strcmp(i2c->name, i2c_pnx_driver1.driver.name) == 0)
		i2c->adap.nr = 0;
	if (strcmp(i2c->name, i2c_pnx_driver2.driver.name) == 0)
		i2c->adap.nr = 1;
#endif

	/* register instance data */
	platform_set_drvdata(pdev, i2c);
	i2c_set_adapdata(&i2c->adap, i2c);

	/* return new I2C adapter to linux */
#ifdef CONFIG_I2C_NEW_PROBE
	return i2c_add_numbered_adapter(&i2c->adap);
#else
	return i2c_add_adapter(&i2c->adap);
#endif
}

static int i2c_pnx_remove(struct platform_device *pdev)
{
	struct pnx_i2c *i2c = platform_get_drvdata(pdev);

	pm_qos_remove_requirement(PM_QOS_PCLK2_THROUGHPUT,
			(char *) pdev->name);
	del_timer(&(i2c->pm_qos_timer));

	i2c_del_adapter(&i2c->adap);
	platform_set_drvdata(pdev, NULL);

	iounmap((void *)i2c->base);
	/* Free I2C controller irq */
	free_irq(i2c->irq, i2c);
	clk_put(i2c->clk);

	kfree(i2c);
	return 0;
};

static int i2c_pnx_controller_suspend(struct platform_device *pdev,
				      pm_message_t state)
{
	struct pnx_i2c *i2c = platform_get_drvdata(pdev);

	/* Check if i2c transfert is on-going */
	if (i2c->current_status == I2C_WAIT_COMPLETION) {
		return -EAGAIN;
	} else {
		i2c_pnx_pm_qos_down(i2c);
		return 0;
	}
}

#if defined(CONFIG_NKERNEL)
static int i2c_pnx_suspend_late(struct platform_device *pdev,
				      pm_message_t state)
{
	struct pnx_i2c *i2c = platform_get_drvdata(pdev);


	disable_irq(i2c->irq);

	clk_enable(i2c->clk);
	/* read actual value of IIC_CTL */
	i2c_hloddat_save = readl(i2c->base+IIC_HOLDDAT);
	i2c_clkho_save = readl(i2c->base+IIC_CLKLO);
	i2c_clkhi_save = readl(i2c->base+IIC_CLKHI);
	i2c_ctl_save = readl(i2c->base+IIC_CTL);

	/* enable master interupts and disable slave interrupts */
	writel(0x2F, i2c->base+IIC_CTL);

	clk_disable(i2c->clk);

	/*  miscellanous variables shared between linux and drvpmu */
	pnx_i2c_data.shared = xos_area_connect(I2C_VAR_AREA_NAME,
			sizeof(struct i2c_pnx_intf_var));

	if (pnx_i2c_data.shared) {
		pnx_i2c_data.base_adr = xos_area_ptr(pnx_i2c_data.shared);
		pnx_i2c_data.base_adr->suspend_state = 1;
	}

	return 0;
}

/* resume i2c */
int i2c_pnx_early_resume(struct platform_device *pdev)
{
	struct pnx_i2c *i2c = platform_get_drvdata(pdev);

	/* miscellanous variables shared between linux and drvpmu */
	pnx_i2c_data.shared = xos_area_connect(I2C_VAR_AREA_NAME,
			sizeof(struct i2c_pnx_intf_var));

	if (pnx_i2c_data.shared) {
		pnx_i2c_data.base_adr = xos_area_ptr(pnx_i2c_data.shared);
		pnx_i2c_data.base_adr->suspend_state = 0;
	}

	clk_enable(i2c->clk);

	/* restore IIC register saved values */
	writel(i2c_ctl_save, i2c->base+IIC_CTL);
	writel(i2c_clkhi_save, i2c->base+IIC_CLKHI);
	writel(i2c_clkho_save, i2c->base+IIC_CLKLO);
	writel(i2c_hloddat_save, i2c->base+IIC_HOLDDAT);

	clk_disable(i2c->clk);

	enable_irq(i2c->irq);

	return 0;
}
#endif


/* Structure for a device driver */
static struct platform_driver i2c_pnx_driver1 = {
	.probe = i2c_pnx_probe,
	.remove = i2c_pnx_remove,
	.suspend = i2c_pnx_controller_suspend,
#if defined(CONFIG_NKERNEL)
	.suspend_late = i2c_pnx_suspend_late,
	.resume_early = i2c_pnx_early_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "pnx-i2c1",
	},

};


static struct platform_driver i2c_pnx_driver2 = {
	.probe = i2c_pnx_probe,
	.remove = i2c_pnx_remove,
	.suspend = i2c_pnx_controller_suspend,
	.driver = {
		.owner = THIS_MODULE,
		.name = "pnx-i2c2",
	},
};

/** I2C controller initialization and registration */
static int __init i2c_pnx_init(void)
{
	int ret;

	ret = platform_driver_register(&i2c_pnx_driver1);
	if (ret)
		return ret;

	ret = platform_driver_register(&i2c_pnx_driver2);
	return ret;
}

static void __exit i2c_pnx_exit(void)
{
	platform_driver_unregister(&i2c_pnx_driver1);
	platform_driver_unregister(&i2c_pnx_driver2);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Purple Labs SA");
MODULE_DESCRIPTION("PNX I2C driver");

module_param(clock, int, 0);
MODULE_PARM_DESC(clock, "I2C clock frequence in KHz (default: 100)");

/* module_init(i2c_pnx_init); */
subsys_initcall(i2c_pnx_init);
module_exit(i2c_pnx_exit);
