/*
 * linux/drivers/mtd/nand/pnx_nfi.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Ludovic Barre <ludovic.barre@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtd.h>
#include <linux/io.h>
#include <linux/pm_qos_params.h>

#include <mach/dma.h>
#include <mach/nand_pnx.h>

struct pnx_nfi {
	struct platform_device		*pdev;
	struct pnx_nand_platform_data	*pdata;
	struct resource			*mem;

	struct nand_chip		nand_chip;
	struct mtd_info			mtd;
	void __iomem			*regs;

	int				usedma;

	/* config descriptor */
	struct pnx_dma_ch_ctrl		write_ch_ctrl;
	struct pnx_dma_ch_ctrl		read_ch_ctrl;
	struct pnx_dma_ch_ctrl		ecc_ch_ctrl;
	struct pnx_dma_ch_config	read_ch_cfg;
	struct pnx_dma_ch_config	write_ch_cfg;
	struct pnx_dma_ch_config	ecc_ch_cfg;
	/* config used by dma driver */
	struct pnx_dma_config		write_cfg;
	struct pnx_dma_config		read_cfg;
	struct pnx_dma_config		ecc_cfg;

	int				dma_data_ch;
	int				dma_ecc_ch;
	u_char				*ecc_buf;
	u_char				*data_buf;

	struct clk			*clk;

	u32				with_ecc;
	/* timing configuration*/
	/* timer for pm_qos */
	struct timer_list		pm_qos_timer;
	int				pm_qos_rate;
	int				pm_qos_status;
	int				nandalreadylocked;
};

#if defined(CONFIG_MTD_NAND_BBM)
static uint8_t bbm_pattern[] = {'B', 'b', 'm', '0'};
static uint8_t mirror_bbm_pat[] = {'1', 'm', 'b', 'B'};
static struct nand_bbm_descr bbm_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 0,
	.pattern = bbm_pattern
};

static struct nand_bbm_descr bbm_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 8,
	.len = 4,
	.veroffs = 12,
	.maxblocks = 0,
	.pattern = mirror_bbm_pat
};
#endif

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/* ACER Erace.Ma@20100523, keep flash size for AP display */
int nandSize;
char nandID[10];
/* End Erace.Ma@20100523*/

enum t_NFI_pm_qos_status{
	PNXNAND_PM_QOS_DOWN,
	PNXNAND_PM_QOS_HALT,
	PNXNAND_PM_QOS_UP,
};

enum t_NFI_pm_qos_rate{
	PNXNAND_PM_QOS_STOP,
	PNXNAND_PM_QOS_XFER
};

/*
 * bitsperbyte contains the number of bits per byte
 * this is only used for testing and repairing parity
 * (a precalculated value slightly improves performance)
 */
static const char bitsperbyte[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

/*
 * addressbits is a lookup table to filter out the bits from the xor-ed
 * ecc data that identify the faulty location.
 * this is only used for repairing parity
 * see the comments in nand_correct_data for more details
 */
static const char addressbits[256] = {
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f
};

#define NFI_CFG_INIT		(NFI_WIDTH_8_BIT |	\
				NFI_TAC_MODE |		\
				NFI_CE_LOW_0 |		\
				NFI_DMA_BURST_ENABLE |	\
				NFI_ECC_ENABLE |	\
				NFI_DMA_ECC_ENABLE)

#define read_data_reg(dev)       \
	ioread32((dev)->regs + NFI_DATA_OFFSET)
#define write_data_reg(dev, val) \
	iowrite32((val), (dev)->regs + NFI_DATA_OFFSET)
#define write_cmd_reg(dev, val)  \
	iowrite32((val), (dev)->regs + NFI_CMD_OFFSET)
#define write_addr_reg(dev, val) \
	iowrite32((val), (dev)->regs + NFI_ADDR_OFFSET)

/*
 * ============================================================================
 * QOS & timer
 * ============================================================================
 */
void pnx_nfi_pm_qos_up(struct pnx_nfi *nfi)
{
	/* transfer will start */
	nfi->pm_qos_rate = PNXNAND_PM_QOS_XFER;
	wmb();
	/* increase hclk2 if needed */
	if (nfi->pm_qos_status == PNXNAND_PM_QOS_DOWN) {
		pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
				(char *) nfi->pdev->name, 104);
		mod_timer(&(nfi->pm_qos_timer), (jiffies + HZ/10));
	}
	nfi->pm_qos_status = PNXNAND_PM_QOS_UP;
}

void pnx_nfi_pm_qos_halt(struct pnx_nfi *nfi)
{
	/* transfer is stopped */
	nfi->pm_qos_rate = PNXNAND_PM_QOS_STOP;
}

void pnx_nfi_pm_qos_down(struct pnx_nfi *nfi)
{
	del_timer(&(nfi->pm_qos_timer));
	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
			(char *) nfi->pdev->name, PM_QOS_DEFAULT_VALUE);
	nfi->pm_qos_status = PNXNAND_PM_QOS_DOWN;
	nfi->pm_qos_rate = PNXNAND_PM_QOS_STOP;
}

void pnx_nfi_pm_qos_timeout(unsigned long arg)
{
	struct pnx_nfi *nfi = (struct pnx_nfi *) arg;

	if (nfi->pm_qos_rate == PNXNAND_PM_QOS_STOP) {
		if (nfi->pm_qos_status == PNXNAND_PM_QOS_HALT) {
			pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
					(char *) nfi->pdev->name,
					PM_QOS_DEFAULT_VALUE);
			nfi->pm_qos_status = PNXNAND_PM_QOS_DOWN;
		} else{
			nfi->pm_qos_status = PNXNAND_PM_QOS_HALT;
                        /* 2009/12/21, ACER Ed (LeonWu AU4.B-2190), 
                           check in STE patch BT_sleep_improvement.patch
                           To reduce system power consumption to 0.5mA when BT on
                        */
			// mod_timer(&(nfi->pm_qos_timer), (jiffies + HZ/10));
			mod_timer(&(nfi->pm_qos_timer), (jiffies + HZ/20) ); 
                        /* End, 2009/12/21, ACER Ed (LeonWu AU4.B-2190),  */ 
		}
	} else
                 /* 2009/12/21, 2009/12/21, ACER Ed (LeonWu AU4.B-2190), 
                           check in STE patch BT_sleep_improvement.patch
                           To reduce system power consumption to 0.5mA when BT on
                 */
		// mod_timer(&(nfi->pm_qos_timer), (jiffies + HZ/10));
		mod_timer(&(nfi->pm_qos_timer), (jiffies + HZ/20) );
                /* End, 2009/12/21, 2009/12/21, ACER Ed (LeonWu AU4.B-2190),  */ 
}

/*
 * ============================================================================
 * private functions
 * ============================================================================
 */
/**
  * @brief dma irq handler
  *
  * @param channel : number of channel that cause the interrupt
  * @param cause :   DMA_ERR_INT is enbaled , DMA_TC_INT is masked
  * @param cookie:   nfi
  *
  * @return
  */
void pnx_nfi_dma_handler(int channel, int cause, void *cookie)
{
	struct pnx_nfi *nfi = (struct pnx_nfi *) cookie;

	if ((cause & DMA_ERR_INT)) {
		if (channel == nfi->dma_data_ch)
			dev_err(&nfi->pdev->dev, "dma data error\n");
		else if (channel == nfi->dma_ecc_ch)
			dev_err(&nfi->pdev->dev, "dma ecc error\n");
		else
			dev_err(&nfi->pdev->dev, "dma spurious error\n");
	}
}

/**
 * pnx_nfi_wait_tc - wait for NFI terminal count flag
 * @mtd: MTD device structure
 *
 * This function poll until a terminal count occurred with timeout
 * normaly wait time: round 100us (dma transfert)
 * too short for completion or delay with sleep cpu
 */
void pnx_nfi_wait_tc(struct mtd_info *mtd)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	for (;;) {
		if (ioread32(nfi->regs + NFI_TC_OFFSET))
			goto loop_next;
		if (!pnx_dma_ch_fifo_empty(nfi->dma_data_ch))
			goto loop_next;
		if (nfi->with_ecc)
			if (!pnx_dma_ch_fifo_empty(nfi->dma_ecc_ch))
				goto loop_next;
		return;
loop_next:
		cond_resched();
	}
}

/**
 * pnx_nfi_write_buf_no_dma - write data to NAND
 * @mtd: MTD device structure
 * @buf: data to write
 * @len: size of data to write
 *
 * Write data to NAND not using DMA.
 */
static void pnx_nfi_write_buf_no_dma(struct mtd_info *mtd,
		const u_char *buf, int len)
{
	int i;
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	for (i = 0; i < len; i++)
		write_data_reg(nfi, buf[i]);
}

/**
 * pnx_nfi_read_buf_no_dma - read data from NAND
 * @mtd: MTD device structure
 * @buf: buffer to store the data
 * @len: size of data to read
 *
 * Read data from NAND not using DMA.
 */
static void pnx_nfi_read_buf_no_dma(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	for (i = 0; i < len; i++)
		buf[i] = read_data_reg(nfi);
}

/*
 * ============================================================================
 * overload board specific function of mtd api
 * ============================================================================
 */
/**
 * pnx_nfi_dev_ready - read ready status
 * @mtd: MTD device structure
 *
 * Read the NFI ready flag.
 */
static int pnx_nfi_dev_ready(struct mtd_info *mtd)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	return test_bit(NFI_RDY_SHIFT, nfi->regs + NFI_STAT_OFFSET);
}

/**
 * pnx_nfi_read_byte - read a byte from NAND
 * @mtd: MTD device structure
 *
 * Replace default nand_read_byte for
 * read a byte from NAND with read 32 bit.
 * (ahb constraint).
 */
static u8 pnx_nfi_read_byte(struct mtd_info *mtd)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	return read_data_reg(nfi);
}

/**
 * pnx_nfi_verify_buf - Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 *
 * Replace default pnx_nfi_verify_buf for
 * read a byte from NAND with read 32 bit.
 * (ahb constraint).
 */
static int pnx_nfi_verify_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	int i;
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	for (i = 0; i < len; i++)
		if (buf[i] != read_data_reg(nfi))
			return -EFAULT;
	return 0;
}

/**
 * pnx_nfi_write_buf - write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 *
 * Default write function for 8bit buswith using dma.
 */
static void pnx_nfi_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	int tr_size = len/4;
	u_char *buf_tmp = nfi->data_buf;

	if (len <= 0)
		return;

	if ((nfi->with_ecc == 0) || (len % 4) || (tr_size > DMA_TR_SIZE_MAX)) {
		pnx_nfi_write_buf_no_dma(mtd, buf, len);
		return;
	}
	/* write with dma burst (4 bytes) */
	/* clear bit dma start before programming dma */
	clear_bit(NFI_DMA_START_SHIFT, nfi->regs + NFI_CTRL_OFFSET);

	if (nfi->with_ecc) {
		set_bit(NFI_ECC_CLEAR_SHIFT, nfi->regs + NFI_CTRL_OFFSET);
		pnx_config_channel(nfi->dma_ecc_ch, &nfi->ecc_cfg);
		pnx_dma_ch_enable(nfi->dma_ecc_ch);
	}

	/* recopy the buffer in a non cacheable area */
	memcpy(buf_tmp, buf, len);

	/* clear tr_size bits and set manualy */
	nfi->write_cfg.ch_ctrl &= ~DMA_TR_SIZE_MAX;
	nfi->write_cfg.ch_ctrl |= tr_size & DMA_TR_SIZE_MAX;

	pnx_config_channel(nfi->dma_data_ch, &nfi->write_cfg);
	pnx_dma_ch_enable(nfi->dma_data_ch);

	/* set terminal count register */
	iowrite32(len, nfi->regs + NFI_TC_OFFSET);
	/* set dma data channel direction in write mode */
	clear_bit(NFI_DMA_DIR_SHIFT, nfi->regs + NFI_CONFIG_OFFSET);
	/* start dma transfer */
	set_bit(NFI_DMA_START_SHIFT, nfi->regs + NFI_CTRL_OFFSET);

	pnx_nfi_wait_tc(mtd);

	pnx_dma_ch_disable(nfi->dma_data_ch);

	if (nfi->with_ecc) {
		nfi->with_ecc = 0;
		pnx_dma_ch_disable(nfi->dma_ecc_ch);
	}
}

/**
 * pnx_nfi_read_buf - read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	buffer to store date
 * @len:	number of bytes to read
 *
 * Default read function for 8bit buswith using dma.
 */
static void pnx_nfi_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	int tr_size = len/4;
	u_char *buf_tmp = nfi->data_buf;

	if (len <= 0)
		return;

	if ((nfi->with_ecc == 0) || (len % 4) || (tr_size > DMA_TR_SIZE_MAX)) {
		pnx_nfi_read_buf_no_dma(mtd, buf, len);
		return;
	}
	/* read with dma burst (4 bytes) */
	/* clear bit dma start before programming dma */
	clear_bit(NFI_DMA_START_SHIFT, nfi->regs + NFI_CTRL_OFFSET);

	if (nfi->with_ecc) {
		set_bit(NFI_ECC_CLEAR_SHIFT, nfi->regs + NFI_CTRL_OFFSET);
		pnx_config_channel(nfi->dma_ecc_ch, &nfi->ecc_cfg);
		pnx_dma_ch_enable(nfi->dma_ecc_ch);
	}

	/* clear tr_size bits and set manualy */
	nfi->read_cfg.ch_ctrl &= ~DMA_TR_SIZE_MAX;
	nfi->read_cfg.ch_ctrl |= tr_size & DMA_TR_SIZE_MAX;

	pnx_config_channel(nfi->dma_data_ch, &nfi->read_cfg);
	pnx_dma_ch_enable(nfi->dma_data_ch);

	/* set terminal count register */
	iowrite32(len, nfi->regs + NFI_TC_OFFSET);
	/* set dma data channel direction in read mode */
	set_bit(NFI_DMA_DIR_SHIFT, nfi->regs + NFI_CONFIG_OFFSET);
	/* start dma transfer */
	set_bit(NFI_DMA_START_SHIFT, nfi->regs + NFI_CTRL_OFFSET);

	pnx_nfi_wait_tc(mtd);

	pnx_dma_ch_disable(nfi->dma_data_ch);

	if (nfi->with_ecc) {
		nfi->with_ecc = 0;
		pnx_dma_ch_disable(nfi->dma_ecc_ch);
	}

	/* finish DMA (data and ecc) transfer manually */
	/* recopy the data from the non cacheable area */
	memcpy(buf, buf_tmp, len);
}

/**
 * pnx_nfi_select_chip
 * @mtd:	MTD device structure
 * @chipnr:	chipnumber to select, -1 for deselect
 *
 * qos power halt on deselec chip, qos power up on command
 */
static void pnx_nfi_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	switch (chipnr) {
	case -1: /* chip deselection is requested */
		if (nfi->nandalreadylocked) {
			clk_disable(nfi->clk);
			pnx_nfi_pm_qos_halt(nfi);
			nfi->nandalreadylocked = 0;
		}
		break;
	case 0:		/* select */
		break;
	default:
		BUG();
	}
}

/**
 * pnx_nfi_hwcontrol
 * @mtd: MTD device structure
 * @cmd: NAND flash command to be send
 *
 * this function is not necessary
 * all ctrl is integrated in pnx_nfi_nand_command_lp
 */
static void pnx_nfi_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);

	dev_err(&nfi->pdev->dev, "spurious call at cmd_ctrl function.\n");
}

/**
 * pnx_nfi_nand_command_lp - Send command to NAND large page device
 * @mtd:	MTD device structure
 * @command:	the command to be sent
 * @column:	the column address for this command, -1 if none
 * @page_addr:	the page address for this command, -1 if none
 *
 * Send command to NAND device. This is the version for the new large page
 * devices. We must emulate NAND_CMD_READOOB to keep the code compatible.
 * Simplify for pnx
 */
static void pnx_nfi_nand_command_lp(struct mtd_info *mtd,
		unsigned int command, int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	unsigned long timeo = jiffies + 2;

	if (!nfi->nandalreadylocked) {
		/* chip selection is requested */
		nfi->nandalreadylocked = 1;
		pnx_nfi_pm_qos_up(nfi);
		clk_enable(nfi->clk);
	}

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}
	/* write command */
	write_cmd_reg(nfi, command & 0xff);

	if (column != -1 || page_addr != -1) {
		if (column != -1) {
			write_addr_reg(nfi, column);
			write_addr_reg(nfi, column >> 8);
		}
		if (page_addr != -1) {
			write_addr_reg(nfi, page_addr);
			write_addr_reg(nfi, page_addr >> 8);
			if (chip->chipsize > (128 << 20))
				write_addr_reg(nfi, page_addr >> 16);
		}
	}
	/*
	 * program and erase have their own busy handlers
	 * status, sequential in, and deplete1 need no delay
	 */
	switch (command) {

	case NAND_CMD_READ0:
		write_cmd_reg(nfi, NAND_CMD_READSTART);
		break;
	case NAND_CMD_RNDOUT:
		write_cmd_reg(nfi, NAND_CMD_RNDOUTSTART);
		return;
	case NAND_CMD_RESET:
		if (chip->dev_ready)
			break;
		udelay(chip->chip_delay);
		write_cmd_reg(nfi, NAND_CMD_STATUS);
		while (!(read_data_reg(nfi) & NAND_STATUS_READY));
		return;
	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_DEPLETE1:
		return;
	/* read error status commands require only a short delay */
	case NAND_CMD_STATUS_ERROR:
	case NAND_CMD_STATUS_ERROR0:
	case NAND_CMD_STATUS_ERROR1:
	case NAND_CMD_STATUS_ERROR2:
	case NAND_CMD_STATUS_ERROR3:
		udelay(chip->chip_delay);
		return;
	}

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	ndelay(100);

	/* wait until command is processed or timeout occures */
	do {
		if (chip->dev_ready(mtd))
			break;
	} while (time_before(jiffies, timeo));
}

/**
 * pnx_nfi_enable_hwecc - function to enable (reset) hardware ecc generator
 * @mtd: MTD device structure
 * @mode: the mode for which the hardware ECC generation should be enabled
 *
 * This function resets the hardware ecc generator and enables
 * the ecc generation for the successive transfer.
 */
static void pnx_nfi_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	nfi->with_ecc = 1;
}

/**
 * pnx_nfi_calculate_ecc - function for ecc calculation or readback from ecc hardware
 * @mtd: MTD device structure
 * @dat: the data for ecc calculation
 * @ecc_code: buffer to store the calculated ecc values
 *
 * This function translates the ecc values from PNX hardware style to the
 * format expected by MTD layer.
 */
static int pnx_nfi_calculate_ecc(struct mtd_info *mtd,
		const u_char *dat, u_char *ecc_code)
{
	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	int i;

	for (i = 0; i < 8; i++) {
		/* translate hw-ecc values to Linux format */
		ecc_code[i*3] = ~((nfi->ecc_buf[i*4+1] >> 6)
				+ (nfi->ecc_buf[i*4 + 2] << 2));
		ecc_code[i*3+1] = ~((nfi->ecc_buf[i*4] >> 6)
				+ (nfi->ecc_buf[i*4 + 1] << 2));
		ecc_code[i*3+2] = (~(nfi->ecc_buf[i*4] << 2));
	}
	nfi->with_ecc = 0;
	return 0;
}

/**
 * pnx_nfi_correct_data - function for ecc correction, matching to ecc generator
 * @mtd: MTD device structure
 * @dat: the data for ecc correction
 * @read_ecc: ecc values read from the hardware
 * @calc_ecc: ecc values generated from raw data
 *
 * nfi driver set ecc.size to 2048 because read/write by page (not by chunk)
 * So doesn't use nand_correct_data directly, So overload ecc.correct
 * This function reuse nand_correct_data code but fix number bit error
 * ( 1bit error for 256 byte block )
 */
static int pnx_nfi_correct_data(struct mtd_info *mtd, u_char *dat,
		u_char *read_ecc, u_char *calc_ecc)
{
	unsigned char b0, b1, b2;
	unsigned char byte_addr, bit_addr;

	struct pnx_nfi *nfi = container_of(mtd, struct pnx_nfi, mtd);
	int ret = 0;
	int corrected = 0;
	int i;

	for (i = 0; i < 8; i++) {
		b0 = read_ecc[1+(i*3)] ^ calc_ecc[1+(i*3)];
		b1 = read_ecc[0+(i*3)] ^ calc_ecc[0+(i*3)];
		b2 = read_ecc[2+(i*3)] ^ calc_ecc[2+(i*3)];

		if ((b0 | b1 | b2) == 0)
			continue;	/* no error */

		if ((((b0 ^ (b0 >> 1)) & 0x55) == 0x55) &&
		    (((b1 ^ (b1 >> 1)) & 0x55) == 0x55) &&
		    (((b2 ^ (b2 >> 1)) & 0x54) == 0x54)) {
			/* single bit error */
			byte_addr = (addressbits[b1] << 4) + addressbits[b0];
			bit_addr = addressbits[b2 >> 2];
			/* flip the bit */
			dat[byte_addr+(i*256)] ^= (1 << bit_addr);
			corrected++;
			continue;
		}
		/* count nr of bits; use table lookup,
		 * faster than calculating it
		 * error in ecc data; no action needed
		 */
		if ((bitsperbyte[b0] + bitsperbyte[b1] +
					bitsperbyte[b2]) == 1) {
			corrected++;
			continue;
		}
		/* uncorrectable error */
		ret = -1;
	}

	if (ret < 0) {
		dev_err(&nfi->pdev->dev, "uncorrectable error : ");
		return ret;
	}
	return corrected;
}

/*
 * ============================================================================
 * Init,register,unregister nfi functions
 * ============================================================================
 */
static void pnx_nfi_hardware_init(struct pnx_nfi *nfi)
{
	write_cmd_reg(nfi, NAND_CMD_RESET);

	/* init NFI registers */
	iowrite32(NFI_CFG_INIT, nfi->regs + NFI_CONFIG_OFFSET);
	iowrite32(nfi->pdata->timing[*nfi->pdata->combo_index].timing_tac, nfi->regs + NFI_TAC_OFFSET);
	iowrite32(nfi->pdata->timing[*nfi->pdata->combo_index].timing_tac_read, nfi->regs + NFI_TAC_READ_OFFSET);

	/* clear all IT */
	iowrite32(NFI_RDYINT | NFI_TCZINT, nfi->regs + NFI_ICR_OFFSET);
}

static void pnx_nfi_dma_free(struct pnx_nfi *nfi)
{
	if (nfi->data_buf)
		dma_free_coherent(&nfi->pdev->dev, 4096,
				nfi->data_buf, nfi->write_cfg.src_addr);
	if (nfi->ecc_buf)
		dma_free_coherent(&nfi->pdev->dev, 32,
				nfi->ecc_buf, nfi->ecc_cfg.dest_addr);
	if (!IS_ERR_VALUE(nfi->dma_data_ch)) {
		pnx_dma_ch_disable(nfi->dma_data_ch);
		pnx_free_channel(nfi->dma_data_ch);
	}
	if (!IS_ERR_VALUE(nfi->dma_ecc_ch)) {
		pnx_dma_ch_disable(nfi->dma_ecc_ch);
		pnx_free_channel(nfi->dma_ecc_ch);
	}
}

static int pnx_nfi_dma_init(struct pnx_nfi *nfi)
{
	dma_addr_t dma_data_handle, dma_ecc_handle;
	int err;

	/* allocating memory for dma buffer */
	nfi->data_buf = dma_alloc_coherent(&nfi->pdev->dev,
			4096, &dma_data_handle, GFP_KERNEL);
	if (!nfi->data_buf) {
		dev_err(&nfi->pdev->dev, "dma (data) allocation failed.\n");
		err = -ENOMEM;
		goto out;
	}

	nfi->ecc_buf = dma_alloc_coherent(&nfi->pdev->dev,
			32, &dma_ecc_handle, GFP_KERNEL);
	if (!nfi->ecc_buf) {
		dev_err(&nfi->pdev->dev, "dma (ecc) allocation failed.\n");
		err = -ENOMEM;
		goto out;
	}

	/*  write config  */
	nfi->write_ch_ctrl.tc_mask = 1; /* FIXME LBA normalement 0 */
	nfi->write_ch_ctrl.di = 0;
	nfi->write_ch_ctrl.si = 1;
	nfi->write_ch_ctrl.dest_ahb1 = 1;
	nfi->write_ch_ctrl.src_ahb1 = 1;
	nfi->write_ch_ctrl.dwidth = WIDTH_WORD;
	nfi->write_ch_ctrl.swidth = WIDTH_WORD;
	nfi->write_ch_ctrl.dbsize = 4;
	nfi->write_ch_ctrl.sbsize = 4;

	nfi->write_ch_cfg.flow_cntrl = FC_MEM2PER_DMA ;
	nfi->write_ch_cfg.dest_per = PER_NFI;
	nfi->write_ch_cfg.src_per = 0 ;
	nfi->write_ch_cfg.ie = 1;
	nfi->write_ch_cfg.itc = 0;

	nfi->write_cfg.src_addr = dma_data_handle;
	nfi->write_cfg.dest_addr = nfi->mem->start + NFI_DMA_DATA_OFFSET;
	nfi->write_cfg.ll_dma = 0;
	nfi->write_cfg.is_ll = 0;

	pnx_dma_pack_control(&nfi->write_ch_ctrl, &(nfi->write_cfg.ch_ctrl));
	pnx_dma_pack_config(&nfi->write_ch_cfg, &(nfi->write_cfg.ch_cfg));

	/* read config */
	nfi->read_ch_ctrl.tc_mask = 1; /* FIXME LBA normalement 0 */
	nfi->read_ch_ctrl.di = 1;
	nfi->read_ch_ctrl.si = 0;
	nfi->read_ch_ctrl.dest_ahb1 = 1;
	nfi->read_ch_ctrl.src_ahb1 = 1;
	nfi->read_ch_ctrl.dwidth = WIDTH_WORD;
	nfi->read_ch_ctrl.swidth = WIDTH_WORD;
	nfi->read_ch_ctrl.dbsize = 4;
	nfi->read_ch_ctrl.sbsize = 4;

	nfi->read_ch_cfg.flow_cntrl = FC_PER2MEM_DMA;
	nfi->read_ch_cfg.dest_per = 0;
	nfi->read_ch_cfg.src_per = PER_NFI;
	nfi->read_ch_cfg.ie = 1;
	nfi->read_ch_cfg.itc = 0;

	nfi->read_cfg.src_addr = nfi->mem->start + NFI_DMA_DATA_OFFSET;
	nfi->read_cfg.dest_addr = dma_data_handle;
	nfi->read_cfg.ll_dma = 0;
	nfi->read_cfg.is_ll = 0;

	pnx_dma_pack_control(&nfi->read_ch_ctrl, &(nfi->read_cfg.ch_ctrl));
	pnx_dma_pack_config(&nfi->read_ch_cfg, &(nfi->read_cfg.ch_cfg));

	/* ecc config */
	nfi->ecc_ch_ctrl.tc_mask = 1; /* FIXME LBA normalement 0 */
	nfi->ecc_ch_ctrl.di = 1;
	nfi->ecc_ch_ctrl.si = 0;
	nfi->ecc_ch_ctrl.dest_ahb1 = 1;
	nfi->ecc_ch_ctrl.src_ahb1 = 1;
	nfi->ecc_ch_ctrl.dwidth = WIDTH_WORD;
	nfi->ecc_ch_ctrl.swidth = WIDTH_WORD;
	nfi->ecc_ch_ctrl.dbsize = 1; /* no burst not supported */
	nfi->ecc_ch_ctrl.sbsize = 1;
	nfi->ecc_ch_ctrl.tr_size = 32/4;

	nfi->ecc_ch_cfg.flow_cntrl = FC_PER2MEM_DMA;
	nfi->ecc_ch_cfg.dest_per = 0;
	nfi->ecc_ch_cfg.src_per = PER_NFI_ECC;
	nfi->ecc_ch_cfg.ie = 1;
	nfi->ecc_ch_cfg.itc = 0;
	nfi->ecc_ch_cfg.halt = 0;

	nfi->ecc_cfg.src_addr = nfi->mem->start + NFI_ECC_OFFSET;
	nfi->ecc_cfg.dest_addr = dma_ecc_handle;
	nfi->ecc_cfg.ll_dma = 0;
	nfi->ecc_cfg.is_ll = 0;

	pnx_dma_pack_control(&nfi->ecc_ch_ctrl, &(nfi->ecc_cfg.ch_ctrl));
	pnx_dma_pack_config(&nfi->ecc_ch_cfg, &(nfi->ecc_cfg.ch_cfg));

	/* request the channel */
	nfi->dma_data_ch = pnx_request_channel("NFI DAT",
				nfi->pdata->dma_data_ch,
				pnx_nfi_dma_handler, (void *)nfi);
	if (IS_ERR_VALUE(nfi->dma_data_ch)) {
		dev_err(&nfi->pdev->dev, "unable to get dma data channel.\n");
		err = nfi->dma_data_ch;
		goto out;
	}

	nfi->dma_ecc_ch = pnx_request_channel("NFI ECC",
				nfi->pdata->dma_ecc_ch,
				pnx_nfi_dma_handler, (void *)nfi);
	if (IS_ERR_VALUE(nfi->dma_ecc_ch)) {
		dev_err(&nfi->pdev->dev, "unable to get dma ecc channel.\n");
		err = nfi->dma_ecc_ch;
		goto out;
	}

	dev_notice(&nfi->pdev->dev,
				"dma data ch: %d dma ecc ch: %d\n",
				nfi->dma_data_ch, nfi->dma_ecc_ch);

	return 0;

out:
	pnx_nfi_dma_free(nfi);
	return err;
}

static void pnx_nfi_mtd_init(struct pnx_nfi *nfi)
{
	struct nand_chip *this = &nfi->nand_chip;

	/* initialize MTD NAND driver */
	this->IO_ADDR_R = nfi->regs + NFI_DATA_OFFSET;
	this->IO_ADDR_W = nfi->regs + NFI_DATA_OFFSET;
	this->read_byte = pnx_nfi_read_byte;
	this->verify_buf = pnx_nfi_verify_buf;

	if (nfi->pdata->usedma) {
		dev_notice(&nfi->pdev->dev, "ecc hw enabled\n");
		this->read_buf = pnx_nfi_read_buf;
		this->write_buf = pnx_nfi_write_buf;

		this->ecc.mode = NAND_ECC_HW;
		this->ecc.hwctl  = pnx_nfi_enable_hwecc;
		this->ecc.calculate = pnx_nfi_calculate_ecc;
		this->ecc.correct  = pnx_nfi_correct_data;
		this->ecc.bytes = 24;
		this->ecc.size = 2048;
	} else {
		dev_notice(&nfi->pdev->dev, "ecc soft enabled\n");
		this->read_buf = pnx_nfi_read_buf_no_dma;
		this->write_buf = pnx_nfi_write_buf_no_dma;
		this->ecc.mode = NAND_ECC_SOFT;
	}

	this->cmdfunc = pnx_nfi_nand_command_lp;
	this->cmd_ctrl = pnx_nfi_cmd_ctrl;
	this->select_chip = pnx_nfi_select_chip;
	this->dev_ready = pnx_nfi_dev_ready;
	this->chip_delay = 25;

	this->options = NAND_CACHEPRG;

#ifdef CONFIG_MTD_NAND_BBM
	this->options |= NAND_USE_FLASH_BBT;
	this->bbm_td = &bbm_main_descr;
	this->bbm_md = &bbm_mirror_descr;
#endif
}

static int __devinit pnx_nfi_probe(struct platform_device *pdev)
{
	struct pnx_nfi *nfi;
	struct mtd_info *mtd;
	int ret = 0;

	struct mtd_partition *mtd_parts;
	int mtd_parts_nb = 0;
	const char *part_type = 0;
	int i;
	int maf_id;
	int dev_id;
	struct nand_chip *chip;

	nfi = kzalloc(sizeof(*nfi), GFP_KERNEL);
	if (!nfi) {
		ret = -ENOMEM;
		goto out;
	}

	nfi->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!nfi->mem) {
		ret = -ENXIO;
		goto err_res;
	}

	nfi->mem = request_mem_region(nfi->mem->start,
			nfi->mem->end-nfi->mem->start+1, pdev->name);
	if (!nfi->mem) {
		ret = -ENXIO;
		goto err_req_mem;
	}

	nfi->regs = ioremap(nfi->mem->start,
			nfi->mem->end-nfi->mem->start+1);
	if (!nfi->regs) {
		ret = -EIO;
		goto err_ioremap;
	}

	nfi->pdata = pdev->dev.platform_data;
	nfi->pdev = pdev;

	/* link the private data structures */
	mtd = &nfi->mtd;
	mtd->priv = &nfi->nand_chip;
	nfi->nand_chip.priv = nfi;
	mtd->owner = THIS_MODULE;

	pnx_nfi_mtd_init(nfi);
	platform_set_drvdata(pdev, nfi);

	if (nfi->pdata->usedma) {
		ret = pnx_nfi_dma_init(nfi);
		if (IS_ERR_VALUE(ret))
			goto err_dma;
	}

	nfi->clk = clk_get(NULL, "NFI");
	if (IS_ERR(nfi->clk)) {
		dev_err(&nfi->pdev->dev, "failed to get nfi clk\n");
		ret = PTR_ERR(nfi->clk);
		goto err_clk;
	}
	/* init qos requirement */
	nfi->pm_qos_rate = PNXNAND_PM_QOS_STOP;
	nfi->pm_qos_status = PNXNAND_PM_QOS_DOWN;
	init_timer(&(nfi->pm_qos_timer));
	nfi->pm_qos_timer.function = &pnx_nfi_pm_qos_timeout;
	nfi->pm_qos_timer.data = (unsigned long)nfi;
	pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT,
			(char *) pdev->name, PM_QOS_DEFAULT_VALUE);

	pnx_nfi_pm_qos_up(nfi);
	clk_enable(nfi->clk);

	/* init HW registers before accessing to nand */
	pnx_nfi_hardware_init(nfi);

	/* detect NAND chip */
	if (nand_scan(mtd, 1)) {
		dev_err(&nfi->pdev->dev, "no nand chips found?\n");
		ret = -ENXIO;
		goto err_scan;
	}

	/* Identify the NAND and set up the timing again */
	chip = mtd->priv;
	chip->select_chip(mtd, 0);
	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
	maf_id = chip->read_byte(mtd);
	dev_id = chip->read_byte(mtd);

	for (i = 0; nfi->pdata->combo_info[i].device_id != 0xFF; i++) {
		if (dev_id == nfi->pdata->combo_info[i].device_id &&
				maf_id == nfi->pdata->combo_info[i].manuf_id) {
			*nfi->pdata->combo_index = i;
			break;
		}
	}

	/* Write the the new timing values to the block */
	pnx_nfi_hardware_init(nfi);

#ifdef CONFIG_MTD_CMDLINE_PARTS
	mtd->name = "pnx_board_nand";
	mtd_parts_nb = parse_mtd_partitions(mtd, part_probes, &mtd_parts, 0);
	if (mtd_parts_nb > 0)
		part_type = "command line";
	else
		mtd_parts_nb = 0;
#endif
	if ((mtd_parts_nb <= 0) && nfi->pdata) {
		mtd_parts = nfi->pdata->parts;
		mtd_parts_nb = nfi->pdata->nr_parts;
		part_type = "static";
	}

	if (mtd_parts_nb) {
		dev_notice(&nfi->pdev->dev,
				"Using %s partition definition\n", part_type);
		ret = add_mtd_partitions(mtd, mtd_parts, mtd_parts_nb);
	} else {
		dev_err(&nfi->pdev->dev,
				"No partition defined in platform data\n");
		ret = -EINVAL;
		goto err_partition;
	}

	/* ACER Erace.Ma@20100106, keep nand flash size for AP display */
	nandSize = (mtd->size/(1024*1024));
	/* End Erace.Ma@20100106*/

	clk_disable(nfi->clk);
	pnx_nfi_pm_qos_down(nfi);

	if (!ret)
		return ret;

err_partition:
	nand_release(mtd);
err_scan:
	clk_disable(nfi->clk);
	pnx_nfi_pm_qos_down(nfi);
err_clk:
err_dma:
	if (nfi->pdata->usedma)
		pnx_nfi_dma_free(nfi);
	iounmap(nfi->regs);
err_ioremap:
	release_mem_region(nfi->mem->start, nfi->mem->end - nfi->mem->start+1);
err_req_mem:
err_res:
	kfree(nfi);
out:
	return ret;
}

static int __devexit pnx_nfi_remove(struct platform_device *pdev)
{
	struct pnx_nfi *nfi;

	nfi = platform_get_drvdata(pdev);
	if (!nfi)
		return 0;

	platform_set_drvdata(pdev, NULL);

	if (nfi->regs)
		iounmap(nfi->regs);

	clk_put(nfi->clk);
	if (nfi->pdata->usedma)
		pnx_nfi_dma_free(nfi);
	kfree(nfi);

	return 0;
}

static struct platform_driver pnx_nfi_driver = {
	.probe = pnx_nfi_probe,
	.remove = pnx_nfi_remove,
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "pnx-nand",
	},
};

static int __init pnx_nfi_init(void)
{
	return platform_driver_register(&pnx_nfi_driver);
}

static void __exit pnx_nfi_exit(void)
{
	platform_driver_unregister(&pnx_nfi_driver);
}

module_init(pnx_nfi_init);
module_exit(pnx_nfi_exit);

MODULE_DESCRIPTION("ST-Ericsson PNX NAND Flash Interface driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ludovic Barre <ludovic.barre@stericsson.com>");
