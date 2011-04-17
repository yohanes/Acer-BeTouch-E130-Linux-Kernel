/*
 *  linux/arch/arm/plat-pnx/dma.c
 *
 *  PNXxxxx DMA registration and IRQ dispatching
 *
 *  Author:	Vitaly Wool
 *  Copyright:	MontaVista Software Inc. (c) 2005
 *
 *  Based on the code from Nicolas Pitre
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>

#include <asm/system.h>
#include <mach/hardware.h>
//#include <asm/dma.h>
#include <asm/dma-mapping.h>
#include <asm/io.h>
//#include <asm/mach/dma.h>
//#include <asm/arch/clock.h>
#include <mach/dma.h>

#define DMAC_INT_STAT		(DMAC_BASE + 0x0000)
#define DMAC_INT_TC_STAT	(DMAC_BASE + 0x0004)
#define DMAC_INT_TC_CLEAR	(DMAC_BASE + 0x0008)
#define DMAC_INT_ERR_STAT	(DMAC_BASE + 0x000c)
#define DMAC_INT_ERR_CLEAR	(DMAC_BASE + 0x0010)
#define DMAC_SOFT_SREQ		(DMAC_BASE + 0x0024)
#define DMAC_CONFIG		(DMAC_BASE + 0x0030)
#define DMAC_Cx_SRC_ADDR(c)	(DMAC_BASE + 0x0100 + (c) * 0x20)
#define DMAC_Cx_DEST_ADDR(c)	(DMAC_BASE + 0x0104 + (c) * 0x20)
#define DMAC_Cx_LLI(c)		(DMAC_BASE + 0x0108 + (c) * 0x20)
#define DMAC_Cx_CONTROL(c)	(DMAC_BASE + 0x010c + (c) * 0x20)
#define DMAC_Cx_CONFIG(c)	(DMAC_BASE + 0x0110 + (c) * 0x20)


#ifdef CONFIG_NKERNEL
#include <osware/osware.h>
/* pointer to shared memory, common with RTK-E */
tNkDMAU * shared_dma = (tNkDMAU *) 0;
#endif

unsigned long DMAC_BASE;
static struct dma_channel {
	char *name;
	void (*irq_handler) (int, int, void *);
	void *data;
	struct pnx_dma_ll *ll;
	u32 ll_dma;
	u32 use_count;
} dma_channels[MAX_DMA_CHANNELS_NBR+1];

static struct clk *dma_clk=0;

static struct ll_pool {
	void *vaddr;
	void *cur;
	dma_addr_t dma_addr;
	int count;
} ll_pool;

static spinlock_t ll_lock = SPIN_LOCK_UNLOCKED;

struct pnx_dma_ll *pnx_alloc_ll_entry(dma_addr_t * ll_dma)
{
	struct pnx_dma_ll *ll = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ll_lock, flags);
	if (ll_pool.count > 4) { /* can give one more */
		ll = *(struct pnx_dma_ll **) ll_pool.cur;
		*ll_dma = ll_pool.dma_addr + ((void *)ll - ll_pool.vaddr);
		*(void **)ll_pool.cur = **(void ***)ll_pool.cur;
		memset(ll, 0, sizeof(*ll));
		ll_pool.count--;
	}
	spin_unlock_irqrestore(&ll_lock, flags);

	return ll;
}

EXPORT_SYMBOL_GPL(pnx_alloc_ll_entry);

void pnx_free_ll_entry(struct pnx_dma_ll * ll, dma_addr_t ll_dma)
{
	unsigned long flags;

	if (ll) {
		if ((unsigned long)((long)ll - (long)ll_pool.vaddr) > 0x4000) {
			printk(KERN_ERR "Trying to free entry not allocated by DMA\n");
			BUG();
		}

		if (ll->flags & DMA_BUFFER_ALLOCATED)
			ll->free(ll->alloc_data);

		spin_lock_irqsave(&ll_lock, flags);
		*(long *)ll = *(long *)ll_pool.cur;
		*(long *)ll_pool.cur = (long)ll;
		ll_pool.count++;
		spin_unlock_irqrestore(&ll_lock, flags);
	}
}

EXPORT_SYMBOL_GPL(pnx_free_ll_entry);

void pnx_free_ll(u32 ll_dma, struct pnx_dma_ll * ll)
{
	struct pnx_dma_ll *ptr;
	u32 dma;

	while (ll) {
		dma = ll->next_dma;
		ptr = ll->next;
		pnx_free_ll_entry(ll, ll_dma);

		ll_dma = dma;
		ll = ptr;
	}
}

EXPORT_SYMBOL_GPL(pnx_free_ll);

static unsigned long dma_channels_clocked = 0;

static inline void dma_increment_usage(int ch)
{
	dma_channels[ch].use_count++;
	if ( !dma_channels_clocked ){
		clk_enable(dma_clk);
#ifdef CONFIG_NKERNEL
#else
		pnx_config_dma(-1, -1, 1);
#endif
	}
	set_bit(ch,&dma_channels_clocked);
}
static inline void dma_decrement_usage(int ch)
{

	if (dma_channels[ch].use_count>0){
		if (!--dma_channels[ch].use_count){
			clear_bit(ch,&dma_channels_clocked);
			if (!dma_channels_clocked) {
				clk_disable(dma_clk);
#ifdef CONFIG_NKERNEL
#else
			pnx_config_dma(-1, -1, 0);
#endif
			}
		}
	}
}

static spinlock_t dma_lock = SPIN_LOCK_UNLOCKED;

static inline void pnx_dma_lock(void)
{
	spin_lock_irq(&dma_lock);
}

static inline void pnx_dma_unlock(void)
{
	spin_unlock_irq(&dma_lock);
}
/* channel from 0 till 5 are used by rtk only */
#define VALID_CHANNEL(c)	((c) >=MIN_DMA_CHANNELS_NBR ) 
int pnx_request_channel(char *name, int ch,
			    void (*irq_handler) (int, int, void *), void *data)
{
	int i, found = 0;

	/* basic sanity checks */
	if (!name || (ch != -1 && !VALID_CHANNEL(ch)))
		return -EINVAL;

	pnx_dma_lock();

	/* try grabbing a DMA channel with the requested priority */
	for (i = MAX_DMA_CHANNELS_NBR ; i >= MIN_DMA_CHANNELS_NBR; i--) {
		if (!dma_channels[i].name && (ch == -1 || ch >= i)) {
			found = 1;
			break;
		}
	}

	if (found) {
		dma_channels[i].name = name;
		dma_channels[i].irq_handler = irq_handler;
		dma_channels[i].data = data;
		dma_channels[i].ll = NULL;
		dma_channels[i].ll_dma = 0;
		dma_channels[i].use_count = 0;
	} else {
		printk(KERN_WARNING "No more available DMA channels for %s\n",
		       name);
		i = -ENODEV;
	}

	pnx_dma_unlock();
	return i;
}

EXPORT_SYMBOL_GPL(pnx_request_channel);

void pnx_free_channel(int ch)
{
	if (!VALID_CHANNEL(ch))
		return;
	
	if (!dma_channels[ch].name) {
		printk(KERN_CRIT
		       "%s: trying to free channel %d which is already freed\n",
		       __FUNCTION__, ch);
		return;
	}

	pnx_dma_lock();
	pnx_free_ll(dma_channels[ch].ll_dma, dma_channels[ch].ll);
	dma_channels[ch].ll = NULL;
	dma_decrement_usage(ch);

	dma_channels[ch].name = NULL;
	pnx_dma_unlock();
}

EXPORT_SYMBOL_GPL(pnx_free_channel);
#ifdef CONFIG_NKERNEL
#else
/* normally rtk has already configured this register */
int pnx_config_dma(int ahb_m1_be, int ahb_m2_be, int enable)
{
	unsigned long dma_cfg = __raw_readl(DMAC_CONFIG);

	switch (ahb_m1_be) {
	case 0:
		dma_cfg &= ~(1 << 1);
		break;
	case 1:
		dma_cfg |= (1 << 1);
		break;
	default:
		break;
	}

	switch (ahb_m2_be) {
	case 0:
		dma_cfg &= ~(1 << 2);
		break;
	case 1:
		dma_cfg |= (1 << 2);
		break;
	default:
		break;
	}

	switch (enable) {
	case 0:
		dma_cfg &= ~(1 << 0);
		break;
	case 1:
		dma_cfg |= (1 << 0);
		break;
	default:
		break;
	}

	pnx_dma_lock();
	__raw_writel(dma_cfg, DMAC_CONFIG);
	pnx_dma_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(pnx_config_dma);
#endif

/**
  * @brief
  *
  * @param ch_ctrl
  * @param ctrl
  *
  * @return
  */
int pnx_dma_pack_control(const struct pnx_dma_ch_ctrl * ch_ctrl,
			     unsigned long *ctrl)
{
	int i = 0, dbsize, sbsize, err = 0;

	if (!ctrl || !ch_ctrl) {
		err = -EINVAL;
		goto out;
	}

	*ctrl = 0;

	switch (ch_ctrl->tc_mask) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 31);
		break;

	default:
		err = -EINVAL;
		goto out;
	}

	switch (ch_ctrl->cacheable) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 30);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->bufferable) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 29);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->priv_mode) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 28);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->di) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 27);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->si) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 26);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->dest_ahb1) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 25);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->src_ahb1) {
	case 0:
		break;
	case 1:
		*ctrl |= (1 << 24);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->dwidth) {
	case WIDTH_BYTE:
		*ctrl &= ~(7 << 21);
		break;
	case WIDTH_HWORD:
		*ctrl &= ~(7 << 21);
		*ctrl |= (1 << 21);
		break;
	case WIDTH_WORD:
		*ctrl &= ~(7 << 21);
		*ctrl |= (2 << 21);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_ctrl->swidth) {
	case WIDTH_BYTE:
		*ctrl &= ~(7 << 18);
		break;
	case WIDTH_HWORD:
		*ctrl &= ~(7 << 18);
		*ctrl |= (1 << 18);
		break;
	case WIDTH_WORD:
		*ctrl &= ~(7 << 18);
		*ctrl |= (2 << 18);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	dbsize = ch_ctrl->dbsize;
	while (!(dbsize & 1)) {
		i++;
		dbsize >>= 1;
	}
	if (dbsize != 1 || i > 8 || i == 1) {
		err = -EINVAL;
		goto out;
	} else if (i > 1)
		i--;
	*ctrl &= ~(7 << 15);
	*ctrl |= (i << 15);

	sbsize = ch_ctrl->sbsize;
	i=0;
	while (!(sbsize & 1)) {
		i++;
		sbsize >>= 1;
	}
	if (sbsize != 1 || i > 8 || i == 1) {
		err = -EINVAL;
		goto out;
	} else if (i > 1)
		i--;
	*ctrl &= ~(7 << 12);
	*ctrl |= (i << 12);

	if (ch_ctrl->tr_size > 0x7ff) {
		err = -E2BIG;
		goto out;
	}
	*ctrl &= ~0x7ff;
	*ctrl |= ch_ctrl->tr_size & 0x7ff;

out:
	return err;
}

EXPORT_SYMBOL_GPL(pnx_dma_pack_control);

int pnx_dma_parse_control(unsigned long ctrl,
			      struct pnx_dma_ch_ctrl * ch_ctrl)
{
	int err = 0;

	if (!ch_ctrl) {
		err = -EINVAL;
		goto out;
	}
	ch_ctrl->tr_size = ctrl & 0x7ff;
	ctrl >>= 12;

	ch_ctrl->sbsize = 1 << (ctrl & 7);
	if (ch_ctrl->sbsize > 1)
		ch_ctrl->sbsize <<= 1;
	ctrl >>= 3;

	ch_ctrl->dbsize = 1 << (ctrl & 7);
	if (ch_ctrl->dbsize > 1)
		ch_ctrl->dbsize <<= 1;
	ctrl >>= 3;

	switch (ctrl & 7) {
	case 0:
		ch_ctrl->swidth = WIDTH_BYTE;
		break;
	case 1:
		ch_ctrl->swidth = WIDTH_HWORD;
		break;
	case 2:
		ch_ctrl->swidth = WIDTH_WORD;
		break;
	default:
		err = -EINVAL;
		goto out;
	}
	ctrl >>= 3;

	switch (ctrl & 7) {
	case 0:
		ch_ctrl->dwidth = WIDTH_BYTE;
		break;
	case 1:
		ch_ctrl->dwidth = WIDTH_HWORD;
		break;
	case 2:
		ch_ctrl->dwidth = WIDTH_WORD;
		break;
	default:
		err = -EINVAL;
		goto out;
	}
	ctrl >>= 3;

	ch_ctrl->src_ahb1 = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->dest_ahb1 = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->si = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->di = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->priv_mode = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->bufferable = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->cacheable = ctrl & 1;
	ctrl >>= 1;

	ch_ctrl->tc_mask = ctrl & 1;

out:
	return err;
}

EXPORT_SYMBOL_GPL(pnx_dma_parse_control);

/**
  * @brief build the channel config register according to the channel config
  *
  * @param ch_cfg input
  * @param cfg output
  *
  * @return
  */
int pnx_dma_pack_config(const struct pnx_dma_ch_config * ch_cfg,
			    unsigned long *cfg)
{
	int err = 0;

	if (!cfg || !ch_cfg) {
		err = -EINVAL;
		goto out;
	}

	*cfg = 0;

	switch (ch_cfg->halt) {
	case 0:
		break;
	case 1:
		*cfg |= (1 << 18);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_cfg->active) {
	case 0:
		break;
	case 1:
		*cfg |= (1 << 17);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_cfg->lock) {
	case 0:
		break;
	case 1:
		*cfg |= (1 << 16);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_cfg->itc) {
	case 0:
		break;
	case 1:
		*cfg |= (1 << 15);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_cfg->ie) {
	case 0:
		break;
	case 1:
		*cfg |= (1 << 14);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	switch (ch_cfg->flow_cntrl) {
	case FC_MEM2MEM_DMA:
		*cfg &= ~(7 << 11);
		break;
	case FC_MEM2PER_DMA:
		*cfg &= ~(7 << 11);
		*cfg |= (1 << 11);
		break;
	case FC_PER2MEM_DMA:
		*cfg &= ~(7 << 11);
		*cfg |= (2 << 11);
		break;
	case FC_PER2PER_DMA:
		*cfg &= ~(7 << 11);
		*cfg |= (3 << 11);
		break;
	case FC_PER2PER_DPER:
		*cfg &= ~(7 << 11);
		*cfg |= (4 << 11);
		break;
	case FC_MEM2PER_PER:
		*cfg &= ~(7 << 11);
		*cfg |= (5 << 11);
		break;
	case FC_PER2MEM_PER:
		*cfg &= ~(7 << 11);
		*cfg |= (6 << 11);
		break;
	case FC_PER2PER_SPER:
		*cfg |= (7 << 11);
		break;

	default:
		err = -EINVAL;
		goto out;
	}
	*cfg &= ~(0x1f << 6);
	*cfg |= ((ch_cfg->dest_per & 0x1f) << 6);

	*cfg &= ~(0x1f << 1);
	*cfg |= ((ch_cfg->src_per & 0x1f) << 1);

out:
	return err;
}

EXPORT_SYMBOL_GPL(pnx_dma_pack_config);

/**
  * @brief build the channel configuration structure according to the config register value
  *
  * @param cfg input
  * @param ch_cfg output
  *
  * @return
  */
int pnx_dma_parse_config(unsigned long cfg,
			     struct pnx_dma_ch_config * ch_cfg)
{
	int err = 0;

	if (!ch_cfg) {
		err = -EINVAL;
		goto out;
	}

	cfg >>= 1;

	ch_cfg->src_per = cfg & 0x1f;
	cfg >>= 5;

	ch_cfg->dest_per = cfg & 0x1f;
	cfg >>= 5;

	switch (cfg & 7) {
	case 0:
		ch_cfg->flow_cntrl = FC_MEM2MEM_DMA;
		break;
	case 1:
		ch_cfg->flow_cntrl = FC_MEM2PER_DMA;
		break;
	case 2:
		ch_cfg->flow_cntrl = FC_PER2MEM_DMA;
		break;
	case 3:
		ch_cfg->flow_cntrl = FC_PER2PER_DMA;
		break;
	case 4:
		ch_cfg->flow_cntrl = FC_PER2PER_DPER;
		break;
	case 5:
		ch_cfg->flow_cntrl = FC_MEM2PER_PER;
		break;
	case 6:
		ch_cfg->flow_cntrl = FC_PER2MEM_PER;
		break;
	case 7:
		ch_cfg->flow_cntrl = FC_PER2PER_SPER;
	}
	cfg >>= 3;

	ch_cfg->ie = cfg & 1;
	cfg >>= 1;

	ch_cfg->itc = cfg & 1;
	cfg >>= 1;

	ch_cfg->lock = cfg & 1;
	cfg >>= 1;

	ch_cfg->active = cfg & 1;
	cfg >>= 1;

	ch_cfg->halt = cfg & 1;

out:
	return err;
}

EXPORT_SYMBOL_GPL(pnx_dma_parse_config);

void pnx_dma_split_head_entry(struct pnx_dma_config * config,
				  struct pnx_dma_ch_ctrl * ctrl)
{
	int new_len = ctrl->tr_size, num_entries = 0;
	int old_len = new_len;
	int src_width, dest_width, count = 1;
	switch (ctrl->swidth) {
	case WIDTH_BYTE:
		src_width = 1;
		break;
	case WIDTH_HWORD:
		src_width = 2;
		break;
	case WIDTH_WORD:
		src_width = 4;
		break;
	default:
		return;
	}

	switch (ctrl->dwidth) {
	case WIDTH_BYTE:
		dest_width = 1;
		break;
	case WIDTH_HWORD:
		dest_width = 2;
		break;
	case WIDTH_WORD:
		dest_width = 4;
		break;
	default:
		return;
	}

	while (new_len > 0x7FF) {
		num_entries++;
		new_len = (ctrl->tr_size + num_entries) / (num_entries + 1);
	}
	if (num_entries != 0) {
		struct pnx_dma_ll *ll = NULL;
		config->ch_ctrl &= ~0x7ff;
		config->ch_ctrl |= new_len;
		if (!config->is_ll) {
			config->is_ll = 1;
			while (num_entries) {
				if (!ll) {
					config->ll =
					    pnx_alloc_ll_entry(&config->
								   ll_dma);
					ll = config->ll;
				} else {
					ll->next =
					    pnx_alloc_ll_entry(&ll->
								   next_dma);
					ll = ll->next;
				}

				if (ctrl->si)
					ll->src_addr =
					    config->src_addr +
					    src_width * new_len * count;
				else
					ll->src_addr = config->src_addr;
				if (ctrl->di)
					ll->dest_addr =
					    config->dest_addr +
					    dest_width * new_len * count;
				else
					ll->dest_addr = config->dest_addr;
				ll->ch_ctrl = config->ch_ctrl & 0x7fffffff;
				ll->next_dma = 0;
				ll->next = NULL;
				num_entries--;
				count++;
			}
		} else {
			struct pnx_dma_ll *ll_old = config->ll;
			unsigned long ll_dma_old = config->ll_dma;
			while (num_entries) {
				if (!ll) {
					config->ll =
					    pnx_alloc_ll_entry(&config->
								   ll_dma);
					ll = config->ll;
				} else {
					ll->next =
					    pnx_alloc_ll_entry(&ll->
								   next_dma);
					ll = ll->next;
				}

				if (ctrl->si)
					ll->src_addr =
					    config->src_addr +
					    src_width * new_len * count;
				else
					ll->src_addr = config->src_addr;
				if (ctrl->di)
					ll->dest_addr =
					    config->dest_addr +
					    dest_width * new_len * count;
				else
					ll->dest_addr = config->dest_addr;
				ll->ch_ctrl = config->ch_ctrl & 0x7fffffff;
				ll->next_dma = 0;
				ll->next = NULL;
				num_entries--;
				count++;
			}
			ll->next_dma = ll_dma_old;
			ll->next = ll_old;
		}
		/* adjust last length/tc */
		ll->ch_ctrl = config->ch_ctrl & (~0x7ff);
		ll->ch_ctrl |= old_len - new_len * (count - 1);
		config->ch_ctrl &= 0x7fffffff;
	}
}

EXPORT_SYMBOL_GPL(pnx_dma_split_head_entry);

void pnx_dma_split_ll_entry(struct pnx_dma_ll * cur_ll,
				struct pnx_dma_ch_ctrl * ctrl)
{
	int new_len = ctrl->tr_size, num_entries = 0;
	int old_len = new_len;
	int src_width, dest_width, count = 1;

	switch (ctrl->swidth) {
	case WIDTH_BYTE:
		src_width = 1;
		break;
	case WIDTH_HWORD:
		src_width = 2;
		break;
	case WIDTH_WORD:
		src_width = 4;
		break;
	default:
		return;
	}

	switch (ctrl->dwidth) {
	case WIDTH_BYTE:
		dest_width = 1;
		break;
	case WIDTH_HWORD:
		dest_width = 2;
		break;
	case WIDTH_WORD:
		dest_width = 4;
		break;
	default:
		return;
	}

	while (new_len > 0x7FF) {
		num_entries++;
		new_len = (ctrl->tr_size + num_entries) / (num_entries + 1);
	}
	if (num_entries != 0) {
		struct pnx_dma_ll *ll = NULL;
		cur_ll->ch_ctrl &= ~0x7ff;
		cur_ll->ch_ctrl |= new_len;
		if (!cur_ll->next) {
			while (num_entries) {
				if (!ll) {
					cur_ll->next =
					    pnx_alloc_ll_entry(&cur_ll->
								   next_dma);
					ll = cur_ll->next;
				} else {
					ll->next =
					    pnx_alloc_ll_entry(&ll->
								   next_dma);
					ll = ll->next;
				}

				if (ctrl->si)
					ll->src_addr =
					    cur_ll->src_addr +
					    src_width * new_len * count;
				else
					ll->src_addr = cur_ll->src_addr;
				if (ctrl->di)
					ll->dest_addr =
					    cur_ll->dest_addr +
					    dest_width * new_len * count;
				else
					ll->dest_addr = cur_ll->dest_addr;
				ll->ch_ctrl = cur_ll->ch_ctrl & 0x7fffffff;
				ll->next_dma = 0;
				ll->next = NULL;
				num_entries--;
				count++;
			}
		} else {
			struct pnx_dma_ll *ll_old = cur_ll->next;
			unsigned long ll_dma_old = cur_ll->next_dma;
			while (num_entries) {
				if (!ll) {
					cur_ll->next =
					    pnx_alloc_ll_entry(&cur_ll->
								   next_dma);
					ll = cur_ll->next;
				} else {
					ll->next =
					    pnx_alloc_ll_entry(&ll->
								   next_dma);
					ll = ll->next;
				}

				if (ctrl->si)
					ll->src_addr =
					    cur_ll->src_addr +
					    src_width * new_len * count;
				else
					ll->src_addr = cur_ll->src_addr;
				if (ctrl->di)
					ll->dest_addr =
					    cur_ll->dest_addr +
					    dest_width * new_len * count;
				else
					ll->dest_addr = cur_ll->dest_addr;
				ll->ch_ctrl = cur_ll->ch_ctrl & 0x7fffffff;
				ll->next_dma = 0;
				ll->next = NULL;
				num_entries--;
				count++;
			}

			ll->next_dma = ll_dma_old;
			ll->next = ll_old;
		}
		/* adjust last length/tc */
		ll->ch_ctrl = cur_ll->ch_ctrl & (~0x7ff);
		ll->ch_ctrl |= old_len - new_len * (count - 1);
		cur_ll->ch_ctrl &= 0x7fffffff;
	}
}

EXPORT_SYMBOL_GPL(pnx_dma_split_ll_entry);

/**
  * @brief programm the dma channel ch with the config passed
  * in parameter
  *
  * @param ch
  * @param config
  *
  * @return
  */
int pnx_config_channel(int ch, struct pnx_dma_config * config)
{
	unsigned long i_bit= 1<< ch;
	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name)
		return -EINVAL;

	pnx_dma_lock();
	/* activate clock gating */
    dma_increment_usage(ch);
	/* clear previous treatment */
	__raw_writel(i_bit, DMAC_INT_TC_CLEAR);
	__raw_writel(i_bit, DMAC_INT_ERR_CLEAR);

	__raw_writel(config->src_addr, DMAC_Cx_SRC_ADDR(ch));
	__raw_writel(config->dest_addr, DMAC_Cx_DEST_ADDR(ch));

	if (config->is_ll)
		__raw_writel(config->ll_dma, DMAC_Cx_LLI(ch));
	else
		__raw_writel(0, DMAC_Cx_LLI(ch));

	__raw_writel(config->ch_ctrl, DMAC_Cx_CONTROL(ch));
	__raw_writel(config->ch_cfg, DMAC_Cx_CONFIG(ch));
    dma_decrement_usage(ch);
	pnx_dma_unlock();

	return 0;

}

EXPORT_SYMBOL_GPL(pnx_config_channel);


/**
  * @brief retrieve the channel configuration and write it in
  * config pointer
  *
  * @param ch
  * @param config
  *
  * @return
  */
int pnx_channel_get_config(int ch, struct pnx_dma_config * config)
{
	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name || !config)
		return -EINVAL;

	pnx_dma_lock();
    dma_increment_usage(ch);
	config->ch_cfg = __raw_readl(DMAC_Cx_CONFIG(ch));
	config->ch_ctrl = __raw_readl(DMAC_Cx_CONTROL(ch));

	config->ll_dma = __raw_readl(DMAC_Cx_LLI(ch));
	config->is_ll = config->ll_dma ? 1 : 0;

	config->src_addr = __raw_readl(DMAC_Cx_SRC_ADDR(ch));
	config->dest_addr = __raw_readl(DMAC_Cx_DEST_ADDR(ch));
    dma_decrement_usage(ch);
	pnx_dma_unlock();

	return 0;
}

EXPORT_SYMBOL_GPL(pnx_channel_get_config);

int pnx_dma_ch_enable(int ch)
{
	unsigned long ch_cfg;

	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name)
		return -EINVAL;

	pnx_dma_lock();
	/* activate clock and keep it on*/
    dma_increment_usage(ch);
	ch_cfg = __raw_readl(DMAC_Cx_CONFIG(ch));
	ch_cfg |= 1;
	__raw_writel(ch_cfg, DMAC_Cx_CONFIG(ch));
	pnx_dma_unlock();

	return 0;
}

EXPORT_SYMBOL_GPL(pnx_dma_ch_enable);

int pnx_dma_ch_fifo_empty(int ch)
{
	unsigned long ch_cfg, ch_ctrl;
	struct pnx_dma_ch_config config;
	struct pnx_dma_ch_ctrl control;

	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name)
		return -EINVAL;
	pnx_dma_lock();
	dma_increment_usage(ch);
	ch_ctrl = __raw_readl(DMAC_Cx_CONTROL(ch));
	pnx_dma_parse_control(ch_ctrl, &control);
	ch_cfg = __raw_readl(DMAC_Cx_CONFIG(ch));
	pnx_dma_parse_config(ch_cfg,&config);
	dma_decrement_usage(ch);
	pnx_dma_unlock();
	return ((config.active == 0) && (control.tr_size == 0));

}

EXPORT_SYMBOL_GPL(pnx_dma_ch_fifo_empty);

int pnx_dma_ch_disable(int ch)
{
	unsigned long ch_cfg;

	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name)
		return -EINVAL;

	pnx_dma_lock();

	ch_cfg = __raw_readl(DMAC_Cx_CONFIG(ch));
	ch_cfg &= ~1;
	__raw_writel(ch_cfg, DMAC_Cx_CONFIG(ch));
	/* transaction is finshed stop clock */
    dma_decrement_usage(ch);
	pnx_dma_unlock();

	return 0;
}

EXPORT_SYMBOL_GPL(pnx_dma_ch_disable);

int pnx_dma_ch_enabled(int ch)
{
	unsigned long ch_cfg;

	if (!VALID_CHANNEL(ch) || !dma_channels[ch].name)
		return -EINVAL;

	pnx_dma_lock();
	dma_increment_usage(ch);
	ch_cfg = __raw_readl(DMAC_Cx_CONFIG(ch));
	dma_decrement_usage(ch);
	pnx_dma_unlock();

	return ch_cfg & 1;
}

EXPORT_SYMBOL_GPL(pnx_dma_ch_enabled);

static irqreturn_t dma_irq_handler(int irq, void *dev_id)
{
	int i;
	unsigned long i_bit;
#ifdef CONFIG_NKERNEL
	unsigned long dint, tcint,eint;
	unsigned long flags;
    hw_raw_local_irq_save ( flags );
	tcint = shared_dma->ulTCStatus;
    eint  = shared_dma->ulErrorStatus;
    shared_dma->ulTCStatus=0;
    shared_dma->ulErrorStatus=0;
    hw_raw_local_irq_restore ( flags );
	dint = tcint | eint;	
#else
	unsigned long dint = __raw_readl(DMAC_INT_STAT);
	unsigned long tcint = __raw_readl(DMAC_INT_TC_STAT);
	unsigned long eint = __raw_readl(DMAC_INT_ERR_STAT);
#endif

	for (i = MAX_DMA_CHANNELS_NBR ; i >= MIN_DMA_CHANNELS_NBR; i--) {
		i_bit = 1 << i;
		if (dint & i_bit) {
			struct dma_channel *channel = &dma_channels[i];

			if (channel->name && channel->irq_handler) {
				int cause = 0;

				if (eint & i_bit)
					cause |= DMA_ERR_INT;
				if (tcint & i_bit)
					cause |= DMA_TC_INT;
				channel->irq_handler(i, cause, channel->data);
			} else {
				/*
				 * IRQ for an unregistered DMA channel
				 */
				printk(KERN_WARNING
				       "spurious IRQ for DMA channel %d\n", i);
			}
#ifdef CONFIG_NKERNEL
#else
			if (tcint & i_bit)
				__raw_writel(i_bit, DMAC_INT_TC_CLEAR);
			if (eint & i_bit)
				__raw_writel(i_bit, DMAC_INT_ERR_CLEAR);
#endif
		}
	}
	return IRQ_HANDLED;
}

static int __init pnx_dma_init(void)
{
	int ret=0, i,size;
    struct resource * res;

#ifdef CONFIG_NKERNEL
/* pointer to shared memory, common with RTK-E */
	NkPhAddr	addr;	/* Physical address of DMA shared device */
	NkXIrqId	xid;

	/* retrieve shared DMA memory */
	addr = nkops.nk_dev_lookup_by_type (NK_DEV_ID_DMAU, 0) ;

	/* bug if shared structure not available */
	BUG_ON(!addr);
    /* get clock */
	dma_clk=clk_get(0, "DMAU");
	BUG_ON(IS_ERR(dma_clk));
	/* get virtual address */
	shared_dma = (tNkDMAU *) nkops.nk_ptov(addr + sizeof(NkDevDesc));

	/* allocate one cross interrupt */
	shared_dma->linux_xirq = nkops.nk_xirq_alloc(1);

	/* attach cross interrupt handler */
	xid = nkops.nk_xirq_attach(	shared_dma->linux_xirq,	/* xirq ID */
			                    (NkXIrqHandler)dma_irq_handler,
								NULL);

	/* bug if attaching the handler failed */
	BUG_ON(!xid);
	/* set Linux OS id */
	shared_dma->linux_id = nkops.nk_id_get();

#else
	ret = request_irq(IRQ_DMAU, dma_irq_handler, 0, "DMA", NULL);
	if (ret) {
		printk(KERN_CRIT "Wow!  Can't register IRQ for DMA\n");
		goto out;
	}
#endif
	res = request_mem_region(DMAU_BASE_ADDR, SZ_4K, "pnx-dma");
	if (res==NULL) {
		printk(KERN_ERR "cannot get dma region");
		goto out;
	}
	size = res->end - res->start + 1;
	/* by default ioremap mapped it as non cacheable */
    DMAC_BASE = (unsigned long) ioremap(res->start,size);
	ll_pool.count = 0x4000 / sizeof(struct pnx_dma_ll);
	ll_pool.cur = ll_pool.vaddr =
	    dma_alloc_coherent(NULL, ll_pool.count * sizeof(struct pnx_dma_ll),
			       &ll_pool.dma_addr, GFP_KERNEL);

	if (!ll_pool.vaddr) {
		ret = -ENOMEM;
#ifndef CONFIG_NKERNEL
		free_irq(IRQ_DMAU, NULL);
#endif
		goto out;
	}

	for (i = 0; i < ll_pool.count - 1; i++) {
		void **addr = ll_pool.vaddr + i * sizeof(struct pnx_dma_ll);
		*addr = (void *)addr + sizeof(struct pnx_dma_ll);
	}
	*(long *)(ll_pool.vaddr +
		  (ll_pool.count - 1) * sizeof(struct pnx_dma_ll)) =
	    (long)ll_pool.vaddr;

	__raw_writel(1, DMAC_CONFIG);

out:
	return ret;
}
arch_initcall(pnx_dma_init);
