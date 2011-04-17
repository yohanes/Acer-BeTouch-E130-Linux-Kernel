/*
 * ============================================================================
 *
 * Filename:     pnxmci.c
 *
 * Description:  
 *
 * Version:      1.0
 * Created:      10.04.2009 11:29:47
 * Revision:     none
 * Compiler:     gcc
 *
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
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
 * NOTE: (hw fixe)
 *   - For a read we need to write to the data ctrl register first, then
 *	   send the command. For writes, the order is reversed.
 *   - when you send a cmd that not check the crc ( without MMC_RSP_CRC flags
 *     example cmd41), the FCI ip does not move the cmd_send status but 
 *     cmd_crcfail status
 *   - performing a DMA AHB1 to AHB2 (and the reverse) has a higher performance
 *     than acting on the same AHB layer (AHB2 in our case).
 *     But there's a bug SWIFT_PR_CR 8964. Today, multi-layers DMA transfers
 *     are 4 times worse than mono layer.
 *     Today, this PR will not be solved for PRISM3A.
 *     DMAU is clocked @104 max., we're not sure the DMA monolayer will be
 *     enough to empty the data from the FCI.
 *
 * TODO:
 *   - use dma lli.
 * ============================================================================
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/pm_qos_params.h>

#include <mach/gpio.h>
#include <mach/mci.h>
#include <mach/dma.h>

#define ACER_L1_KX

#include "pnxmci.h"

#define DRIVER_NAME "pnxmci"
#define FCI_BUFFER_SZ SZ_64K

#define RESSIZE(ressource) (((ressource)->end - (ressource)->start)+1)

static void pnxmci_set_power(struct pnxmci_host *host,
		enum FCI_power_mode mode,int ios_vdd);
static int pnxmci_card_present(struct mmc_host *mmc);

/* ACER Jen chang, 2009/12/01, IssueKeys:AU4.B-1170, Separate sd card plug in/out detecting debounce time { */
#define plugin_debounce_time	2000 // 1 seconds
#define plugout_debounce_time	0 // 1 seconds
/* } ACER Jen Chang, 2009/12/01*/

/* 
 * ============================================================================
 * QOS & timer
 * ============================================================================
 */

static void pnxmci_pm_qos_down(struct pnxmci_host *host)
{
	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT, DRIVER_NAME,
			PM_QOS_DEFAULT_VALUE);
	pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT, DRIVER_NAME,
			PM_QOS_DEFAULT_VALUE);
	host->pm_qos_status=PNXMCI_PM_QOS_DOWN;

	if (clk_set_rate(host->fc_clk,0))
		dev_err(&host->pdev->dev,"failed to setrate fc_clk\n");

	if (host->mmc->ios.power_mode!=MMC_POWER_OFF)
		pnxmci_set_power(host,FCI_ECO,host->mmc->ios.vdd);
}

static void pnxmci_pm_qos_up(struct pnxmci_host *host)
{
	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	pnxmci_set_power(host,FCI_ON,host->mmc->ios.vdd);

	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT, DRIVER_NAME,
			PM_QOS_HCLK2_MCI_THROUGHPUT);
	pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT, DRIVER_NAME,
			PM_QOS_DDR_MCI_THROUGHPUT);
	host->pm_qos_status=PNXMCI_PM_QOS_UP;
	host->stat.nb_pm_qos_up++;

	if (clk_set_rate(host->fc_clk,host->fc_clk_rate))
		dev_err(&host->pdev->dev,"failed to setrate fc_clk to:%ld\n",
			host->fc_clk_rate);
}

static void pnxmci_pm_qos_refresh(struct pnxmci_host *host)
{
	int ret;

	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	if (host->pm_qos_status==PNXMCI_PM_QOS_DOWN)
		pnxmci_pm_qos_up(host);

	/* timeout calibrated 500ms */
	ret = schedule_delayed_work(&host->qos_work, HZ>>1);
	if (!ret) {
		cancel_delayed_work(&host->qos_work);
		schedule_delayed_work(&host->qos_work,HZ>>1);
	}
}

static void pnxmci_wq_qos_timeout( struct work_struct *work)
{
	struct pnxmci_host *host = 
		container_of(work, struct pnxmci_host, qos_work.work);

	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	if (host->complete_what==COMPLETION_NONE)
		pnxmci_pm_qos_down(host);
	else
		pnxmci_pm_qos_refresh(host);
}
/* 
 * ============================================================================
 * private functions
 * ============================================================================
 */
/* fc_clk setrate (sort max to min) */
static unsigned long fc_clk_pt_func[] = {
	104000000, 78000000, 52000000, 26000000, 400000
};


static int pnxmci_mng_clkrate(struct pnxmci_host *host,
		unsigned int clock)
{
	int i=ARRAY_SIZE(fc_clk_pt_func)-1;
	int err=0;

	/* to protected the division by zero */
	clock=clock ? clock : 1;

	do{
		if ( clock*2 < fc_clk_pt_func[i])
			break;
		i--;
	}while(i>0);

#if !defined (ACER_L1_KX)
/* ACER Jen chang, 2009/12/07, IssueKeys:AU4.FC-498, Modify the default clock from 52Mhz to 39Mhz. { */
	if(i == 0)
		i = 1;
/* } ACER Jen Chang, 2009/12/07*/
#endif

/* ACER Jen chang, 2009/12/07, IssueKeys:AU4.FC-498, Add to adjust sd card clock by sysfs { */
	if(host->clk_select > 0)
	{
		if(host->clk_select > i)
			i = host->clk_select;

		host->clk_select = 0;
	}
/* } ACER Jen Chang, 2009/12/07*/

	if ( fc_clk_pt_func[i] != host->fc_clk_rate ){
		/* set fc_clk */
		err=clk_set_rate(host->fc_clk,fc_clk_pt_func[i]);
		if (err){
			dev_err(&host->pdev->dev,"failed to setrate fc_clk\n");
			goto out;
		}
		host->fc_clk_rate=clk_get_rate(host->fc_clk);
		if (IS_ERR_VALUE(host->fc_clk_rate)){
			dev_err(&host->pdev->dev,"failed to getrate fc_clk\n");
			err=-EINVAL;
			goto out;
		}
	}
	if ( 2*clock > host->fc_clk_rate)
		host->fci_clk_div=0;
	else{	
		host->fci_clk_div = DIV_ROUND_UP((host->fc_clk_rate
					/(2*clock))-1, 1);
		if (host->fci_clk_div > 255)
			host->fci_clk_div=255;
	}	
	host->fci_clk_rate = host->fc_clk_rate/(2*(host->fci_clk_div+1));

out:
	return err; 
}

static inline void enable_imask(struct pnxmci_host *host)
{
	writel(host->fci_mask, host->base + FCI_MASK_OFFSET);
}
static inline void disable_imask(struct pnxmci_host *host)
{
	writel(0, host->base + FCI_MASK_OFFSET);
}

static inline void clear_imask(struct pnxmci_host *host)
{
	host->fci_mask=0;
	writel(host->fci_mask, host->base + FCI_MASK_OFFSET);
}

/* set power voltage on mci bus */
static const unsigned short mmc_ios_bit_to_vdd[] = {
	1500,	1550,	1600,	1650,	1700,	1800,	1900,	2000,
	2100,	2200,	2300,	2400,	2500,	2600,	2700,	2800,
	2900,	3000,	3100,	3200,	3300,	3400,	3500,	3600
};

static void pnxmci_set_power(struct pnxmci_host *host,
		enum FCI_power_mode mode,int ios_vdd)
{
	int err;
	
	dev_dbg(&host->pdev->dev, "[%s] mode:%d vddbit:0x%x value:%d\n",
			__func__, mode, ios_vdd,
			mmc_ios_bit_to_vdd[ios_vdd-1]);

	if ( host->pdata->set_power != NULL ){
		err = host->pdata->set_power(mode,
				mmc_ios_bit_to_vdd[ios_vdd-1]);
		if ( err < 0 )
			dev_err(&host->pdev->dev,
					"failed"
					"to set voltage (%dmV) error:%d\n",
					mmc_ios_bit_to_vdd[ios_vdd-1],err);
	}	
}

static inline int pnxmci_setup_dma(struct pnxmci_host *host,
		struct mmc_data *data)
{
	int err;

	memset(&(host->cfg), 0, sizeof(host->cfg));

	if (data->flags & MMC_DATA_READ){
		host->cfg.dest_addr=host->buffer_dma;
		host->cfg.src_addr=host->mem->start+FCI_FIFO_OFFSET;
		host->ch_cfg.flow_cntrl = FC_PER2MEM_PER;
		host->ch_cfg.src_per = PER_FCI;
		host->ch_cfg.dest_per = 0;
		host->ch_ctrl.di = 1;
		host->ch_ctrl.si = 0;
	}else if (data->flags & MMC_DATA_WRITE){
		host->cfg.dest_addr= host->mem->start+FCI_FIFO_OFFSET;
		host->cfg.src_addr= host->buffer_dma;
		host->ch_cfg.flow_cntrl= FC_MEM2PER_PER;
		host->ch_cfg.src_per = 0;
		host->ch_cfg.dest_per = PER_FCI;
		host->ch_ctrl.di = 0;
		host->ch_ctrl.si = 1;
		/* normaly, flush do in sg_copy by data provider (queue.c) */
		sg_copy_to_buffer(data->sg,data->sg_len,host->buffer,
				host->fci_datalenght);
	}else{
		err = -EINVAL;
		goto out;
	}

	err = pnx_dma_pack_config(&(host->ch_cfg), &(host->cfg.ch_cfg));
	if (err < 0)
		goto out;

	err = pnx_dma_pack_control(&(host->ch_ctrl), &(host->cfg.ch_ctrl));
	if (err < 0)
		goto out;

	err = pnx_config_channel(host->dma_ch, &(host->cfg));

out:
	dev_dbg(&host->pdev->dev,"[%s] config dma transfert: len:%d\n",
			__func__, host->fci_datalenght);
	return err;
}

static int pnxmci_prepare_data_xfer(struct pnxmci_host *host,
		struct mmc_data *data)
{
	if (host->dodma)
		return pnxmci_setup_dma(host, data);
	
	dev_err(&host->pdev->dev,"data xfer without dma, not implemented\n");
	return -EINVAL;
}

static void pnxmci_stop_data(struct pnxmci_host *host)
{
	writel(0, host->base + FCI_DATACTRL_OFFSET);
	clear_imask(host);
}

static void pnxmci_send_command(struct pnxmci_host *host)
{
	writel(host->fci_argument, host->base + FCI_ARGUMENT_OFFSET);
	writel(host->fci_command, host->base + FCI_COMMAND_OFFSET);
}

static int pnxmci_start_data(struct pnxmci_host *host)
{
	int err=0;

	if ( host->dodma ){
		err=pnx_dma_ch_enable(host->dma_ch);
		if (err)
			goto out;
	}
	writel(host->fci_datatimer, host->base + FCI_DATATIMER_OFFSET );
	writel(host->fci_datalenght, host->base + FCI_DATALENGTH_OFFSET );
	writel(host->fci_datactrl, host->base + FCI_DATACTRL_OFFSET );

out:
	return err;
}

static int pnxmci_prepare_data(struct pnxmci_host *host, struct mmc_data *data)
{
	unsigned int cycle_ns;
	int blksz_bits;

	dev_dbg(&host->pdev->dev,
			"[%s] blksz:%d nb_blk:%d sg_len:%d ptr_sg:%p\n",
			__func__, data->blksz, data->blocks,
			data->sg_len, data->sg);

	/* fci_datactrl
	 * blk size: power of 2
	 * mode: block data transfer
	 * direction: read or write
	 * data transfer enable */
	blksz_bits = ffs(data->blksz) - 1;
	if (1 << blksz_bits != data->blksz)
		return -EINVAL; 	

	host->fci_datactrl = (ffs(data->blksz)-1) << FCI_BLOCKSIZE_SHIFT;
	if (host->dodma)
		host->fci_datactrl |= FCI_DMAENABLE_ON;

	if ((data->flags & MMC_DATA_READ))
		host->fci_datactrl |= FCI_DIRECTION_CARD_TO_CONTROLLER;

	host->fci_datactrl |= FCI_DATATRANSFEREN;

	/* fci_datalenght
	 * data length
	 * mode:block data transfer 
	 */
	host->fci_datalenght = data->blksz * data->blocks;

	/* fci_datatimer
	 * data timer */
	cycle_ns=1000000000/host->fci_clk_rate;
	host->fci_datatimer=data->timeout_ns / cycle_ns;
	host->fci_datatimer+=data->timeout_clks;

	/* irq mask */
	host->fci_mask|=FCI_DATACRCFAILCLR|FCI_DATATIMEOUTCLR|
		FCI_TXUNDERRUNCLR|FCI_RXOVERRUNCLR|FCI_DATAENDCLR|
		FCI_STARTBITERRCLR;

	return 0;
}

static void pnxmci_prepare_command(struct pnxmci_host *host,
		struct mmc_command *cmd)
{
	host->fci_mask |= FCI_CMDTIMEOUTCLR | FCI_CMDCRCFAILCLR;
	
	if (cmd->data)
		host->complete_what = COMPLETION_XFERFINISH_RSPFIN;
	else if (cmd->flags & MMC_RSP_PRESENT)
		host->complete_what = COMPLETION_RSPFIN;
	else
		host->complete_what = COMPLETION_CMDSENT;

	host->fci_argument=cmd->arg;
	
	host->fci_command = cmd->opcode | FCI_ENABLE;
	
	if (cmd->flags & MMC_RSP_PRESENT){
		host->fci_command |= FCI_RESPONSE;
		host->fci_mask |= FCI_CMDRESPENDCLR;
	} else
		host->fci_mask |= FCI_CMDSENTCLR;

	if (cmd->flags & MMC_RSP_136)
		host->fci_command |= FCI_LONGRSP;	
}

static void pnxmci_send_request(struct mmc_host *mmc)
{
	struct pnxmci_host *host = mmc_priv(mmc);
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd_is_stop ? mrq->stop : mrq->cmd;
	int res=0;
	
	/* Clear fci status and mask registers */
	writel(0xFFFF, host->base + FCI_CLEAR_OFFSET);
	clear_imask(host);

	if (cmd->data){
		res = pnxmci_prepare_data(host, cmd->data);
		if (res) {
			dev_err(&host->pdev->dev,
					"data prepare error %d.\n", res);
			goto err_data;
		}
		res = pnxmci_prepare_data_xfer(host, cmd->data);
		if (res) {
			dev_err(&host->pdev->dev,
					"data prepare xfer error %d.\n", res);
			goto err_data;
		}	
		if (cmd->data->flags & MMC_DATA_READ) {
			res = pnxmci_start_data(host);
			if (res) {
				dev_err(&host->pdev->dev,
					"error: start data from device %d.\n",
					res);
				goto err_data;
			}
		}
	}
	
    pnxmci_prepare_command(host, cmd);
	pnxmci_send_command(host);
	enable_imask(host);

	/* Enable Interrupt */
	enable_irq(host->irq);
	return;

err_data:
	cmd->error=res;
	cmd->data->error=res;
	mmc_request_done(mmc, mrq);
	return;
}

static void pnxmci_finalize_request(struct pnxmci_host *host)
{
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd_is_stop ? mrq->stop : mrq->cmd;
	
	if (!mrq){
		dev_err(&host->pdev->dev,"request Missing!\n");
		return;
	}	
	
	/* Read response from controller. */
	cmd->resp[0] = readl(host->base + FCI_RESPONSE0_OFFSET);
	cmd->resp[1] = readl(host->base + FCI_RESPONSE1_OFFSET);
	cmd->resp[2] = readl(host->base + FCI_RESPONSE2_OFFSET);
	cmd->resp[3] = readl(host->base + FCI_RESPONSE3_OFFSET);

	/* Cleanup controller */
	writel(0,host->base + FCI_COMMAND_OFFSET);
	writel(0,host->base + FCI_ARGUMENT_OFFSET);
	writel(0xFFFF, host->base + FCI_CLEAR_OFFSET);
	clear_imask(host);
	
	if (cmd->data && cmd->error)
		cmd->data->error = cmd->error;

	/* If we have no data transfer we are finished here */
	if (!mrq->data)
		goto request_done;

	pnxmci_stop_data(host);

	if (cmd->data && cmd->data->stop && (!host->cmd_is_stop)) {
		host->cmd_is_stop = 1;
		pnxmci_send_request(host->mmc);
		return;
	}

	if (host->dodma)
		pnx_dma_ch_disable(host->dma_ch);
	
	/* Calulate the amout of bytes transfer if there was no error */
	if (mrq->data->error == 0) {
		mrq->data->bytes_xfered = mrq->data->blocks * mrq->data->blksz;
		if (mrq->data->flags & MMC_DATA_READ)
			sg_copy_from_buffer(mrq->data->sg,mrq->data->sg_len,
					host->buffer,host->fci_datalenght);
	} else
		mrq->data->bytes_xfered = 0;

request_done:
	host->complete_what = COMPLETION_NONE;
	host->mrq = NULL;
	mmc_request_done(host->mmc, mrq);
	clk_disable(host->fci_clk);
	dev_dbg(&host->pdev->dev, "[%s] request done\n", __func__);
	return;
}

/* 
 * ============================================================================
 * Sysfs
 * ============================================================================
 */
static ssize_t pnxmci_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct pnxmci_host	*host = mmc_priv(mmc);
	char *b = buf;
	
	b += sprintf(b, "\033[1;34mPnxmci Statistic\033[0m\n");

	b += sprintf(b, "%-20s:%ld\n",
			"nb core requetes", host->stat.nb_core_req);
	b += sprintf(b, "%-20s:%ld\n",
			"nb rx overrun", host->stat.nb_rxoverrun);
	b += sprintf(b, "%-20s:%ld\n",
			"nb tx underrun", host->stat.nb_txunderrun);
	b += sprintf(b, "%-20s:%ld\n",
			"nb datacrcfail", host->stat.nb_datacrcfail);
	b += sprintf(b, "%-20s:%ld\n",
			"nb datatimeout", host->stat.nb_datatimeout);
	b += sprintf(b, "%-20s:%ld\n",
			"nb startbiterr", host->stat.nb_startbiterr);
	b += sprintf(b, "%-20s:%ld\n",
			"nb dma callback err", host->stat.nb_dma_err);
	b += sprintf(b, "%-20s:%ld\n",
			"nb up pm_qos", host->stat.nb_pm_qos_up);

	b += sprintf(b, "\n\033[1;34mPnxmci Status\033[0m\n");
	b += sprintf(b,"%-20s:", "completion pending");
	switch ( host->complete_what ) {
	case COMPLETION_NONE:
		b += sprintf(b, "COMPLETION_NONE");
		break;
	case COMPLETION_FINALIZE:
		b += sprintf(b, "COMPLETION_FINALIZE");
		break;
	case COMPLETION_CMDSENT:
		b += sprintf(b, "COMPLETION_CMDSENT");
		break;
	case COMPLETION_RSPFIN:
		b += sprintf(b, "COMPLETION_RSPFIN");
		break;
	case COMPLETION_XFERFINISH:
		b += sprintf(b, "COMPLETION_XFERFINISH");
		break;
	case COMPLETION_XFERFINISH_RSPFIN:
		b += sprintf(b, "COMPLETION_XFERFINISH_RSPFIN");
		break;
	default:
		b += sprintf(b, "warning not define");
		break;
	}
	b += sprintf(b, "\n");

	b += sprintf(b,"%-20s:", "power qos");
	switch ( host->pm_qos_status ) {
	case PNXMCI_PM_QOS_DOWN:
		b += sprintf(b, "PNXMCI_PM_QOS_DOWN");
		break;
	case PNXMCI_PM_QOS_UP:
		b += sprintf(b, "PNXMCI_PM_QOS_UP");
		break;
	default:
		b += sprintf(b, "warning not define");
		break;
	}
	b += sprintf(b, "\n");

	return b - buf;
}

static ssize_t pnxmci_stat_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct pnxmci_host	*host = mmc_priv(mmc);
	
	memset(&(host->stat), 0, sizeof(host->stat));

	return count;
}
static DEVICE_ATTR(stat, S_IWUSR | S_IRUGO,
		pnxmci_stat_show, pnxmci_stat_reset);

/* ACER Jen chang, 2009/12/07, IssueKeys:AU4.FC-498, Add to adjust sd card clock by sysfs { */
static ssize_t pnxmci_clkselect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct pnxmci_host	*host = mmc_priv(mmc);
	char *b = buf;
	
	b += sprintf(b, "\033[1;34mPnxmci clock range:\033[0m\n");
	b += sprintf(b,"1:52MHz\n");
	b += sprintf(b,"2:39MHz\n");
	b += sprintf(b,"3:26MHz\n");
	b += sprintf(b,"4:13MHz\n");

	b += sprintf(b, "\033[1;34mNow clock is:%ld\033[0m\n", host->fci_clk_rate);

	return b - buf;
}

static ssize_t pnxmci_clkselect_set(struct device *dev,
		struct device_attribute *attr, char *buf, size_t count)
{
	struct mmc_host *mmc = container_of(dev, struct mmc_host, class_dev);
	struct pnxmci_host	*host = mmc_priv(mmc);
	unsigned int clk_set = simple_strtoul(buf, NULL, 10);
	unsigned long clk_rate[] = {52000000, 39000000, 26000000, 13000000};

	if(clk_set < 1 || clk_set > 4)
		printk("Input range error, please cat clk_select to query!!\n");
	else
	{
		printk("--clk_set=%d--\n", clk_set);
		if(clk_rate[clk_set - 1] <= host->fci_clk_rate)
		{
			host->clk_select = clk_set - 1;
			pnxmci_mng_clkrate(host,mmc->ios.clock);
		}
	}

	return count;
}
static DEVICE_ATTR(clk_select, S_IWUSR | S_IRUGO,pnxmci_clkselect_show,pnxmci_clkselect_set);
/* } ACER Jen Chang, 2009/12/07*/

/* 
 * ============================================================================
 * Entry points between mmc_core and pnxmci driver see mmc_pnx_ops
 * ============================================================================
 */
static void pnxmci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct pnxmci_host *host = mmc_priv(mmc);
	host->cmd_is_stop = 0;
	host->mrq = mrq;

	if ( !pnxmci_card_present(mmc) ) {
		dev_dbg(&host->pdev->dev, "[%s]no medium\n", __func__);
		host->mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
	} else {
		host->complete_what = COMPLETION_REQ;
		clk_enable(host->fci_clk);
		host->stat.nb_core_req++;
		pnxmci_pm_qos_refresh(host);
		pnxmci_send_request(mmc);
	}
}

/* set power, clock, busmode, timming hs
 *	power_up 
 */
static void pnxmci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct pnxmci_host *host = mmc_priv(mmc);
	u32 reg_pwr=0, reg_clk=0;

	dev_dbg(&host->pdev->dev,
			"[%s] clock %uHz busmode %u powermode %u Vdd %u\n",
			__func__, ios->clock, ios->bus_mode,
			ios->power_mode, ios->vdd);

	pnxmci_mng_clkrate(host,ios->clock);
	reg_clk=host->fci_clk_div;

	if(ios->bus_width == MMC_BUS_WIDTH_4)
		reg_clk |=FCI_WIDEBUS_1; /* set fci bus with 4 */

	switch (ios->power_mode){
		case MMC_POWER_UP:
			pnxmci_set_power(host,FCI_ECO,ios->vdd);
			reg_pwr |= FCI_CTRL_POWERUP;
			break;
		case MMC_POWER_ON:
			reg_pwr |= FCI_CTRL_POWERON;
			reg_clk |= FCI_CLKEN_ENABLED;
		/* reg_clk |= FCI_PWRSAVE_ENABLED;*/ /* FIXME try it */
			break;
		case MMC_POWER_OFF:
			pnxmci_set_power(host,FCI_OFF,ios->vdd);
			reg_pwr &= ~FCI_CTRL_POWERON;
			break;
	}

	if ( ios->timing == MMC_TIMING_LEGACY )
		reg_pwr |= FCI_PEDGE_FALLING;
	else /* MMC_TIMING_MMC_HS || MMC_TIMING_SD_HS */
		reg_pwr |= FCI_PEDGE_RISING;

	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN)
		reg_pwr |= FCI_OPENDRAIN;

	clk_enable(host->fci_clk);
	writel(reg_pwr, host->base + FCI_POWER_OFFSET);
	writel(reg_clk, host->base + FCI_CLOCK_OFFSET);

	dev_dbg(&host->pdev->dev,
			"[%s] "
			"fc_ck:%ldHz fci_clk:%ldHz reg_pwr:%08x reg_clk:%08x\n",
			__func__, host->fc_clk_rate, host->fci_clk_rate,
			reg_pwr, reg_clk);
	clk_disable(host->fci_clk);
}
/*
 * Return values for the get_ro callback should be:
 *       0 for a read/write card
 *       1 for a read-only card
 *       or a negative errno value when something bad happened
 */
static int pnxmci_get_ro(struct mmc_host *mmc)
{
	/* micro sd: do not has wp switch so always read/write */
	/* FIXME try with -ENOSYS */
	return 0; 
}
/*
 * Return values for the get_cd callback should be:
 *       0 for a absent card
 *       1 for a present card
 *       or a negative errno value when something bad happened
 */
static int pnxmci_card_present(struct mmc_host *mmc)
{
	struct pnxmci_host *host =  mmc_priv(mmc);
	int ret;

	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	ret = gpio_get_value(EXTINT_TO_GPIO(host->irq_cd)) ? 0 : 1;
	return ret ^ host->pdata->detect_invert;
}

/* 
 * ============================================================================
 * IRQ callback, FCI and card detect
 * ============================================================================
 */
static irqreturn_t pnxmci_irq_cd(int irq, void *dev_id)
{
	struct pnxmci_host *host = (struct pnxmci_host *)dev_id;

	dev_dbg(&host->pdev->dev, "[%s]\n", __func__);

	/* ACER Jen chang, 2009/12/01, IssueKeys:AU4.B-1170, Separate sd card plug in/out detecting debounce time { */
	if (pnxmci_card_present(host->mmc))
	{
		printk(KERN_DEBUG "-plug in card-\n");
		mmc_detect_change(host->mmc, msecs_to_jiffies(plugin_debounce_time));
	}
	else
	{
		printk(KERN_DEBUG "-plug out card-\n");
		mmc_detect_change(host->mmc, msecs_to_jiffies(plugout_debounce_time));
	}
	/* } ACER Jen Chang, 2009/12/01*/
	return IRQ_HANDLED;
}

/*
 * ISR for SDI Interface IRQ
 * Communication between driver and ISR works as follows:
 *   host->mrq 			points to current request
 *   host->complete_what	Indicates when the request is considered done
 *     COMPLETION_CMDSENT           when the command was sent
 *     COMPLETION_RSPFIN            when a response was received
 *     COMPLETION_XFERFINISH        when the data transfer is finished
 *     COMPLETION_XFERFINISH_RSPFIN both of the above.
 */
static irqreturn_t pnxmci_irq(int irq, void *dev_id)
{
	struct pnxmci_host *host = dev_id;
	struct mmc_command *cmd;
	u32 mci_status,mci_cclear=0, mci_mask;
	unsigned long iflags;	

	spin_lock_irqsave(&host->complete_lock, iflags);
	mci_status = readl(host->base + FCI_STATUS_OFFSET );
	mci_mask = readl(host->base + FCI_MASK_OFFSET );

	if ( !(mci_status & mci_mask)){
		dev_dbg(&host->pdev->dev,"strange interrupt\n");
		clear_imask(host);
		goto irq_out;
	}
	
	if ((host->complete_what == COMPLETION_NONE) ||
	    (host->complete_what == COMPLETION_FINALIZE)) {
		dev_dbg(&host->pdev->dev,"nothing to complete\n");
		clear_imask(host);
		goto irq_out;
	}
	if (!host->mrq) {
		dev_dbg(&host->pdev->dev,"no active mrq\n");
		clear_imask(host);
		goto irq_out;
	}

	cmd = host->cmd_is_stop ? host->mrq->stop : host->mrq->cmd;

	if (!cmd) {
		dev_dbg(&host->pdev->dev,"no active cmd\n");
		clear_imask(host);
		goto irq_out;
	}

	if ( mci_status & (FCI_CMDTIMEOUT|FCI_CMDCRCFAIL) ){
		if (mci_status & FCI_CMDTIMEOUT) {
			dev_dbg(&host->pdev->dev,"error: CMDTIMEOUT\n");
			cmd->error = -ETIMEDOUT;
		}
		if (mci_status & FCI_CMDCRCFAIL) {
			if ( !(cmd->flags & MMC_RSP_CRC) ) {
				/* When you send a cmd that not check the crc
				 * ( without MMC_RSP_CRC flags example cmd41),
				 * the FCI ip does not move the cmd_send
				 * status but cmd_crcfail status */
				dev_dbg(&host->pdev->dev,
						"ok: hw ip bug respsend "
						"never send ex:CDM41\n");
				goto close_transfer;
			}
			dev_dbg(&host->pdev->dev,"error: CMDCRCFAIL\n");
			cmd->error = -EILSEQ;
		}
		goto fail_transfer;
	}

	if (mci_status & FCI_CMDSENT) {
		if (host->complete_what == COMPLETION_CMDSENT) {
			dev_dbg(&host->pdev->dev,"ok: command sent\n");
			goto close_transfer;
		}
		mci_cclear |= FCI_CMDSENTCLR;
	}
	
	if (mci_status & FCI_CMDRESPEND) {
		if (host->complete_what == COMPLETION_RSPFIN) {
			dev_dbg(&host->pdev->dev,
					"ok: command response received\n");
			goto close_transfer;
		}

		if (host->complete_what == COMPLETION_XFERFINISH_RSPFIN){
			dev_dbg(&host->pdev->dev,
					"command response received, "
					"wait data end\n");
			host->complete_what = COMPLETION_XFERFINISH;
			if (cmd->data->flags & MMC_DATA_WRITE){
				if (pnxmci_start_data(host)){
					dev_dbg(&host->pdev->dev,
							"error: start data "
							"to device\n");
					cmd->data->error = -EIO;
					goto fail_transfer;
				}
			}	
		}
		mci_cclear |= FCI_CMDRESPENDCLR;
	}
	
	/* errors handled after this point are only relevant
	   when a data transfer is in progress */
	if (!cmd->data)
		goto clear_status_bits;
	
	if ( mci_status & (FCI_DATACRCFAIL|FCI_DATATIMEOUT|FCI_RXOVERRUN
				|FCI_TXUNDERRUN|FCI_STARTBITERR)){
		if (mci_status & FCI_DATACRCFAIL) {
			dev_dbg(&host->pdev->dev,"error: DATACRCFAIL\n");
			host->stat.nb_datacrcfail++;
			cmd->data->error = -EILSEQ;
		}
		if (mci_status & FCI_DATATIMEOUT) {
			dev_dbg(&host->pdev->dev,"error: DATATIMEOUT\n");
			host->stat.nb_datatimeout++;
			cmd->data->error = -ETIMEDOUT;
		}
		if (mci_status & FCI_RXOVERRUN) {
			dev_dbg(&host->pdev->dev,"error: RXOVERRUN\n");
			host->stat.nb_rxoverrun++;
			cmd->data->error = -EIO;
		}
		if (mci_status & FCI_TXUNDERRUN) {
			dev_dbg(&host->pdev->dev,"error: TXUNDERRUN\n");
			host->stat.nb_txunderrun++;
			cmd->data->error = -EIO;
		}
		if (mci_status & FCI_STARTBITERR) {
			dev_dbg(&host->pdev->dev,"error: STARTBITERR\n");
			host->stat.nb_startbiterr++;
			cmd->data->error = -EIO;
		}
		goto fail_transfer;
	}

	if ( mci_status & FCI_DATAEND) {
		/* data block sent/received (CRC check passed) */
		if (host->complete_what == COMPLETION_XFERFINISH) {
			dev_dbg(&host->pdev->dev,
					"ok: data transfer completed\n");
			goto close_transfer;
		}
		if (host->complete_what == COMPLETION_XFERFINISH_RSPFIN){
			dev_dbg(&host->pdev->dev,
					"data transfer completed, "
					"wait cmd respend\n");
			host->complete_what = COMPLETION_RSPFIN;
		}
		mci_cclear |= FCI_DATAENDCLR;
	}

clear_status_bits:
	writel(mci_cclear, host->base + FCI_CLEAR_OFFSET );
	goto irq_out;

fail_transfer:
close_transfer:
	host->complete_what = COMPLETION_FINALIZE;
	clear_imask(host);
	tasklet_schedule(&host->pnxmci_tasklet);
	
irq_out:
	dev_dbg(&host->pdev->dev,
			"[%s] mci_status:%04x\n", __func__, mci_status);
	spin_unlock_irqrestore(&host->complete_lock, iflags);
	return IRQ_HANDLED;
}
/* 
 * ============================================================================
 * DMA callback
 * ============================================================================
 */
static void pnxmci_dma_callback(int channel, int cause, void* context)
{
	struct pnxmci_host *host = (struct pnxmci_host *) context;
	unsigned long iflags;

	spin_lock_irqsave(&host->complete_lock, iflags);
	if (cause!=DMA_ERR_INT) {
		dev_dbg(&host->pdev->dev,
				"[%s] spurious dma callback\n", __func__);
		return;
	}
	dev_dbg(&host->pdev->dev, "[%s] dma requete error.\n", __func__);
	host->complete_what = COMPLETION_FINALIZE;
	host->mrq->data->error = -EINVAL;
	host->stat.nb_dma_err++;

	tasklet_schedule(&host->pnxmci_tasklet);
	spin_unlock_irqrestore(&host->complete_lock, iflags);
}

/* 
 * ============================================================================
 * Tasklet
 * ============================================================================
 */
static void pnxmci_tasklet(unsigned long data)
{
	struct pnxmci_host *host = (struct pnxmci_host *) data;

	disable_irq(host->irq);

	if  (host->complete_what == COMPLETION_FINALIZE ) {
		pnxmci_finalize_request(host);
	}else{
		/* wait fci completion */
		enable_irq(host->irq);
	}
}
/* 
 * ============================================================================
 * Init,register,unregister host functions to core mmc 
 * ============================================================================
 */
/*
 *	pnxmci_dma_init
 *	  initialization of common parameters
 */
static void pnxmci_dma_init(struct pnxmci_host *host)
{
	/* common parameters pnx_dma_ch_config */
	host->ch_cfg.halt = 0;
	host->ch_cfg.active = 1;
	host->ch_cfg.lock = 0;
	host->ch_cfg.itc = 0;
	host->ch_cfg.ie = 1;
	/* variable: flow_cntrl, src_per, dest_per */

	/* common parameters pnx_dma_ch_ctrl */
	/* FIXME fix hw dma bug
	 * dma ip can be use 2 ahb bus (source <=> destination)
	 * however, dma ip has a bug and this use case is more slower 
	 * than source and destination on the same ahb bus   
	 */
	host->ch_ctrl.src_ahb1 = 1;
	host->ch_ctrl.dest_ahb1 = 1;
	host->ch_ctrl.tc_mask = 1;
	host->ch_ctrl.cacheable = 0;
	host->ch_ctrl.bufferable = 0;
	host->ch_ctrl.priv_mode = 1;
	host->ch_ctrl.dbsize = 8; /* 8 */
	host->ch_ctrl.sbsize = 8; /* 8 */
	host->ch_ctrl.dwidth = WIDTH_WORD;
	host->ch_ctrl.swidth = WIDTH_WORD;
	/* variable: di, si, tr_size */	
}

static int pnxmci_initfci (struct pnx_mci_pdata *pdata, const char *name)
{
	int err;
	
	err =  gpio_request(pdata->gpio_fcicmd, name);
	err |= gpio_request(pdata->gpio_fciclk, name);
	err |= gpio_request(pdata->gpio_data0, name);
	err |= gpio_request(pdata->gpio_data1, name);
	err |= gpio_request(pdata->gpio_data2, name);
	err |= gpio_request(pdata->gpio_data3, name);

	if (err)
		return -1; 

	pnx_gpio_set_mode(pdata->gpio_fcicmd, pdata->gpio_mux_fcicmd);
	pnx_gpio_set_mode(pdata->gpio_fciclk, pdata->gpio_mux_fciclk);
	pnx_gpio_set_mode(pdata->gpio_data0, pdata->gpio_mux_data0);
	pnx_gpio_set_mode(pdata->gpio_data1, pdata->gpio_mux_data1);
	pnx_gpio_set_mode(pdata->gpio_data2, pdata->gpio_mux_data2);
	pnx_gpio_set_mode(pdata->gpio_data3, pdata->gpio_mux_data3);

	return 0;
}

static void pnxmci_freefci (struct pnx_mci_pdata *pdata)
{
	gpio_free(pdata->gpio_fcicmd);
	gpio_free(pdata->gpio_fciclk);
	gpio_free(pdata->gpio_data0);
	gpio_free(pdata->gpio_data1);
	gpio_free(pdata->gpio_data2);
	gpio_free(pdata->gpio_data3);
}

static struct mmc_host_ops pnxmci_ops = {
	.request	= pnxmci_request,
	.set_ios	= pnxmci_set_ios,
	.get_ro		= pnxmci_get_ro,
	.get_cd		= pnxmci_card_present,
};

static int __devinit pnxmci_probe(struct platform_device *pdev)
{
	struct pnxmci_host *host;
	struct mmc_host	*mmc;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct pnxmci_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto probe_out;
	}
	
	host = mmc_priv(mmc);
	host->mmc 	= mmc;
	host->pdev	= pdev;

	host->pdata = pdev->dev.platform_data;
	if (!host->pdata){
		dev_err(&pdev->dev, "Platform Data is missing\n");
		ret=-ENXIO;
		goto probe_free_host;
	}
	
	/* RESOURCE mem*/
	host->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!host->mem) {
		dev_err(&pdev->dev,
				"failed to get io memory region resouce.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}	

	host->mem = request_mem_region(host->mem->start,
						RESSIZE(host->mem), pdev->name);
	if (!host->mem) {
		dev_err(&pdev->dev, "failed to request io memory region.\n");
		ret = -ENOENT;
		goto probe_free_host;
	}
	
	host->base = ioremap(host->mem->start, RESSIZE(host->mem));
	if (!host->base) {
		dev_err(&pdev->dev, "failed to ioremap() io memory region.\n");
		ret = -EINVAL;
		goto probe_free_mem_region;
	}

	/* RESOURCE irq*/
	host->irq = platform_get_irq(pdev, 0);
	if (!host->irq) {
		dev_err(&pdev->dev, "failed to get interrupt resouce.\n");
		ret = -EINVAL;
		goto probe_iounmap;
	}
	
	host->irq_cd = platform_get_irq(pdev, 1);
	if (!host->irq_cd) {
		dev_err(&pdev->dev, "failed to get extint resouce.\n");
		ret = -EINVAL;
		goto probe_iounmap;
	}
	
	ret=request_irq(host->irq, pnxmci_irq, 0, DRIVER_NAME" (fci)" , host);
	if (ret) {
		dev_err(&pdev->dev, "failed to request mci interrupt.\n");
		goto probe_iounmap;
	}
	disable_irq(host->irq);

	/* configure card detection */
	/* register GPIO in input */
	ret=gpio_request(EXTINT_TO_GPIO(host->irq_cd), pdev->name);
	if (ret){
		dev_err(&pdev->dev, "failed to request extint.\n");
		goto probe_free_irq;
	}
	gpio_direction_input(EXTINT_TO_GPIO(host->irq_cd));
	ret=set_irq_type(host->irq_cd, IRQ_TYPE_EDGE_BOTH);
	if (ret){
		dev_err(&pdev->dev, "failed to set extint edge.\n");
		goto probe_free_gpio;
	}
	ret = request_irq(host->irq_cd, pnxmci_irq_cd,
			0, DRIVER_NAME" (cd)", host);
	if (ret){
		dev_err(&pdev->dev,
				"failed to request card detect interrupt\n");
		goto probe_free_gpio;
	}

	/* RESOURCE dma */
	host->dma_ch = platform_get_resource(pdev, IORESOURCE_DMA, 0)->start;
	host->dma_ch = pnx_request_channel("mmc_pnx",host->dma_ch,
			pnxmci_dma_callback, (void*)host);
	if (IS_ERR_VALUE(host->dma_ch)){
		dev_err(&pdev->dev, "unable to get DMA channel.\n");
		host->dodma=0;
		ret = host->dma_ch;
		goto probe_free_irq_cd;
	}
	host->dodma=1;
	pnxmci_dma_init(host);
	/* allocate coherent memory (64k), lli not functional */
	host->buffer=dma_alloc_coherent(&pdev->dev, FCI_BUFFER_SZ,
			&host->buffer_dma, GFP_KERNEL);
	if (!host->buffer){
		dev_err(&pdev->dev, "dma allocation failed.\n");
		ret = -ENOMEM;
		goto probe_free_dmachannel;
	}

	/* FCI MUX */
	ret = pnxmci_initfci(host->pdata, pdev->name);
	if (ret){
		dev_err(&pdev->dev, "failed to init fci pad.\n");
		goto probe_free_dmabuffer;
	}

	/* MCI configuration*/
	mmc->ops 	= &pnxmci_ops;
	mmc->ocr_avail	= MMC_VDD_32_33;
	mmc->caps = MMC_CAP_4_BIT_DATA
		| MMC_CAP_SD_HIGHSPEED
		| MMC_CAP_MMC_HIGHSPEED;
	mmc->f_min 	= 400000;				/* 400KHz */
	mmc->f_max 	= fc_clk_pt_func[0]/2;	/* fc_ck/2  */

	if (host->pdata->ocr_avail)
		mmc->ocr_avail = host->pdata->ocr_avail;
	
	/* we have a 16-bit data length register, we must */
	/* ensure that we don't exceed (2^16)-1 bytes in a single request. */
    mmc->max_blk_count	= 65535;
    mmc->max_blk_size	= 2048;
    mmc->max_req_size	= 65535;
    mmc->max_seg_size	= PAGE_SIZE; /* default is PAGE_SIZE (4096); */

    /* 	mmc->max_phys_segs	= 16; */
    /* mmc->max_hw_segs	= 16; */

    mmc->max_phys_segs	= 1;
     mmc->max_hw_segs	= 1;


	/* set default clock */
	/* only parent clk can be set rate */
	host->fci_clk = clk_get(NULL, "FCI");
	if (IS_ERR(host->fci_clk)) {
		ret = PTR_ERR(host->fci_clk);
		dev_err(&pdev->dev,"failed to get fci clk\n");
		goto probe_free_fci;
	}

	host->fc_clk = clk_get_parent(host->fci_clk);
	if (IS_ERR(host->fc_clk)) {
		ret = PTR_ERR(host->fc_clk);
		dev_err(&pdev->dev,"failed to get parent clk\n");
		goto probe_free_fci;
	}
	
	host->fc_clk_rate=0;
	pnxmci_mng_clkrate(host,0);

	clk_enable(host->fci_clk);
	/* clock gating feature on fifoctrl. This is useful to prevent 
	 * the over-run and under-run conditions during RX and TX */
	writel(FCI_GATEFCICLKEN, host->base + FCI_FIFOCTRL_OFFSET);
	/* clock gating --> disable clock */
	clk_disable(host->fci_clk);

	INIT_DELAYED_WORK(&host->qos_work,pnxmci_wq_qos_timeout);

	pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT,
			DRIVER_NAME, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_requirement(PM_QOS_DDR_THROUGHPUT,
			DRIVER_NAME, PM_QOS_DEFAULT_VALUE);
	pnxmci_pm_qos_down(host);

	spin_lock_init(&host->complete_lock);
	tasklet_init(&host->pnxmci_tasklet,
			pnxmci_tasklet, (unsigned long) host);

	host->complete_what 	= COMPLETION_NONE;

	/* host register to mmc core */
	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(&pdev->dev, "failed to add mmc host.\n");
		goto probe_free_fci;
	}

	platform_set_drvdata(pdev, mmc);
	dev_info(&pdev->dev,
			"support pnxmci "
			"mci_base:%p irq:%u irq_cd:%u dma_ch:%u\n",
			host->base,host->irq,host->irq_cd,host->dma_ch);

	/* init stat variables */
	ret=device_create_file(&mmc->class_dev, &dev_attr_stat);
	if (ret)
		dev_err(&pdev->dev, "no stat possible.\n");

	memset(&(host->stat), 0, sizeof(host->stat));
	
/* ACER Jen chang, 2009/12/07, IssueKeys:AU4.FC-498, Add to adjust sd card clock by sysfs { */
	ret=device_create_file(&mmc->class_dev, &dev_attr_clk_select);
	if (ret)
		dev_err(&pdev->dev, "no clk_select possible.\n");
/* } ACER Jen Chang, 2009/12/07*/
	
	return ret;

	/* errors managment */
probe_free_fci:
	pnxmci_freefci(host->pdata);
probe_free_dmabuffer:
	dma_free_coherent(&pdev->dev, FCI_BUFFER_SZ, host->buffer,
			host->buffer_dma);
probe_free_dmachannel:
	pnx_free_channel(host->dma_ch);
probe_free_irq_cd:
	free_irq(host->irq_cd, host);
probe_free_gpio:
	gpio_free(EXTINT_TO_GPIO(host->irq_cd));
probe_free_irq:
	free_irq(host->irq, host);
probe_iounmap:
	iounmap(host->base);
probe_free_mem_region:
	release_mem_region(host->mem->start, RESSIZE(host->mem));
probe_free_host:
	mmc_free_host(mmc);
probe_out:
	return ret;
}

static void pnxmci_shutdown(struct platform_device *pdev)
{
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct pnxmci_host *host = mmc_priv(mmc);

	free_irq(host->irq_cd, host);
	gpio_free(EXTINT_TO_GPIO(host->irq_cd));
	
	pnxmci_freefci(host->pdata);

	mmc_remove_host(mmc);
}

static int __devexit pnxmci_remove(struct platform_device *pdev)
{
	struct mmc_host		*mmc  = platform_get_drvdata(pdev);
	struct pnxmci_host	*host = mmc_priv(mmc);

	pnxmci_shutdown(pdev);

	cancel_delayed_work(&host->qos_work);
	flush_scheduled_work();

	tasklet_disable(&host->pnxmci_tasklet);

	pnx_free_channel(host->dma_ch);
	dma_free_coherent(&pdev->dev, FCI_BUFFER_SZ, host->buffer,
			host->buffer_dma);
	free_irq(host->irq, host);

	iounmap(host->base);
	release_mem_region(host->mem->start, RESSIZE(host->mem));

	mmc_free_host(mmc);
	
	return 0;
}

#ifdef CONFIG_PM
static int pnxmci_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	return  mmc_suspend_host(mmc, state);
}

static int pnxmci_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	return mmc_resume_host(mmc);
}

#endif /* CONFIG_PM */

static struct platform_driver pnxmci_driver = {
	.driver.name	= DRIVER_NAME,
	.driver.owner	= THIS_MODULE,
	.probe		= pnxmci_probe,
	.remove		= __devexit_p(pnxmci_remove),
	.shutdown	= pnxmci_shutdown,
#ifdef CONFIG_PM
	.suspend	= pnxmci_suspend,
	.resume		= pnxmci_resume,
#endif
};

static int __init pnxmci_init(void)
{
	return platform_driver_register(&pnxmci_driver);
}

static void __exit pnxmci_exit(void)
{
	platform_driver_unregister(&pnxmci_driver);
}
module_init(pnxmci_init);
module_exit(pnxmci_exit);

MODULE_DESCRIPTION("ST-Ericsson PNX MMC/SD Card Interface driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ludovic Barre <ludovic.barre@stericsson.com>");
