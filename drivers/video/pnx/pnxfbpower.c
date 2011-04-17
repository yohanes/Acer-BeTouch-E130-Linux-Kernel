/*
 * linux/drivers/video/pnx/pnxfbpower.c
 *
 * PNX framebuffer - power management
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/pm_qos_params.h>
#include <mach/pwr.h>
#include <mach/power.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/delay.h>

#define PNXFB_REQ_NAME "pnx_fb_power"

/*
 * Power management struct
 *
 */
struct pnxpower {
	struct atomic_notifier_head notifier;
	struct notifier_block nb;
	struct clk *tvo_clk_pll;
	int switched_off;
	int is_startup;
};

/* Global variable */
static struct pnxpower gpwr;

/*
 *
 *
 */
static inline const char *pwr_state_to_char(int state)
{
	switch (state) {
	case PWR_ENABLE_PRECHANGE:
		return "PWR_ENABLE_PRECHANGE";
		break;

	case PWR_ENABLE_POSTCHANGE:
		return "PWR_ENABLE_POSTCHANGE";
		break;

	case PWR_DISABLE_PRECHANGE:
		return "PWR_DISABLE_PRECHANGE";
		break;

	case PWR_DISABLE_POSTCHANGE:
		return "PWR_DISABLE_POSTCHANGE";
		break;

	default:
		return "UNKNOWN";
		break;
	}
}

/*
 *
 */
static int pnxpwr_reset_hw(struct pnxpower *pwr)
{
	int ret = 0;
	unsigned int regValue;

	//printk("%s ()\n", __FUNCTION__);

	/***********************/
	/* Reset the IVS block */
	/***********************/
	regValue = ioread32(WDRU_CON_REG);
	regValue = (regValue & WDRU_RESIVS_FIELD) | WDRU_RESIVS_1;

	iowrite32(regValue, WDRU_CON_REG);
	udelay(100);

	/***********************/
	/* Reset the TVO       */
	/***********************/

	/* enable TVO PLL clock */
	clk_enable(gpwr.tvo_clk_pll);

	regValue = ioread32(WDRU_CON_REG);
	regValue = (regValue & WDRU_RESTVO_FIELD) | WDRU_RESTVO_1;

	iowrite32(regValue, WDRU_CON_REG);
	udelay(100);

	/* disable TVO PLL clock */
	clk_disable(gpwr.tvo_clk_pll);

	return ret;
}

/*
 *
 *
 */
static int
pnxpwr_state_change(struct notifier_block *nb, unsigned long state, void *data)
{
	int ret = NOTIFY_DONE, err;

	switch (state) {
		/* switch ON (pwr_enable) */
	case PWR_ENABLE_PRECHANGE:
		break;

	case PWR_ENABLE_POSTCHANGE:
		/* Check if the IVS was switched OFF */
		if ((gpwr.switched_off) || (gpwr.is_startup)) {
			/* HW Reset */
			pnxpwr_reset_hw(&gpwr);

			/*  IVS is ON */

			/* --> Set HCLK2 constraint (104 Mhz) */
			err = pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
							PNXFB_REQ_NAME, 104);
			if (err < 0) {
				printk
				    ("%s (pm_qos_update_requirement (HCLK2) failed ! (%d))\n",
				     __FUNCTION__, err);
			}

			/* --> Set DDR constraint (180 MB/s) */
			err = pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT,
							PNXFB_REQ_NAME, 180);
			if (err < 0) {
				printk
				    ("%s (pm_qos_update_requirement (DDR) failed ! (%d))\n",
				     __FUNCTION__, err);
			}

			/* Reset the switch off flag */
			gpwr.switched_off = 0;
			gpwr.is_startup = 0;
		}
		break;

		/* switch OFF (pwr_disable) */
	case PWR_DISABLE_PRECHANGE:
		break;

	case PWR_DISABLE_POSTCHANGE:
		/* Set the switch off flag */
		gpwr.switched_off = 1;

		/*  IVS is OFF */

		/* --> Set HCLK2 constraint (104 Mhz) */
		err = pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
						PNXFB_REQ_NAME,
						PM_QOS_DEFAULT_VALUE);
		if (err < 0) {
			printk
			    ("%s (pm_qos_update_requirement (HCLK2) failed ! (%d))\n",
			     __FUNCTION__, err);
		}

		/* --> Set DDR constraint (default value) */
		err = pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT,
						PNXFB_REQ_NAME,
						PM_QOS_DEFAULT_VALUE);
		if (err < 0) {
			printk
			    ("%s (pm_qos_update_requirement (DDR) failed ! (%d))\n",
			     __FUNCTION__, err);
		}

		break;

	default:
		break;
	}

	/* printk("%s (State changed to %s)\n",
	   __FUNCTION__, pwr_state_to_char(state)); */

	return ret;
}

/*
 *
 */
static int __init pnxpwr_init(void)
{
	int ret = 0;
	struct pwr *pwr;

	pr_debug("%s()\n", __FUNCTION__);

	/* Initialize the global variable */
	memset(&gpwr, 0, sizeof(struct pnxpower));

	gpwr.is_startup = 1;

	/* Common notification callback */
	gpwr.nb.notifier_call = pnxpwr_state_change;

	/* get the power */
	pwr = pwr_get(NULL, "ivs_pw");
	if (IS_ERR(pwr)) {
		printk("%s Failed (Unable to get the IVS power)\n",
		       __FUNCTION__);
	}

	/* register notifier */
	pwr->notifier = &gpwr.notifier;
	ret = pwr_register_notifier(&gpwr.nb, pwr);
	if (ret < 0) {
		printk("%s Failed (Unable to register IVS power CB)\n",
		       __FUNCTION__);
	}

	/* Add QOS requirement */
	ret = pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT,
				     PNXFB_REQ_NAME, PM_QOS_DEFAULT_VALUE);
	if (ret < 0) {
		printk("%s (pm_qos_add_requirement (HCLK2) failed ! (%d))\n",
		       __FUNCTION__, ret);
	}

	ret = pm_qos_add_requirement(PM_QOS_DDR_THROUGHPUT,
				     PNXFB_REQ_NAME, PM_QOS_DEFAULT_VALUE);
	if (ret < 0) {
		printk("%s (pm_qos_add_requirement (DDR) failed ! (%d))\n",
		       __FUNCTION__, ret);
	}

	/* grab TVO PLL clock */
	gpwr.tvo_clk_pll = clk_get(NULL, "TVOPLL");
	if (IS_ERR(gpwr.tvo_clk_pll)) {
		printk("%s Failed ! (Could not get the PLL clock of TVO)\n",
		       __FUNCTION__);
	}

	/* release the power */
	pwr_put(pwr);

	return ret;
}

/*
 *
 */
static void __exit pnxpwr_exit(void)
{
	int ret;
	struct pwr *pwr;

	pr_debug("%s()\n", __FUNCTION__);

	/* get the power */
	pwr = pwr_get(NULL, "ivs_pw");
	if (IS_ERR(pwr)) {
		printk("%s Failed (Unable to get the IVS power)\n",
		       __FUNCTION__);
	} else {
		/* unregister notifier */
		ret = pwr_unregister_notifier(&gpwr.nb, pwr);
		if (ret < 0) {
			printk
			    ("%s Failed (Unable to Unregister IVS power CB)\n",
			     __FUNCTION__);
		}

		/* release the power */
		pwr_put(pwr);
	}

	/* Remove QOS requirement */
	pm_qos_remove_requirement(PM_QOS_HCLK2_THROUGHPUT, PNXFB_REQ_NAME);
	pm_qos_remove_requirement(PM_QOS_DDR_THROUGHPUT, PNXFB_REQ_NAME);

	/* release tvo clk pll */
	if (!IS_ERR(gpwr.tvo_clk_pll)) {
		clk_put(gpwr.tvo_clk_pll);
	}
}

module_init(pnxpwr_init);
module_exit(pnxpwr_exit);

MODULE_AUTHOR("Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("IVS Power management");
MODULE_LICENSE("GPL");
