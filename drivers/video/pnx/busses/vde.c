/*
 * linux/drivers/video/pnx/busses/vde.c
 *
 * Video Display Engine (VDE) driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <asm/io.h>
#include <video/pnx/lcdbus.h>
#include <video/pnx/lcdctrl.h>
#include <mach/vde.h>

/* configuration register fields */
#define VDE_REG_CONF_DISPLAY(v)  (v)
#define VDE_REG_CONF_BURST(v)    ((v) << 1)
#define VDE_REG_CONF_CLOCKDIV(v) (((v) & 0x3) << 2)
#define VDE_REG_CONF_ALIGN(v)    ((v) << 4)


/* display configuration register fields */
#define VDE_REG_CONFD_MODE(v)    ((v) & 0x3)
#define VDE_REG_CONFD_SER(v)     (((v) & 0x3) << 2)
#define VDE_REG_CONFD_HOLD(v)    ((v) << 4)
#define VDE_REG_CONFD_RH(v)      (((v) & 0x7) << 5)
#define VDE_REG_CONFD_RC(v)      (((v) & 0x7) << 8)
#define VDE_REG_CONFD_RS(v)      (((v) & 0x3) << 11)
#define VDE_REG_CONFD_WH(v)      (((v) & 0x7) << 13)
#define VDE_REG_CONFD_WC(v)      (((v) & 0x7) << 16)
#define VDE_REG_CONFD_WS(v)      (((v) & 0x3) << 19)
#define VDE_REG_CONFD_CH(v)      ((v) << 21)
#define VDE_REG_CONFD_CS(v)      ((v) << 22)
#define VDE_REG_CONFD_CSKIP(v)   ((v) << 23)
#define VDE_REG_CONFD_BSWAP(v)   ((v) << 24)


/* EFOI register fields */
#define VDE_REG_EOFI_CTRL_DEL(v)    ((v)  & 0xFFFFFF)
#define VDE_REG_EOFI_CTRL_SKIP(v)   (((v) & 0x7F) << 24)
#define VDE_REG_EOFI_CTRL_POL(v)    ((v) << 31)

/* desc0 fields */
#define VDE_DESC0_A0                       0
#define VDE_DESC0_DIR                      1
#define VDE_DESC0_D2                       2
#define VDE_DESC0_NX(v)                    (((v) & 0x7FF) <<  3)
#define VDE_DESC0_NY(v)                    (((v) & 0x3FF) << 14)
#define VDE_DESC0_OFF(v)                   (((v) & 0xFF)  << 24)

/* desc1 just holds an address */

/* desc2 fields */
#define VDE_DESC2_OFMT(v)                  (((v) & 0x3)   << 0)
#define VDE_DESC2_IFMT(v)                  (((v) & 0x3)   << 2)
#define VDE_DESC2_STRIDE(v)                (((v) & 0xFFF) << 4)

/* desc3 fields */
#define VDE_DESC3_LAST                     0
#define VDE_DESC3_PAUSE                    1
#define VDE_DESC3_IRQ                      2
#define VDE_DESC3_NEXT(v)                  ((u32)(v) & 0xFFFFFFF0)

/* desc2 values */
#define VDE_DESC_OFMT_RGB_3_3_2			    0
#define VDE_DESC_OFMT_RGB_TRANSP_8_BITS		    0
#define VDE_DESC_OFMT_RGB_4_4_4                     1
#define VDE_DESC_OFMT_RGB_5_6_5			    2
#define VDE_DESC_OFMT_RGB_TRANSP_16_BITS	    2
#define VDE_DESC_OFMT_RGB_6_6_6                     3

#define VDE_DESC_IFMT_TRANSP			    0
#define VDE_DESC_IFMT_RGB_5_6_5                     2
#define VDE_DESC_IFMT_RGB_8_8_8                     3

/* desc0 values */
#define VDE_DESC_D2_DMA_LINEAR_ADDRESSING_1D        0
#define VDE_DESC_D2_DMA_STRIDE_INCREMENT_2D         1
#define VDE_DESC_A0_AS_COMMAND                      0
#define VDE_DESC_A0_AS_DATA                         1
#define VDE_DESC_DIR_WRITE                          0
#define VDE_DESC_DIR_READ                           1
/* desc3 values */
#define VDE_DESC_LAST_MORE_DESCRIPTORS              0
#define VDE_DESC_LAST_NO_MORE_DESCRIPTORS           1
#define VDE_DESC_PAUSE_DONT_WAIT_FOR_EXTERNAL_SYNC  0
#define VDE_DESC_IRQ_DONT_GENERATE_INTERRUPT        0
#define VDE_DESC_IRQ_GENERATE_INTERRUPT             1

/* number of supported displays */
/* TODO (Plaform dependent) to be updated if needed */
#define VDE_NUM_DISPLAYS         1

/* 
 * Default vde configuration
 */
static struct lcdbus_conf __devinitdata vde_def_config = {
	.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB565,
	.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB565,
	.cmd_ofmt  = LCDBUS_INPUT_DATAFMT_TRANSP,
	.cmd_ifmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS,
	
	.cskip = 0,
	.bswap = 0,

	.eofi_del  = 0,
	.eofi_skip = 0,
	.eofi_pol = 0,
	.eofi_use_vsync = 1
};

/*
 * How standard data formats map to internal values.
 *
 * A -1 value means that this particular output format is not supported by the
 * bus controller
 */
static const int vde_data_ofmt[] = {
    /* Input format */
    [LCDBUS_INPUT_DATAFMT_RGB565]	    = -1,
    [LCDBUS_INPUT_DATAFMT_RGB888]	    = -1,
    [LCDBUS_INPUT_DATAFMT_TRANSP]	    = -1,

    /* Output format */
    [LCDBUS_OUTPUT_DATAFMT_RGB332]	    = VDE_DESC_OFMT_RGB_3_3_2,
    [LCDBUS_OUTPUT_DATAFMT_RGB444]	    = VDE_DESC_OFMT_RGB_4_4_4,
    [LCDBUS_OUTPUT_DATAFMT_RGB565]	    = VDE_DESC_OFMT_RGB_5_6_5,
    [LCDBUS_OUTPUT_DATAFMT_RGB666]	    = VDE_DESC_OFMT_RGB_6_6_6,
    [LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS]   = VDE_DESC_OFMT_RGB_TRANSP_8_BITS,
    [LCDBUS_OUTPUT_DATAFMT_TRANSP_16_BITS]  = VDE_DESC_OFMT_RGB_TRANSP_16_BITS
};

/* See previous comment */
static const int vde_data_ifmt[] = {
    /* Input format */
    [LCDBUS_INPUT_DATAFMT_RGB565]	    = VDE_DESC_IFMT_RGB_5_6_5,
    [LCDBUS_INPUT_DATAFMT_RGB888]	    = VDE_DESC_IFMT_RGB_8_8_8,
    [LCDBUS_INPUT_DATAFMT_TRANSP]	    = VDE_DESC_IFMT_TRANSP,

    /* Output format */
    [LCDBUS_OUTPUT_DATAFMT_RGB332]	    = -1,
    [LCDBUS_OUTPUT_DATAFMT_RGB444]	    = -1,
    [LCDBUS_OUTPUT_DATAFMT_RGB565]	    = -1,
    [LCDBUS_OUTPUT_DATAFMT_RGB666]	    = -1,
    [LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS]   = -1,
    [LCDBUS_OUTPUT_DATAFMT_TRANSP_16_BITS]  = -1,
};

/**
 * struct vde_desc - VDE descriptor data structure
 * @desc0: contains transfer type and parameters
 * @desc1: contains source and destination address in memory
 * @desc2: contains data format information
 * @desc3: contains next descriptor address and flow control
 */
struct vde_desc {
	u32 desc0;
	u32 desc1;
	u32 desc2;
	u32 desc3;
};

/**
 * struct vde_t - drivers private data structure
 * @device: the device this drivers belongs to
 * @clk: clock structure of the VDE
 * @irq: the VDE irq identifier
 * @io_mutex: mutex to lock hardware accesses
 * @end_process: completion to wait for irqs
 * @cmds_list: commands list logical address
 * @cmds_list_phys: commands list physical address
 * @cmds_list_max_size: the max size (in bytes) of commands list
 * @lcd: LCD control structure (used by LCD driver)
 * @conf: bus configuration for each display
 */
struct vde_t {
	struct device *device;
	struct clk *clk;
	int    irq;
	struct mutex io_mutex;
	struct completion end_process;

	struct vde_desc *cmds_list;
	u32    cmds_list_phys;
	u32    cmds_list_max_size;

	struct lcdctrl_device  lcd[VDE_NUM_DISPLAYS];
	struct lcdbus_conf conf[VDE_NUM_DISPLAYS];

	struct vde_platform_data *platform_data;
};

/**
 * vde_cmds_to_desc_chain - construct VDE descriptor chain
 * 
 */
static int
vde_cmds_to_desc_chain(struct vde_t *vde,
		const struct list_head *commands,
		const u8 dir,
		const struct lcdbus_conf *conf)
{
	struct vde_desc *prev_desc, *desc;
	struct lcdbus_cmd *cmd;
	u32 phys_addr, size = 0;
	u16 data_ifmt = vde_data_ifmt[conf->data_ifmt];
	u16 data_ofmt = vde_data_ofmt[conf->data_ofmt];
	u16 cmd_ifmt = vde_data_ifmt[conf->cmd_ifmt];
	u16 cmd_ofmt = vde_data_ofmt[conf->cmd_ofmt];

	desc = vde->cmds_list;
	prev_desc = vde->cmds_list;
	phys_addr = vde->cmds_list_phys;

	/* Prepare the VDE descriptor list */
	list_for_each_entry(cmd, commands, link) {

		/* Check the desc cmd size */
		size += sizeof(struct vde_desc);
		if (size > vde->cmds_list_max_size) {
			printk(KERN_ERR "%s(********************************************\n",
					__FUNCTION__);
			printk(KERN_ERR "%s(* Too many cmds (%d), please try to:\n",
					__FUNCTION__, size / sizeof(struct vde_desc));
			printk(KERN_ERR "%s(* 1- increase LCDBUS_CMDS_LIST_LENGTH (%d)\n",
					__FUNCTION__, LCDBUS_CMDS_LIST_LENGTH);
			printk(KERN_ERR "%s(* 2- Split the commands list\n",
					__FUNCTION__);
			printk(KERN_ERR "%s(********************************************\n",
				__FUNCTION__);
			return -ENOMEM;
		}

		/* DESC1 */
		desc->desc1 = cmd->data_phys;

		if (cmd->type == LCDBUS_CMDTYPE_DATA) {

			/* DESC0 */
			desc->desc0 = VDE_DESC0_OFF(cmd->cmd) |
				VDE_DESC0_NY(cmd->h) |
				VDE_DESC0_NX(cmd->w * cmd->bytes_per_pixel) |
				VDE_DESC_D2_DMA_STRIDE_INCREMENT_2D << VDE_DESC0_D2 |
				dir << VDE_DESC0_DIR |
				VDE_DESC_A0_AS_COMMAND << VDE_DESC0_A0;

			/* DESC2 */
			desc->desc2 = VDE_DESC2_STRIDE(cmd->stride) | 
				VDE_DESC2_IFMT(data_ifmt) | 
				VDE_DESC2_OFMT(data_ofmt);

			/* DESC3 */
			prev_desc->desc3 = VDE_DESC_LAST_MORE_DESCRIPTORS << VDE_DESC3_LAST|
                conf->eofi_use_vsync << VDE_DESC3_PAUSE |
                VDE_DESC_IRQ_DONT_GENERATE_INTERRUPT << VDE_DESC3_IRQ |
                VDE_DESC3_NEXT(phys_addr);

		} else { /* LCDBUS_CMDTYPE_CMD:*/

			/* DESC0 */
			desc->desc0 = VDE_DESC0_OFF(cmd->cmd) |
				VDE_DESC0_NY(cmd->len >> 11) |
				VDE_DESC0_NX(cmd->len & ((1 << 11) - 1)) |
				VDE_DESC_D2_DMA_LINEAR_ADDRESSING_1D << VDE_DESC0_D2 |
				dir << VDE_DESC0_DIR |
				VDE_DESC_A0_AS_COMMAND << VDE_DESC0_A0;

			/* DESC2 */
			desc->desc2 = VDE_DESC2_STRIDE(0) |
				VDE_DESC2_IFMT(cmd_ifmt) |
				VDE_DESC2_OFMT(cmd_ofmt);

			/* DESC3 */
			prev_desc->desc3 = VDE_DESC_LAST_MORE_DESCRIPTORS << VDE_DESC3_LAST|
                VDE_DESC_PAUSE_DONT_WAIT_FOR_EXTERNAL_SYNC << VDE_DESC3_PAUSE |
                VDE_DESC_IRQ_DONT_GENERATE_INTERRUPT << VDE_DESC3_IRQ |
                VDE_DESC3_NEXT(phys_addr);
		}

		/* Increment pointer to next desc */
		prev_desc = desc;
		desc++;
		phys_addr += sizeof(struct vde_desc);

	}

	/* last descriptor will generate the interrupt */
	prev_desc->desc3 = VDE_DESC_LAST_NO_MORE_DESCRIPTORS << VDE_DESC3_LAST |
				VDE_DESC_IRQ_GENERATE_INTERRUPT << VDE_DESC3_IRQ;

	return 0;
}

static int
vde_read(const struct device *dev, const struct list_head *commands)
{
	int ret = 0;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct vde_t *vde = dev_get_drvdata(dev->parent);
	struct lcdbus_conf *conf = &(vde->conf[ldev->disp_nr]);
	u32 regvalue;


	if (list_empty(commands)) {
		dev_info((struct device *)dev, "%s(): Got an empty command list\n",
			 __FUNCTION__);
		return 0;
	}

	/* Construct commands list */
	ret = vde_cmds_to_desc_chain(vde, commands, VDE_DESC_DIR_READ, conf);
	if (ret != 0) {
		goto out;
	}

	mutex_lock(&vde->io_mutex);

	/* enable clock */
	clk_enable(vde->clk);

	/* write desc start address */
	iowrite32(vde->cmds_list_phys, VDE_DESC_START_REG);

	/* write config register */
	regvalue = VDE_REG_CONF_ALIGN(conf->align) |
			VDE_REG_CONF_CLOCKDIV(vde->platform_data->clk_div) |
			VDE_REG_CONF_BURST(vde->platform_data->burst) | 
			VDE_REG_CONF_DISPLAY(ldev->disp_nr);

	iowrite32(regvalue, VDE_CONF_REG);

	/* enable vde */
	regvalue = ioread32(VDE_CTRL_REG);
	regvalue |= VDE_ENABLE_1;
	iowrite32(regvalue, VDE_CTRL_REG);

	if (!wait_for_completion_timeout(&vde->end_process, HZ)) {
		printk(KERN_ERR "%s(): command timeout\n", __FUNCTION__);
		ret = -EIO;
	}

	/* disable clock */
	clk_disable(vde->clk);

	mutex_unlock(&vde->io_mutex);

out:
	return ret;
}

static int
vde_write(const struct device *dev, const struct list_head *commands)
{
	int ret = 0;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct vde_t *vde = dev_get_drvdata(dev->parent);
	struct lcdbus_conf *conf = &(vde->conf[ldev->disp_nr]);
	u32 regvalue, ier, icr, isr, eofi_ctl;

	/* Check the commands list */
	if (list_empty(commands)) {
		dev_err((struct device *)dev, "Got an empty command list\n");
		return ret;
	}

	/* Make VDE commands list */
	ret = vde_cmds_to_desc_chain(vde, commands, VDE_DESC_DIR_WRITE, conf);
	if (ret != 0) {
		goto out;
	}

	mutex_lock(&vde->io_mutex);

	/* enable clock */
	clk_enable(vde->clk);

	/* write desc start address */
	iowrite32(vde->cmds_list_phys, VDE_DESC_START_REG);

	/* write config register */
	regvalue = VDE_REG_CONF_ALIGN(conf->align) |
			VDE_REG_CONF_CLOCKDIV(vde->platform_data->clk_div) |
			VDE_REG_CONF_BURST(vde->platform_data->burst) | 
			VDE_REG_CONF_DISPLAY(ldev->disp_nr);

	/* INT registers default value */
	icr = VDE_END_PROCESSING_1 | VDE_AHB_ERR_1 | VDE_DESC_ERR_1;
	ier = VDE_END_PROCESSING_1 | VDE_AHB_ERR_1 | VDE_DESC_ERR_1;
	isr = 0x0000;

	/* init conf, icr, ier and isr registers */
	iowrite32(regvalue, VDE_CONF_REG);

	iowrite32(icr, VDE_ICR_REG);
	iowrite32(isr, VDE_ISR_REG);
	iowrite32(ier, VDE_IEN_REG);

	/* configure the EOFI ctrl register */
	eofi_ctl = VDE_REG_EOFI_CTRL_POL(conf->eofi_pol) | 
			VDE_REG_EOFI_CTRL_SKIP(conf->eofi_skip) |
			VDE_REG_EOFI_CTRL_DEL(conf->eofi_del);
	
	iowrite32(eofi_ctl, VDE_EOFI_CTRL_REG);

	/* start the vde transfer */
	regvalue = ioread32(VDE_CTRL_REG);
	regvalue |= VDE_ENABLE_1;
	iowrite32(regvalue, VDE_CTRL_REG);

	/* Wait for the transfer to be completed */
	if (!wait_for_completion_timeout(&vde->end_process, HZ)) {
		printk(KERN_ERR "%s(): command timeout\n", __FUNCTION__);
		ret = -EIO;
	}

	/* disable clock */
	clk_disable(vde->clk);

	mutex_unlock(&vde->io_mutex);

out:
	return ret;
}

static int
vde_get_conf(const struct device *dev, struct lcdbus_conf *conf)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct vde_t *vde = dev_get_drvdata(dev->parent);

	/* Check display id */
	if (ldev->disp_nr < 0 ||
		ldev->disp_nr >= VDE_NUM_DISPLAYS) {
		dev_err((struct device *)dev,
				"Getting config for an invalid display (%d)\n", ldev->disp_nr);
		return -EINVAL;
	}

	mutex_lock(&vde->io_mutex);

	*conf = vde->conf[ldev->disp_nr];

	mutex_unlock(&vde->io_mutex);
	
	return 0;
}

static int
vde_set_conf(const struct device *dev, const struct lcdbus_conf *conf)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct vde_t *vde = dev_get_drvdata(dev->parent);
	u32 conf_reg = 0;
	int ret = -EINVAL;

	/* Check display id */
	if (ldev->disp_nr < 0 ||
		ldev->disp_nr >= VDE_NUM_DISPLAYS) {
		dev_err((struct device *)dev,
				"Setting config for an invalid display (%d)\n", ldev->disp_nr);
		return ret;
	}

	mutex_lock(&vde->io_mutex);
	
	/* Check data commands */
	if (conf->data_ofmt > LCDBUS_NUM_DATAFMTS ||
	    conf->data_ifmt > LCDBUS_NUM_DATAFMTS) {
		dev_err((struct device *)dev, "Invalid output color format (DATA) !");
		goto ko;
	}

	if (vde_data_ofmt[conf->data_ofmt] == -1 ||
		vde_data_ifmt[conf->data_ifmt] == -1) {
		dev_err((struct device *)dev, "Unsupported color format (DATA) !");
		ret = -EPERM;
		goto ko;
	}

	/* Check ctrl commands */
	if (conf->cmd_ofmt > LCDBUS_NUM_DATAFMTS ||
		conf->cmd_ifmt > LCDBUS_NUM_DATAFMTS) {
		dev_err((struct device *)dev, "Invalid output color format (CMD)!");
		goto ko;
	}
	
	if (vde_data_ofmt[conf->cmd_ofmt] == -1 ||
		vde_data_ifmt[conf->cmd_ifmt] == -1) {
		dev_err((struct device *)dev, "Unsupported color format (CMD) !");
		ret = -EPERM;
		goto ko;
	}

	/******************************/
	/* Set the CSKIP & BSWAP bits */

	/* enable clock */
	clk_enable(vde->clk);

	/* read conf register */
	switch(ldev->disp_nr) {
	case 0:
		conf_reg = ioread32(VDE_CONF1_REG);
		break;
	case 1:
		conf_reg = ioread32(VDE_CONF2_REG);
		break;
	}

	conf_reg |= VDE_REG_CONFD_CSKIP(conf->cskip) |
				VDE_REG_CONFD_BSWAP(conf->bswap);

	/* write conf1 or conf2 register */
	switch(ldev->disp_nr) {
	case 0:
		iowrite32(conf_reg, VDE_CONF1_REG);
		break;
	case 1:
		iowrite32(conf_reg, VDE_CONF2_REG);
		break;
	}

	/* disable clock */
	clk_disable(vde->clk);

	/******************************/
	/* Save display configuration */
	vde->conf[ldev->disp_nr] = *conf;

	ret = 0;
ko:
	mutex_unlock(&vde->io_mutex);

	return ret;
}

static int
vde_set_timing(const struct device *dev,
		const struct lcdbus_timing *timing)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct vde_t *vde = dev_get_drvdata(dev->parent);
	struct lcdbus_conf *conf;
	u32 conf_reg;

	/* Sanity check */
	if ((!dev) || (!timing)) {
		return -EINVAL;
	}

	if (ldev->disp_nr < 0 ||
		ldev->disp_nr >= VDE_NUM_DISPLAYS) {
		dev_err((struct device *)dev,
				"Setting timing for an invalid display (%d)\n", ldev->disp_nr);
		return -EINVAL;
	}

	mutex_lock(&vde->io_mutex);

	conf = &(vde->conf[ldev->disp_nr]);

	/* enable clock */
	clk_enable(vde->clk);

	conf_reg = VDE_REG_CONFD_MODE(timing->mode) |
			VDE_REG_CONFD_SER(timing->ser) |
			VDE_REG_CONFD_HOLD(timing->hold) |
			VDE_REG_CONFD_RH(timing->rh) |
			VDE_REG_CONFD_RC(timing->rc) |
			VDE_REG_CONFD_RS(timing->rs) |
			VDE_REG_CONFD_WH(timing->wh) |
			VDE_REG_CONFD_WC(timing->wc) |
			VDE_REG_CONFD_WS(timing->ws) |
			VDE_REG_CONFD_CH(timing->ch) |
			VDE_REG_CONFD_CS(timing->cs) |
			VDE_REG_CONFD_CSKIP(conf->cskip) |
			VDE_REG_CONFD_BSWAP(conf->bswap);

	/* write conf1 or conf2 register */
	switch(ldev->disp_nr) {
	case 0:
		iowrite32(conf_reg, VDE_CONF1_REG);
		break;
	case 1:
		iowrite32(conf_reg, VDE_CONF2_REG);
		break;
	}

	/* disable clock */
	clk_disable(vde->clk);

	mutex_unlock(&vde->io_mutex);

	return 0;
}

struct lcdbus_ops vde_ops = {
	.read       = vde_read,
	.write      = vde_write,
	.get_conf   = vde_get_conf,
	.set_conf   = vde_set_conf,
	.set_timing = vde_set_timing,
};

irqreturn_t
vde_irq(int irq, void *arg)
{
	struct vde_t *vde = (struct vde_t *)arg;
	u32 int_stat;
	irqreturn_t ret = IRQ_NONE;

	int_stat = ioread32(VDE_INT_STAT_REG);
	iowrite32(int_stat, VDE_ICR_REG);

	if (int_stat & VDE_END_PROCESSING_1) {
		complete(&vde->end_process);
		ret = IRQ_HANDLED;
	}
	else
	{
		if(int_stat == 0) {
			printk(KERN_ERR "%s(): bad clock management\n", __FUNCTION__);
		}
		else {
			if (int_stat & VDE_AHB_ERR_1) 
				printk(KERN_ERR "%s(): AHB error !\n", __FUNCTION__);

			if (int_stat & VDE_DESC_ERR_1) 
				printk(KERN_ERR "%s(): DESC error !\n", __FUNCTION__);
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

static int __devinit
vde_probe(struct platform_device *pdev)
{
	struct vde_t *vde;
	int i, ret = 0;
	u32 regvalue;

	pr_debug("%s()\n", __FUNCTION__);

	vde = kzalloc(sizeof(*vde), GFP_KERNEL);
	if (!vde) {
		printk(KERN_ERR "%s Failed ! (No more memory (vde))\n", __FUNCTION__);
		ret = -ENOMEM;
		goto out;
	}

	/* Prepare the vde cmds list */
	vde->cmds_list_max_size = LCDBUS_CMDS_LIST_LENGTH*sizeof(struct vde_desc);

	vde->cmds_list = (struct vde_desc *)dma_alloc_coherent(NULL, 
			vde->cmds_list_max_size, &(vde->cmds_list_phys), 
			GFP_KERNEL | GFP_DMA);

	if ((!vde->cmds_list) || (!vde->cmds_list_phys)){
		printk(KERN_ERR "%s Failed ! (No more memory (vde->cmds_list))\n", 
				__FUNCTION__);
		ret = -ENOMEM;
		goto err_free_vde;
	}

	pr_debug("VDE cmds_list_phys 0x%X\n", vde->cmds_list_phys);

	/* Set drvdata */
	platform_set_drvdata(pdev, vde);
	vde->device = &pdev->dev;

	/* grab VDE clock */
	vde->clk = clk_get(vde->device, "VDE");
	if (IS_ERR(vde->clk)) {
		printk(KERN_ERR "%s Failed ! (Could not get the clock of VDE)\n", 
				__FUNCTION__);
		ret = -ENXIO;
		goto err_free_cmds_list;
	}

	init_completion(&vde->end_process);
	mutex_init(&vde->io_mutex);

	/* get platform data */
	vde->platform_data = (struct vde_platform_data *)pdev->dev.platform_data;

	/* enable VDE clk */
	clk_enable(vde->clk);

	/* init control register , disable VDE */
	iowrite32(VDE_ENABLE_0, VDE_CTRL_REG);

	/* reset and wait for reset */
	iowrite32(VDE_RESET_1, VDE_CTRL_REG);

	/* check if hardware finished reset */
	while (ioread32(VDE_CTRL_REG) & (VDE_RESET_1));

	/* init desc register */
	iowrite32(0, VDE_DESC_START_REG);

	/* init config register */
	regvalue = VDE_REG_CONF_ALIGN(0) |
			VDE_REG_CONF_CLOCKDIV(vde->platform_data->clk_div) |
			VDE_REG_CONF_BURST(vde->platform_data->burst) |
			VDE_REG_CONF_DISPLAY(0);

	iowrite32(regvalue, VDE_CONF_REG);

	/* init icr register */
	regvalue = VDE_END_PROCESSING_1 | VDE_AHB_ERR_1 | VDE_DESC_ERR_1;
	iowrite32(regvalue, VDE_ICR_REG);

	/* init ier register  */
	regvalue = VDE_END_PROCESSING_1 | VDE_AHB_ERR_1 | VDE_DESC_ERR_1;
	iowrite32(regvalue, VDE_IEN_REG);

	/* setup devices */
	for (i = 0; i < VDE_NUM_DISPLAYS; i++) {
		vde->conf[i] = vde_def_config;
		vde->lcd[i].disp_nr = i;
		vde->lcd[i].ops = &vde_ops;
		vde->lcd[i].dev.parent = &pdev->dev;
		snprintf(vde->lcd[i].dev.bus_id, BUS_ID_SIZE, "%s-lcd%d",
			 pdev->dev.bus_id, i);
	}

	for (i = 0; i < VDE_NUM_DISPLAYS; i++) {
		ret = lcdctrl_device_register(&vde->lcd[i]);
		if (ret < 0) {
			printk(KERN_ERR "%s Failed! (Unable to register lcdctrl device %d)\n",
				   __FUNCTION__, ret);
			goto err_unreg_lcds;
		}
	}

	/* get & register irq */
	vde->irq = platform_get_irq(pdev, 0);
	if (vde->irq == 0) {
		printk(KERN_ERR "%s Failed ! (Could not get irq for VDE)\n", 
				__FUNCTION__);
		ret = -ENXIO;
		goto err_unreg_lcds;
	}

	ret = request_irq(vde->irq, vde_irq, IRQF_DISABLED, "vde", vde);
	if (ret) {
		printk(KERN_ERR "%s Failed ! (Could not register irq for VDE %d)\n", 
				__FUNCTION__, ret);
		ret = -ENXIO;
		goto err_unreg_lcds;
	}

	goto out;

err_unreg_lcds:
	for (i -= 1; i != 0; i--)
		lcdctrl_device_unregister(&vde->lcd[i]);

err_free_cmds_list:
	dma_free_coherent(NULL, vde->cmds_list_max_size, vde->cmds_list, 
			(dma_addr_t)vde->cmds_list_phys);

err_free_vde:
	kfree(vde);
	return ret;

out:
	/* Disable clock */
	clk_disable(vde->clk);

	return ret;
}

static int __devexit
vde_remove(struct platform_device *pdev)
{
	int i;
	struct vde_t *vde = platform_get_drvdata(pdev);

	pr_debug("%s()\n", __FUNCTION__);

	for (i = 0; i < VDE_NUM_DISPLAYS; i++)
		lcdctrl_device_unregister(&vde->lcd[i]);

	/* free the irq */
	free_irq(vde->irq, vde);

	/* disable clock */
	clk_put(vde->clk);

	/* free commands list */
	if (vde->cmds_list) {
		dma_free_coherent(NULL, 
				vde->cmds_list_max_size, 
				vde->cmds_list, 
				(dma_addr_t)vde->cmds_list_phys);
	}

	/* free vde structure */
	kfree(vde);

	return 0;
}

static struct platform_driver vde_driver = {
	.driver.name = "pnx-vde",
	.driver.owner = THIS_MODULE,
	.probe = vde_probe,
	.remove = vde_remove,
};

static int __init
vde_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);
	return platform_driver_register(&vde_driver);
}

static void __exit
vde_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);
	platform_driver_unregister(&vde_driver);
}

module_init(vde_init)
module_exit(vde_exit)

MODULE_AUTHOR("ST-ERICSSON");
MODULE_DESCRIPTION("Video Display Engine driver");
MODULE_LICENSE("GPL");

