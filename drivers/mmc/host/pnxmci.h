/*
 * ============================================================================
 *
 * Filename:     pnxmci.h
 *
 * Description:  
 *
 * Version:      1.0
 * Created:      10.04.2009 11:30:04
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
 * changelog:
 *
 * ============================================================================
 */
#define PM_QOS_HCLK2_MCI_THROUGHPUT 104
#define PM_QOS_DDR_MCI_THROUGHPUT 10

enum pnxmci_pm_qos_status{
	PNXMCI_PM_QOS_DOWN,
	PNXMCI_PM_QOS_UP,
};

enum pnxmci_waitfor {
	COMPLETION_NONE,
	COMPLETION_FINALIZE,
	COMPLETION_REQ,
	COMPLETION_CMDSENT,
	COMPLETION_RSPFIN,
	COMPLETION_XFERFINISH,
	COMPLETION_XFERFINISH_RSPFIN,
};

/*
 *
 *
 * fc_clk = 312MHz/(3+X)
 *		this clk must be set minimum for power consumption optimization
 *
 * fci_clk = fc_clk/(2*(divisor+1)) 
 *		divisor range [0..256]
 *
 */

struct pnxmci_stat{
	unsigned long				nb_core_req;
	unsigned long				nb_rxoverrun;
	unsigned long				nb_txunderrun;
	unsigned long				nb_datacrcfail;
	unsigned long				nb_datatimeout;
	unsigned long				nb_startbiterr;
	unsigned long				nb_dma_err;
	unsigned long				nb_pm_qos_up;
};

struct pnxmci_host {
	struct platform_device		*pdev;
	struct pnx_mci_pdata		*pdata;
	struct mmc_host				*mmc;
	struct resource				*mem;
	void __iomem				*base;

	struct clk					*fci_clk;
	struct clk					*fc_clk;
	unsigned long				fci_clk_rate;
	unsigned long				fc_clk_rate;
	unsigned int				fci_clk_div;
	/* ACER Jen chang, 2009/12/07, IssueKeys:AU4.FC-498, Add for adjusting sd card clock by sysfs { */
	unsigned int				clk_select;
	/* } ACER Jen Chang, 2009/12/07*/

	int							irq;
	int							irq_cd;

	int							dma_ch;
	int							dodma;

	struct pnx_dma_config		cfg;
	struct pnx_dma_ch_config	ch_cfg;
	struct pnx_dma_ch_ctrl		ch_ctrl;

	void						*buffer;
	dma_addr_t					buffer_dma;
	
	int							sg_len;						
	struct mmc_request			*mrq;
	int							cmd_is_stop;

	spinlock_t					complete_lock;

	enum pnxmci_waitfor			complete_what;

	struct tasklet_struct		pnxmci_tasklet;

	/* timer for pm_qos */
	struct delayed_work			qos_work;
	int							pm_qos_status;

	/* fci registers*/
	u32							fci_command;
	u32							fci_argument;

	u32							fci_mask;

	u32							fci_datatimer;
	u32							fci_datalenght;
	u32							fci_datactrl;
	
	struct pnxmci_stat			stat;
};
