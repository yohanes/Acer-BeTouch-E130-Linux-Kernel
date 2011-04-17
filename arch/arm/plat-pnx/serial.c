/*
 *  linux/arch/arm/plat-pnx/serial.c
 *
 *  Copyright (C) 2007 ST-Ericsson
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/io.h>
#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <linux/pm_qos_params.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#ifdef CONFIG_PM
/*#include <mach/pm.h>*/
#endif

#include <linux/clk.h>
#include <mach/serial.h>

#define BASE_BAUD	(7372800)

#define ACER_L1_K3
#define GPS_HOTSTART_LAG_WORKAROUND

/*
 * Internal UARTs need to be initialized for the 8250 autoconfig to work
 * properly. Note that the TX watermark initialization may not be needed
 * once the 8250.c watermark handling code is merged.
 */
static struct plat_serial8250_port serial_platform_data[] = {
#ifndef CONFIG_NKERNEL_CONSOLE
    {
 	.membase	= (void __iomem*)UART1_BASE,
 	.mapbase	= UART1_BASE_ADDR,
 	.irq		= IRQ_UART1,
/* ACER Ed, 2010/06/17, Modified Uart1 tpye(STE only modified Uart2) { */
 	.flags		= UPF_BOOT_AUTOCONF|UPF_SKIP_TEST|UPF_FIXED_TYPE,
 	.iotype		= UPIO_MEM,
 	.regshift	= 2,
 	.uartclk	= BASE_BAUD,
	.type = PORT_16550A,
/* } ACER Ed, 2010/06/17*/
     },
#endif
#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
     {
	.membase	= (void __iomem*)UART2_BASE,
	.mapbase	= UART2_BASE_ADDR,
	.irq		= IRQ_UART2,
	.flags		= UPF_BOOT_AUTOCONF|UPF_SKIP_TEST|UPF_FIXED_TYPE,
	.iotype		= UPIO_MEM,
	.regshift	= 2,
	.uartclk	= BASE_BAUD,
	.type = PORT_16550A,
    },
#else
    {
	 .membase = (void __iomem *)UART2_BASE,
	 .mapbase = UART2_BASE_ADDR,
	 .irq = IRQ_UART2,
	 .flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
	 .iotype = UPIO_MEM,
	 .regshift = 2,
	 .uartclk = BASE_BAUD,
	 },
#endif
#ifdef CONFIG_NKERNEL_CONSOLE
    {
 	.membase	= (void __iomem*)UART1_BASE,
 	.mapbase	= UART1_BASE_ADDR,
 	.irq		= IRQ_UART1,
 	.flags		= UPF_BOOT_AUTOCONF|UPF_SKIP_TEST,
 	.iotype		= UPIO_MEM,
 	.regshift	= 2,
 	.uartclk	= BASE_BAUD,
     },
#endif
	/* void declaration needed by serial core */
	{
	 .flags = 0},
};

static struct platform_device serial_device = {
    .name = "serial8250",
    .id	  = 0,
    .dev  = { .platform_data = serial_platform_data, },
};

/*
 * Manage the rate of the uart bus clock on pnx 
 */
inline unsigned int pnx_serial_pm_qos_up(struct pnx_uart *uart)
{
	pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
				  (char *)uart->uart_name, 52);

	uart->pm_qos_status=PNX_UART_PM_QOS_UP_FORCE; 

	return 0;
}

inline unsigned int pnx_serial_pm_update_qos(struct pnx_uart *uart)
{
	if (uart->pm_qos_status == PNX_UART_PM_QOS_DOWN) {
		pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
					  (char *)uart->uart_name, 52);

		/* start guard timer */
		mod_timer(&(uart->pm_qos_timer), (jiffies + 1*HZ));
	}
	if (uart->pm_qos_status != PNX_UART_PM_QOS_UP_FORCE)
		uart->pm_qos_status=PNX_UART_PM_QOS_UP_XFER; 

	return 0;
}

static void pnx_serial_pm_qos_timeout( unsigned long arg)
{
	struct pnx_uart *uart = (struct pnx_uart *) arg;

	if (uart->pm_qos_status == PNX_UART_PM_QOS_UP_XFER) {
		mod_timer(&(uart->pm_qos_timer), (jiffies + 1*HZ));
		uart->pm_qos_status = PNX_UART_PM_QOS_UP;
	} else if (uart->pm_qos_status != PNX_UART_PM_QOS_UP_FORCE) {
		pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
					  (char *)uart->uart_name,
					  PM_QOS_DEFAULT_VALUE);
		uart->pm_qos_status=PNX_UART_PM_QOS_DOWN;
	}
}

inline unsigned int pnx_serial_pm_qos_down(struct pnx_uart *uart)
{
	uart->pm_qos_status = PNX_UART_PM_QOS_UP;
	
	pnx_serial_pm_qos_timeout( (unsigned long) uart);
	return 0;
}


static struct pnx_uart *console_uart;

/* Used to check if there is some activity on Rx or Tx part and in
 * consequence enable or disable clock*/
void check_rx_tx_activity(void)
{
	struct clk *parentClk = NULL;

	if ((!console_uart->tx_activity) &&
			(!console_uart->rx_activity) &&
			console_uart->clock_enable) {
		parentClk = clk_get_parent(console_uart->uartClk);

		clk_disable(parentClk);
		console_uart->clock_enable = 0;
		wake_unlock(&(console_uart->lock));
	} else if ((console_uart->rx_activity ||
				console_uart->tx_activity) &&
			(!console_uart->clock_enable)) {
		wake_lock(&(console_uart->lock));
		parentClk = clk_get_parent(console_uart->uartClk);
		clk_enable(parentClk);
		console_uart->clock_enable = 1;
	}
}
/*ACER Erace.Ma@20100722, GPS hot start workaround solution*/
#ifdef GPS_HOTSTART_LAG_WORKAROUND
static int onoff_uart=0;
void onoff_uart_clock(int onoff)
{
	struct clk *parentClk = NULL;
	parentClk = clk_get_parent(console_uart->uartClk);

	if(onoff == 1)
	{
		if(onoff_uart == 0)
		{
			printk("onoff_uart_clock on\n");
			clk_enable(parentClk);
			wake_unlock(&(console_uart->lock));
			onoff_uart = 1;
		}
	}
	else
	{
		if(onoff_uart == 1)
		{
			printk("onoff_uart_clock off\n");
			wake_lock(&(console_uart->lock));
			clk_disable(parentClk);
			onoff_uart--;
		}
	}
}
#endif /*GPS_HOTSTART_LAG_WORKAROUND*/
/*End ACER Erace.Ma@20100722*/
/*
 * Manage the detection of activity on uart.
 * This feature is used to disble console clock and put system in sleep mode
 */
#define SERIAL_RX_ACTIVITY_TO 1000
#define SERIAL_TX_ACTIVITY_TO 50

void rx_timeout_work(struct work_struct *work)
{
	/* to avoid preemption problems */
	console_uart->rx_activity = 1;
	wmb();
	/* if rx activity has not been detected since last timeout */
	if (!console_uart->rx_triggered) {
		console_uart->rx_activity = 0;
			/* to avoid preemption problems */
		if (!console_uart->rx_triggered) {
			check_rx_tx_activity();
			return;
		}
	}
	console_uart->rx_activity = 1;
	console_uart->rx_triggered = 0;
		queue_delayed_work(console_uart->workqueue,
			&(console_uart->rx_work),
			SERIAL_RX_ACTIVITY_TO);
	check_rx_tx_activity();
}

void tx_timeout_work(struct work_struct *work)
{
	/* to avoid preemption problems */
	console_uart->tx_activity = 1;
	wmb();
	/* if tx activity has not been detected since last timeout */
	if (!console_uart->tx_triggered) {
		console_uart->tx_activity = 0;
		/* to avoid preemption problems */
		if (!console_uart->tx_triggered) {
			check_rx_tx_activity();
			return;
		}
	}
	console_uart->tx_activity = 1;
	console_uart->tx_triggered = 0;
	queue_delayed_work(console_uart->workqueue,
			&(console_uart->tx_work),
			SERIAL_TX_ACTIVITY_TO);
	check_rx_tx_activity();
}

/* IRQ handler used to detect Rx activity. Schedule the first Rx timer.*/
static irqreturn_t pnx_serial_rx_activity_detected(int irq, void *dev_id)
{
	console_uart->rx_triggered = 1;

	if (!console_uart->rx_activity)
		queue_delayed_work(console_uart->workqueue,
				&(console_uart->rx_work),
				0);
	return IRQ_HANDLED;
}

/* Not an IRQ handler because directly called by another function which want
 * to send data. Schedule the first tx timer .*/
unsigned int pnx_serial_tx_activity_detected(void)
{
	console_uart->tx_triggered = 1;

	if (!console_uart->tx_activity)
		queue_delayed_work(console_uart->workqueue,
				&(console_uart->tx_work),
				0);
	return 0;
}

/* MJA patch for uart on TAT */
int pnx_serial_set_activity_detect(struct pnx_uart *uart)
{
	int ret;

	uart->enable_detection = 0;
	
	/* console activity detection is only available for UART1 */
    if ( strcmp(uart->uart_name,"UART1") != 0 )
		return 0;

	console_uart = uart;

	uart->enable_detection = 1;
	uart->rx_activity = 0;
	uart->tx_activity = 0;
	uart->rx_triggered = 0;
	uart->tx_triggered = 0;
	uart->clock_enable = 0;

	INIT_DELAYED_WORK(&(uart->rx_work), rx_timeout_work);
	INIT_DELAYED_WORK(&(uart->tx_work), tx_timeout_work);
	
	uart->workqueue = create_singlethread_workqueue("kserd");
	if (!(uart->workqueue))
		goto irq_free;

	wake_lock_init(&(uart->lock), WAKE_LOCK_SUSPEND, "console activity");

	if (!tatonuart) {
		/* if not in TAT uart */
		if (set_irq_type(IRQ_EXTINT(18), IRQ_TYPE_EDGE_BOTH)) {
			printk(KERN_WARNING "failed setirq type\n");
			goto irq_free;
		}

		ret = request_irq(IRQ_EXTINT(18),
				pnx_serial_rx_activity_detected,
				0, " serial (detect)", NULL);
		if (ret) {
			printk(KERN_WARNING "request_irq failed\n");
			goto irq_free;
		}
	} else {
		pm_qos_update_requirement(PM_QOS_PCLK2_THROUGHPUT,
					  (char *)uart->uart_name, 52);
		uart->pm_qos_status=PNX_UART_PM_QOS_UP_XFER;
	}

irq_free:
	return 0;	
}

static int __init pnx_serial_init(void)
{
	int i;
	struct clk *pClk_13M;
	struct clk *pClk_26M;
	struct clk *pClk_pclk;
	struct pnx_uart *uart_pnx;

	pClk_13M=clk_get(NULL,"clk13m_ck");
	pClk_26M=clk_get(NULL,"clk26m_ck");
	pClk_pclk=clk_get(NULL,"pclk2_ck");

	if (IS_ERR(pClk_13M) || IS_ERR(pClk_26M) || IS_ERR(pClk_pclk) ){
		printk(KERN_WARNING "%s: parents clk failed\n", __func__);
		goto out;
	}

	/* supress void element from loop */
	for (i = 0; i < (ARRAY_SIZE(serial_platform_data) - 1); i++) {
		struct clk *clk_tmp=NULL;
		
		/* Allocation of pnx struct on uart X */
		uart_pnx=kzalloc(sizeof(*uart_pnx),GFP_KERNEL);
		if (!uart_pnx)
			continue;

		if (serial_platform_data[i].irq == IRQ_UART1) {
			clk_tmp = clk_get(NULL, "UART1");
			strcpy(uart_pnx->uart_name, "UART1");
		} else if (serial_platform_data[i].irq == IRQ_UART2) {
			clk_tmp = clk_get(NULL, "UART2");
			strcpy(uart_pnx->uart_name, "UART2");
		}

		/* initialize pclk2 rate management */
		uart_pnx->pm_qos_status = PNX_UART_PM_QOS_DOWN;
		pm_qos_add_requirement(PM_QOS_PCLK2_THROUGHPUT,
				       uart_pnx->uart_name,
				       PM_QOS_DEFAULT_VALUE);
		uart_pnx->irq_enabled = 1;
		init_timer(&(uart_pnx->pm_qos_timer));
		uart_pnx->pm_qos_timer.function = &pnx_serial_pm_qos_timeout;
		uart_pnx->pm_qos_timer.data= (unsigned long)uart_pnx;

		if ( !IS_ERR(clk_tmp) && clk_tmp!=NULL ){
			uart_pnx->uartClk=clk_tmp;
			uart_pnx->pClk_13M=pClk_13M;
			uart_pnx->pClk_26M=pClk_26M;
			uart_pnx->pClk_pclk=pClk_pclk;
			serial_platform_data[i].private_data=uart_pnx;
		} else {
			printk(KERN_WARNING
			       "%s - get uart clock failed error:%ld\n",
			       __func__, PTR_ERR(clk_tmp));
			kfree(uart_pnx);
			continue;
		}

		/* initialize uart1 activity detection (for console) */
		pnx_serial_set_activity_detect(uart_pnx);
	}
	
out:	
	/* free clks */
	clk_put(pClk_13M);
	clk_put(pClk_26M);
	clk_put(pClk_pclk);

	return platform_device_register(&serial_device);
}

arch_initcall(pnx_serial_init);
