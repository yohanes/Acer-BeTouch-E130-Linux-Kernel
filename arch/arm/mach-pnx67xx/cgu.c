/*
 *  linux/arch/arm/mach-pnx67xx/cgu.c
 *
 *  PNX Clock Gating Unit basic driver/wrapper
 *
 *  Author:	Arnaud Troel
 *  Copyright:	ST-Ericsson (c) 2007
 *
 *  Based on the code from Michel Jaouen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/clock.h>

#if defined(CONFIG_NKERNEL)
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>

#include "cpu-freq_pnx.h"
#include "clock_pnx67xx.h"
#include "cpu-idle_rtke.h"
#endif

#include "cgu.h"

#define MODULE_NAME "PNX_CGU"
#define PKMOD MODULE_NAME ": "

#if defined(DEBUG_CGU)
#define debug(fmt, args...)                                          \
	printk(PKMOD fmt, ## args)
#else
#define debug(fmt, args...)
#endif

#if defined(CONFIG_NKERNEL)
struct rtke_cgu_ctxt {
	xos_area_handle_t shared;
	struct clockmngt_shared *base_adr;
	clock_constraint root;
};

struct clockmngt_shared tmp_clkmngt_shared;

static struct rtke_cgu_ctxt cgu_ctxt = {
	.shared = NULL,
	.base_adr = NULL,
	.root.global = 0,
};
#endif

/*-------------------------------------------------------------------------
 * PNX hw low level access clock functions
 *-------------------------------------------------------------------------*/
static void pnx_clk_disable ( struct clk * clk );
static int pnx_clk_enable ( struct clk * clk );

/**
 * HW enable clock function.
 **/

static int pnx67xx_cgu_enable_fake_clock(struct clk *clk)
{
	debug("pnx67xx_cgu_enable_fake_clock for %s\n", clk->name);
	return 0;
}

static int pnx67xx_cgu_enable_hw_clock(struct clk *clk)
{
    unsigned long value;
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
	debug("pnx67xx_cgu_enable_hw_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Enable for %s without enable register\n",
		       clk->name);
		return 0;
	}
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

    value = inl ( clk->enable_reg );
    value |= 1 << clk->enable_bit;
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	return 0;
}

static int pnx67xx_cgu_enable_tvo_pll_clock(struct clk *clk)
{
    unsigned long value;
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif

	debug("pnx67xx_cgu_enable_tvo_pll_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Enable for %s without enable register\n",
		       clk->name);
		return 0;
	}
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	value = inl ( clk->enable_reg );
    value |= 1 << clk->enable_bit;
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	/* wait pll lock */
	do {
		value = inl ( clk->enable_reg );
	} while(!(value & (0x1 << 16)));

	return 0;
}

static int pnx67xx_cgu_enable_rtke_clock(struct clk *clk)
{
#ifdef CONFIG_NKERNEL
    unsigned long value;

	debug("pnx67xx_cgu_enable_rtke_clock for %s\n", clk->name);

	/* send event to rtke is necessary !! */

	value = cgu_ctxt.root.global;
    value |= 1 << clk->enable_bit;
    cgu_ctxt.root.global = value;
#endif

	return 0;
}

static int pnx67xx_cgu_enable_shared_clock(struct clk *clk)
{
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
    unsigned long value;

	debug("pnx67xx_cgu_enable_shared_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Enable for %s without enable register\n",
		       clk->name);
		return 0;
	}
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );

    if ( strcmp(clk->name,"DMAU")==0 )
		cgu_ctxt.base_adr->kernel.dma = 1;
	else if ( strcmp(clk->name,"MTU")==0 )
		cgu_ctxt.base_adr->kernel.mtu = 1;

#endif /* CONFIG_NKERNEL */

    value = inl ( clk->enable_reg );
    value |= 1 << clk->enable_bit;
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */
	return 0;
}

static int pnx67xx_cgu_enable_clock(struct clk *clk)
{
	printk(PKMOD "No enable clock method for %s."
	       " You should use new one !!!\n", clk->name);
	return 0;
}

static int pnx67xx_cgu_enable_camout(struct clk *clk)
{
    unsigned long value;
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
	debug("pnx67xx_cgu_enable_hw_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Enable for %s without enable register\n",
		       clk->name);
		return 0;
	}

	pnx_clk_enable(&fix_ck);
	pnx_clk_enable(&plltv_ck);

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

    value = inl ( clk->enable_reg );
    value |= 1 << clk->enable_bit;
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */
	
	udelay(1);

	pnx_clk_disable(&fix_ck);
	pnx_clk_disable(&plltv_ck);

	return 0;
}

/**
 * HW disable clock function.
 **/

static void pnx67xx_cgu_disable_fake_clock(struct clk *clk)
{
	debug("pnx67xx_cgu_disable_fake_clock for %s\n", clk->name);
}

static void pnx67xx_cgu_disable_hw_clock(struct clk *clk)
{
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
    unsigned long value;

	debug("pnx67xx_cgu_disable_hw_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Disable for %s without enable register\n",
		       clk->name);
		return;
	}
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	value = inl ( clk->enable_reg );
    value &= ~(1<<clk->enable_bit);
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	return;
}

static void pnx67xx_cgu_disable_rtke_clock(struct clk *clk)
{
#ifdef CONFIG_NKERNEL
    unsigned long value;

	debug("pnx67xx_cgu_disable_rtke_clock for %s\n", clk->name);

	/* send event to rtke is necessary !! */

	value = cgu_ctxt.root.global;
    value &= ~(1<<clk->enable_bit);
    cgu_ctxt.root.global = value;
#endif

	return;
}

static void pnx67xx_cgu_disable_shared_clock(struct clk *clk)
{
#ifdef CONFIG_NKERNEL
    unsigned long flags;
#endif
    unsigned long value;

	debug("pnx67xx_cgu_disable_shared_clock for %s\n", clk->name);

	if (unlikely(clk->enable_reg == 0)) {
		printk(KERN_ERR "Disable for %s without enable register\n",
		       clk->name);
		return;
	}
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );

	if (strcmp(clk->name, "DMAU") == 0) {
		cgu_ctxt.base_adr->kernel.dma = 0;

		if ( cgu_ctxt.base_adr->rtke.dma != 0 )
			goto bail_out;
	} else if (strcmp(clk->name, "MTU") == 0) {
		cgu_ctxt.base_adr->kernel.mtu = 0;

		if ( cgu_ctxt.base_adr->rtke.mtu != 0 )
			goto bail_out;
    }
#endif /* CONFIG_NKERNEL */

    value = inl ( clk->enable_reg );
    value &= ~(1<<clk->enable_bit);
    outl ( value, clk->enable_reg );

#ifdef CONFIG_NKERNEL
bail_out:
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */
}

static void pnx67xx_cgu_disable_clock(struct clk *clk)
{
	printk(PKMOD
	       " No disable clock method for %s. You should use one !!!\n",
	       clk->name);
}

/**
 * set rate clock function.
 **/

static int pnx_clk_set_rate_fci(struct clk *clk, unsigned long rate)
{
	int ret = 0;
	unsigned long ratio = 0;
	unsigned long reg;
#ifdef CONFIG_NKERNEL
	unsigned long flags;
#endif

	if (!rate)
		ratio=31;
	else{
		if (rate > CLK_FC_CLK_MAX_FREQ)
			rate = CLK_FC_CLK_MAX_FREQ;

		/* compute diviser ratio */
		ratio = (clk->parent->rate/rate)-1;	
		if(ratio < 2)
			ratio = 2;
		if(ratio > 31)
			ratio = 31;
		}
	clk->rate = clk->parent->rate/(ratio+1);

	/* for voltage working point only */
	pnx67xx_set_freq (FC_CLK, clk->rate/1000 , clk->rate/1000 );

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	reg = inl(CGU_FIXCON_REG);
	reg &= ~(0x1F << 22);
	reg |= (ratio << 22);
	outl(reg, CGU_FIXCON_REG);

	reg = inl(CGU_SCCON_REG);
	outl(reg, CGU_SCCON_REG);

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	return ret;
}

static int pnx_clk_set_rate_camout(struct clk *clk, unsigned long rate)
{
	int ret = 0;
	unsigned long ratio = 0;
	unsigned long reg;
#ifdef CONFIG_NKERNEL
	unsigned long flags;
#endif

	if (!rate)
		return -EINVAL;

	if (rate > CLK_CAM_CLK_MAX_FREQ)
		rate = CLK_CAM_CLK_MAX_FREQ;

	/* compute diviser ratio */
	ratio = (clk->parent->rate/rate) - 1;
	if(ratio < 1)
		ratio = 1;
	if(ratio > 63)
		ratio = 63;

	clk->rate = clk->parent->rate / (ratio + 1);

	/* for voltage working point only */
	pnx67xx_set_freq (CAM_CLK, clk->rate/1000 , clk->rate/1000 );
	
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	reg = inl(CGU_CAMCON_REG);

	reg &= ~(0x3F << 0);
	reg |= (ratio << 0);

	outl(reg, CGU_CAMCON_REG);

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	return ret;
}

static int pnx_clk_set_rate_rtke(struct clk *clk, unsigned long rate)
{
	unsigned long freq;

	/* call cpu freq function */
	/* RTKE has got a highier priority than linux and will handle the */
	/* new frequency before coming back to linux... */
	pnx67xx_set_freq (clk->rate_offset, rate/1000 , rate/1000 );

	/* Get the real frequency */
	freq = pnx67xx_get_freq(clk->rate_offset);
	if (freq != (-1))
		clk->rate = freq*1000;
		
	return freq;
}

/**
 * set parent clock function.
 **/
static int pnx_clk_set_parent_camout(struct clk *clk, struct clk *parent)
{
	int ret = 0;
	unsigned long srce = 0;
	unsigned long reg;
#ifdef CONFIG_NKERNEL
	unsigned long flags;
#endif

	if (!strcmp(parent->name, "TVOPLL"))
		srce = 0;
	else if (!strcmp(parent->name, "fix_ck"))
		srce = 1;
	else
		return -EINVAL;

	pnx_clk_enable(&fix_ck);
	pnx_clk_enable(&plltv_ck);
	
#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_save ( flags );
#endif /* CONFIG_NKERNEL */

	reg = inl(CGU_CAMCON_REG);

	reg &= ~(0x1 << 6);
	reg |= (srce << 6);

	outl(reg, CGU_CAMCON_REG);

#ifdef CONFIG_NKERNEL
    hw_raw_local_irq_restore ( flags );
#endif /* CONFIG_NKERNEL */

	udelay(1);
	
	pnx_clk_disable(&fix_ck);
	pnx_clk_disable(&plltv_ck);
	
	clk->parent = parent;

	return ret;
}

static int pnx_clk_set_parent_uart(struct clk *clk, struct clk *parent)
{

	if (!strcmp(parent->name, "pclk2_ck")) {
		clk->parent = parent;
		return 0;
	} else if (!strcmp(parent->name, "clk26m_ck")) {
		clk->parent = parent;
		return 0;
	} else if (!strcmp(parent->name, "clk13m_ck")) {
		clk->parent = parent;
		return 0;
	} else {
		return -1;
	}
}

/**
 * HW init clock function.
 **/

static void pnx_clk_dflt_init(struct clk *clk)
{

	/*set default rate to parent one */
	clk->rate = clk->parent->rate;

}

/* to be notified of rtke freq update */
int pnx_clk_update_rate(struct clk *clk, unsigned long rate)
{
	unsigned long flags;
	int ret = 0;

	if (clk == ERR_PTR(-ENOENT))
		return -EINVAL;

	/* notification pre change */
	if (clk->notifier)
		ret = clk_notify_transition(clk, CLK_RATE_PRECHANGE,
					    (void *)rate);

	if (!(ret & NOTIFY_STOP_MASK)) {
		spin_lock_irqsave(&clockfw_lock, flags);
		if (clk->recalc)
			clk->recalc(clk);
	
		spin_unlock_irqrestore(&clockfw_lock, flags);

		/* notification post change */
		if (clk->notifier)
			clk_notify_transition(clk, CLK_RATE_POSTCHANGE,
					      (void *)clk->rate);
	}
	return ret;
}

static int
cgu_rtke_freq_notifier_handler(struct notifier_block *nb,
			       unsigned long type, void *data)
{
	unsigned long freq;

	debug("rtke freq change notification\n");

	freq = pnx67xx_get_freq(arm_ck.rate_offset);
	if (freq != (-1))
		arm_ck.rate = freq * 1000;
	debug("arm : %lu \n", freq);
	freq = pnx67xx_get_freq(hclk_ck.rate_offset);
	if (freq != (-1))
		hclk_ck.rate = freq * 1000;
	freq = pnx67xx_get_freq(sdm_ck.rate_offset);
	if (freq != (-1))
		sdm_ck.rate = freq * 1000;
	freq = pnx67xx_get_freq(hclk2_ck.rate_offset);
	/* we have a notification for hclk2 */
	if ((freq != (-1)) && (freq * 1000 != hclk2_ck.rate))
		pnx_clk_update_rate(&hclk2_ck, freq * 1000);
	debug("hclk2 : %lu \n", freq);
	freq = pnx67xx_get_freq(pclk2_ck.rate_offset);
	if (freq != (-1))
		pclk2_ck.rate = freq * 1000;

	return NOTIFY_OK;
}

struct notifier_block cgu_rtke_freq_notifier_block = {
	.notifier_call = cgu_rtke_freq_notifier_handler,
};

/* to notify driver of hclk2 freq change */
static struct srcu_notifier_head clk_hclk2_transition_notifier_list;

static void pnx_clk_init_hclk2(struct clk *clk)
{

	pnx_clk_set_rate_rtke(clk, clk->rate);

	srcu_init_notifier_head(&clk_hclk2_transition_notifier_list);
	clk->notifier = &clk_hclk2_transition_notifier_list;

}

static void pnx_clk_init_rtke(struct clk *clk)
{
	pnx_clk_set_rate_rtke(clk, clk->rate);
}

static void pnx_clk_init_fci(struct clk *clk)
{
	pnx_clk_set_rate_fci(clk, clk->rate);
}

static void pnx_clk_init_camout(struct clk *clk)
{
	pnx_clk_set_parent_camout(clk, &fix_ck);
	pnx_clk_set_rate_camout(clk, clk->rate);
}

/*
 * Update clock rate
 */
static void followparent_ivs_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
	propagate_rate(clk);
}

/*
 * Update clock rate
 */
static void pnx_clk_rtke_recalc(struct clk *clk)
{
	unsigned long freq;

	freq = pnx67xx_get_freq(clk->rate_offset);
	if (freq != (-1))
		clk->rate = freq*1000;		

	propagate_rate(clk);
}

/*-------------------------------------------------------------------------
 * PNX platform access clock functions
 *-------------------------------------------------------------------------*/

/**
 * @brief Disable a device clock.
 * @param clk clock description structure
 */
static void pnx_clk_disable ( struct clk * clk )
{
	/*    BUG_ON(!clk->ref_count); */
	if((clk->flags & ALWAYS_ENABLED) == ALWAYS_ENABLED)
		return;

	if (clk->usecount > 0 && !(--clk->usecount)) {
		if (clk->disable)
			clk->disable(clk);
		else
			pnx67xx_cgu_disable_clock (clk);

		if (likely((u32)clk->parent))
			pnx_clk_disable(clk->parent);
	}
}

/**
 * @brief Enable a device clock.
 * @param clk clock description structure
 */
static int pnx_clk_enable ( struct clk * clk )
{
	int ret = 0;

	/*   BUG_ON(!clk->ref_count); */
	if((clk->flags & ALWAYS_ENABLED) == ALWAYS_ENABLED)
		return 0;

	if (clk->usecount++ == 0) {
		if (likely((u32)clk->parent))
			ret = pnx_clk_enable(clk->parent);

		if (unlikely(ret != 0)) {
			clk->usecount--;
			return ret;
		}

		if (clk->enable)
			ret = clk->enable(clk);
		else
			ret = pnx67xx_cgu_enable_clock (clk);

		if (unlikely(ret != 0) && clk->parent) {
			pnx_clk_disable(clk->parent);
			clk->usecount--;
		}
	}
    return 0;
}

static long pnx_clk_round_rate(struct clk *clk, unsigned long rate)
{
	if (clk->round_rate)
		return clk->round_rate(clk, rate);
	else 
		return rate;
}

static int pnx_clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = 0;

	/*   BUG_ON(!clk->ref_count); */
	if (clk->usecount > 0) {
		if (clk->disable)
			clk->disable(clk);
		else
			pnx67xx_cgu_disable_clock(clk);
	}

	if (clk->set_rate)
		ret = clk->set_rate(clk, rate);
	else
		ret = -EINVAL;

	if (clk->usecount > 0) {
		if (clk->enable)
			clk->enable(clk);
		else
			pnx67xx_cgu_enable_clock(clk);
	}

	/* propagate rate */
	propagate_rate(clk);

	return ret;
}

static int pnx_clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;
	int old_rate;

	if (clk->usecount > 0) {
		if (clk->disable)
			clk->disable(clk);
		else
			pnx67xx_cgu_disable_clock(clk);

		if (likely((u32)clk->parent))
			pnx_clk_disable(clk->parent);
	}

	old_rate = clk->rate;

	if (clk->set_parent)
		ret = clk->set_parent(clk, parent);
	else
		ret = -EINVAL;

	if (clk->round_rate)
		old_rate = clk->round_rate(clk, old_rate);

	pnx_clk_set_rate(clk, old_rate);

	if (clk->usecount > 0) {
		if (likely((u32)clk->parent))
			pnx_clk_enable(clk->parent);

		if (clk->enable)
			ret = clk->enable(clk);
		else
			ret = pnx67xx_cgu_enable_clock(clk);
	}

	/* propagate rate */
	if (clk->flags & RATE_PROPAGATES)
		propagate_rate(clk);

	return ret;
}

static struct clk * pnx_clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

/*-------------------------------------------------------------------------
 * PNX platform clock constraint functions
 *-------------------------------------------------------------------------*/

#ifdef CONFIG_NKERNEL
unsigned long clk_get_root_constraint(void)
{
	return cgu_ctxt.root.global;
}

ssize_t pnx_clk_show_root_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;

	size += sprintf(&buf[size], "--Root-- : \n");

	if (cgu_ctxt.root.details.fix_clk)
		size += sprintf(&buf[size], "fix_clk requested, ");
	if (cgu_ctxt.root.details.hclk)
		size += sprintf(&buf[size], "hclk requested, ");
	if (cgu_ctxt.root.details.hclk2)
		size += sprintf(&buf[size], "hclk2 requested, ");
	if (cgu_ctxt.root.details.sdmclk)
		size += sprintf(&buf[size], "sdmclk requested, ");
	if (cgu_ctxt.root.details.pclk2)
		size += sprintf(&buf[size], "pclk2 requested, ");
	if (cgu_ctxt.root.details.clk24m)
		size += sprintf(&buf[size], "clk24m requested, ");
	if (cgu_ctxt.root.details.clk26m)
		size += sprintf(&buf[size], "clk26m requested, ");
	if (cgu_ctxt.root.details.clk13m)
		size += sprintf(&buf[size], "clk13m requested, ");
	if (cgu_ctxt.root.details.dsp2_pll_clk)
		size += sprintf(&buf[size], "dsp2_pll_clk requested, ");

	size += sprintf(&buf[size], "\n");

	return size;
}

ssize_t pnx_clk_show_shared_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;

	size += sprintf(&buf[size], "--Shared-- : \n");

	if (cgu_ctxt.base_adr->kernel.dma)
		size += sprintf(&buf[size], "kernel dma requested, ");
	if (cgu_ctxt.base_adr->rtke.dma)
		size += sprintf(&buf[size], "rtke dma requested, ");
	if (cgu_ctxt.base_adr->kernel.mtu)
		size += sprintf(&buf[size], "kernel mtu requested, ");
	if (cgu_ctxt.base_adr->rtke.mtu)
		size += sprintf(&buf[size], "rtke mtu requested, ");

	size += sprintf(&buf[size], "\n");

	return size;
}
#endif

unsigned long clk_get_hw_constraint(unsigned long reg_addr)
{
    unsigned long reg;

	reg = inl(reg_addr);

	return reg;
}

ssize_t pnx_clk_show_hw_rates(struct kobject *kobj, char *buf)
{
	int size = 0;
	unsigned long reg, rate, msc, psc, mclk;

	size += sprintf(&buf[size], "--HW-- : \n");

	mclk = clk_get_hw_constraint(CGU_FIXCON_REG);
	mclk = osc_ck.rate / 1000000 / ( ((mclk & (0x3 << 17)) >> 17) +1 );

	reg = clk_get_hw_constraint(CGU_SCCON_REG);

	msc = ( ((reg & (0xF << 25)) >> 25) + 1 );
    psc = ( ((reg & (0x3 << 15)) >> 15) + 1 );
	size += sprintf(&buf[size], "arm_ck is ");
	if (reg & (1 << 17)) {
		if (reg & (1 << 18))
			rate = 312;
		else
			rate = 156;
		rate = rate/msc/psc;
		size += sprintf(&buf[size], "%lu", rate);
		size += sprintf(&buf[size], "Mhz fix_ck pll\n");
	} else {
		rate = mclk * (((reg & (0xFF << 3)) >> 3) + 1)
		    / (((reg & (0xF << 11)) >> 11) + 1);
		rate = rate/msc/psc;
		size += sprintf(&buf[size], "%lu", rate);
		size += sprintf(&buf[size], "Mhz sc_ck pll\n");
	}

	rate = rate / ( ((reg & (0xF << 21)) >> 21) +1 );

	size += sprintf(&buf[size], "hclk_ck is %luMhz\n", rate);

	reg = clk_get_hw_constraint(CGU_FIXCON_REG);

	rate = 13 * ( ((reg & (0x3 << 20)) >> 20) +1 );

	size += sprintf(&buf[size], "pclk2_ck is %luMhz\n", rate);

	if (reg & (1 << 31)) {
        if (reg & (1<<29))
            rate = 104;
        else
            rate = 78;
	} else
        rate = 13 + (13 * ((reg & (0x7 << 29)) >> 29));

	size += sprintf(&buf[size], "hclk2_ck is %luMhz\n", rate);

	reg = clk_get_hw_constraint(CGU_SDMCON_REG);

	msc = ( ((reg & (0x7 << 0)) >> 0) + 1 );

	size += sprintf(&buf[size], "sdm_ck is ");
	if (reg & (1 << 17)) {
		if (reg & (1 << 18))
			rate = 312;
		else
			rate = 156;
		size += sprintf(&buf[size], "%lu", rate/msc);
		size += sprintf(&buf[size], "Mhz fix_ck pll\n");
	} else {
		rate = mclk * 2 * (((reg & (0xFF << 3)) >> 3) + 1)
		    / (((reg & (0x3F << 11)) >> 11) + 1);
		size += sprintf(&buf[size], "%lu", rate/msc);
		size += sprintf(&buf[size], "Mhz sdm pll\n");
	}

	if (reg & (1 << 23))
		size += sprintf(&buf[size], "ivs-sdm concentrator is enable\n");
	else
		size += sprintf(&buf[size],
				"ivs-sdm concentrator is disable\n");

	return size;
}

unsigned char *cgu_name_sc1[32] = {
  "EBI","01","NFI","SDI","04","MSI","UCC","JDI",
  "08","09","10","DMAU","RFSM1","IIS","USBD","FCI",
  "USIM","GEAC","PWM3","PWM2","PWM1","KBS","GPIO","UART2",
  "UART1","IIC2","IIC1","SPI2","SPI1","SCTU","EXTINT","INTC"
};

unsigned char * cgu_name_sc2[32]={
"32","33","34","35","36","37","38","39",
"40","41","42","43","44","45","46","47",
"48","49","50","51","52","53","BBIP","55",
	"56", "57", "58", "ETB", "60", "MTU", "62", "CAE"
};

ssize_t pnx_clk_show_sc_constraints(struct kobject *kobj, char *buf)
{
	int i, size = 0;
	unsigned long reg;

	size += sprintf(&buf[size], "--SC-- : \n");

	reg = clk_get_hw_constraint(CGU_GATESC1_REG);

	for (i = 0; i < 32; i++) {
		if ((reg >> i) & 0x1)
			size +=
			    sprintf(&buf[size], "%s ,", cgu_name_sc1[31 - i]);
	}

	size += sprintf(&buf[size], "\n");

	reg = clk_get_hw_constraint(CGU_GATESC2_REG);

	for (i = 0; i < 32; i++) {
		if ((reg >> i) & 0x1)
			size +=
			    sprintf(&buf[size], "%s ,", cgu_name_sc2[31 - i]);
	}

	size += sprintf(&buf[size], "\n");

	return size;
}

unsigned char * cgu_name_ivs[32]={
"","","","","","","","",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"CAMJPE", "TVO", "JDU", "VDC", "VEC", "IPP", "VDE", "CAM"
};

ssize_t pnx_clk_show_ivs_constraints(struct kobject *kobj, char *buf)
{
	int i, size = 0;
	unsigned long reg;

	size += sprintf(&buf[size], "--IVS-- : \n");

	reg = clk_get_hw_constraint(CGU_GATEIVS_REG);

	for (i = 0; i < 32; i++) {
		if ((reg >> i) & 0x1)
			size +=
			    sprintf(&buf[size], "%s ,", cgu_name_ivs[31 - i]);
	}

	size += sprintf(&buf[size], "\n");

	return size;
}

ssize_t pnx_clk_show_tvo_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;
	unsigned long reg;

	size += sprintf(&buf[size], "--TVO-- : \n");

	reg = clk_get_hw_constraint(CGU_TVCON_REG);

	if ((reg >> 15) & 0x1)
		size += sprintf(&buf[size], "tvclk_ck pll enable\n");
	else
		size += sprintf(&buf[size], "tvclk_ck pll disable\n");

	if ((reg >> 12) & 0x1)
		size += sprintf(&buf[size], "TVOPLL enable\n");
	else
		size += sprintf(&buf[size], "TVOPLL disable\n");

	return size;
}

ssize_t pnx_clk_show_cam_constraints(struct kobject *kobj, char *buf)
{
	int size = 0;
	unsigned long reg, rate, div;

	size += sprintf(&buf[size], "--CAM-- : \n");

	reg = clk_get_hw_constraint(CGU_CAMCON_REG);

	if ((reg >> 6) & 0x1) {
		size += sprintf(&buf[size], "fix_clock is source\n");
		rate = 312;
	} else {
		size += sprintf(&buf[size], "TVOPLL is source\n");
		rate = 216;
	}

	div = ((reg & (0x3F << 0)) >> 0);

	size += sprintf(&buf[size], "camo_ck is %luMhz\n", rate/(div + 1));

	if ((reg >> 7) & 0x1)
		size += sprintf(&buf[size], "camo_ck enable\n");
	else
		size += sprintf(&buf[size], "camo_ck disable\n");

	return size;
}

/*-------------------------------------------------------------------------
 * PNX clock reset and init functions
 *-------------------------------------------------------------------------*/

static struct clk_functions pnx_clk_functions = {
	.clk_enable			= pnx_clk_enable,
	.clk_disable		= pnx_clk_disable,
	.clk_round_rate		= pnx_clk_round_rate,
	.clk_set_rate		= pnx_clk_set_rate,
	.clk_set_parent		= pnx_clk_set_parent,
	.clk_get_parent		= pnx_clk_get_parent,
};

/* TBC with PLA if this flag is usefull */
/**
 * @brief Attempts to connect the primary description structure for DMA case.
 *
 * We don't do any initialization since we expect the primary OS to have done
 * it for us.
 */
static int __init pnx67xx_cgu_init ( void )
{
	struct clk ** clkp;
	printk(PKMOD "clk management init\n");

#ifdef CONFIG_NKERNEL
	/* Initialiase shared object */
	cgu_ctxt.shared = xos_area_connect(CLOCKMNGT_AREA_NAME,
					   sizeof(struct clockmngt_shared));
	if (cgu_ctxt.shared) {
		cgu_ctxt.base_adr = xos_area_ptr(cgu_ctxt.shared);

		/* clear shared object */
		cgu_ctxt.base_adr->kernel.dma = 0;
		cgu_ctxt.base_adr->kernel.mtu = 1;

	} else {
		printk(PKMOD "failed to connect to xos area\n");

		tmp_clkmngt_shared.rtke.dma = 1;
		tmp_clkmngt_shared.rtke.mtu = 1;
		cgu_ctxt.base_adr = &tmp_clkmngt_shared;
	}

	cgu_ctxt.root.global = 0;
#endif

	if (mpurate)
		sc_ck.rate = mpurate;

	/* init clock function pointer table */
	clk_init(&pnx_clk_functions);

	/* register and init clock elements */
	for (clkp = onchip_clks; clkp < onchip_clks + ARRAY_SIZE(onchip_clks);
	     clkp++) {
		clk_register(*clkp);
	}

	/* to be notified from rtke_freq change */
	/* to be called one time */
	pnx67xx_register_freq_notifier(&cgu_rtke_freq_notifier_block);

	return 0;
   }

arch_initcall ( pnx67xx_cgu_init );
