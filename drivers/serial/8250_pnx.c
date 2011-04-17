/*
 * linux/drivers/serial/8250_pnx.c
 *
 * Created:      20.02.2009
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <mach/serial.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <mach/clock.h>
#include <mach/serial.h>

#define uart_console(port)	((port)->cons &&  \
		(port)->cons->index == (port)->line)

/*
 * console and pctools has needed to start before serial_init
 * (with cgu interface) 
 */
static int uart_enable_clock(struct uart_port *port)
{
	u32 v;
	v = readl (CGU_GATESC1_REG );

	if ( port->irq == IRQ_UART1 )
		v |= CGU_UART1EN_1;
	else if ( port->irq == IRQ_UART2 )
		v |= CGU_UART2EN_1;

	writel (v, CGU_GATESC1_REG);

	return 0;
}

static int uart_disable_clock(struct uart_port *port)
{
	u32 v;
	v = readl (CGU_GATESC1_REG );

	if ( port->irq == IRQ_UART1 )
		v &= ~CGU_UART1EN_0;
	else if ( port->irq == IRQ_UART2 )
		v &= ~CGU_UART2EN_0;

	writel (v, CGU_GATESC1_REG);

	return 0;
}

unsigned int serial8250_enable_clock(struct uart_port *port)
{
	struct pnx_uart *uart_pnx = port->private_data;
	struct clk *parentClk = NULL;


	if (!uart_pnx)
		return uart_enable_clock(port);

	if (IS_ERR(uart_pnx->uartClk)){
		printk(KERN_WARNING "%s - uart clock failed error:%ld\n",
		       __func__, PTR_ERR(uart_pnx->uartClk));
		return PTR_ERR(uart_pnx->uartClk);
	}

	if (clk_get_usecount(uart_pnx->uartClk) == 0) {
		clk_enable(uart_pnx->uartClk);
		if (uart_pnx->enable_detection) {
			/* compensate the uart clock enable */
			parentClk = clk_get_parent(uart_pnx->uartClk);
			clk_disable(parentClk);
		}
	}
	return 0;
}

unsigned int serial8250_disable_clock(struct uart_port *port)
{
	struct pnx_uart *uart_pnx = port->private_data;
	struct clk *parentClk = NULL;

	if (!uart_pnx)
		return uart_disable_clock(port);

	if (IS_ERR(uart_pnx->uartClk)){
		printk(KERN_WARNING "%s - uart clk error :%ld\n", __func__,
				PTR_ERR(uart_pnx->uartClk));
		return PTR_ERR(uart_pnx->uartClk);
	}
	if (clk_get_usecount(uart_pnx->uartClk) >= 1) {
		if (uart_pnx->enable_detection) {
			/* compensate the uart clock disable */
			parentClk = clk_get_parent(uart_pnx->uartClk);
			clk_enable(parentClk);
		}
		clk_disable(uart_pnx->uartClk);
	}
	return 0;
}

unsigned int serial8250_get_custom_clock(struct uart_port *port,
		unsigned int baud)
{
	u32 ret;
	
	if ( baud==3250000 )
		ret=52000000;
	else if ( baud==2000000 )
		ret=32000000;
	else if ( baud==1843200 )
		ret=29491200;
	else if ( baud== 921600 )
		ret=14745600;
	else
		ret=7372800;

	return ret;
}

void serial8250_set_custom_clock(struct uart_port *port)
{
	u32 fdiv_m=0x5F37;
	u32 fdiv_n=0x3600;
	u32 fdiv_ctrl=UARTX_FDIV_ENABLE_ON;
	struct clk *parentClk=NULL;
	struct pnx_uart *uart_pnx =  port->private_data; 

	switch (port->uartclk) {
		case 7372800:
		{ /* clk=13MHz */
			fdiv_ctrl|= UARTX_CLKSEL_13M;
			break;
		}
		case 14745600:
		{ /* clk=26MHz */
			fdiv_ctrl|= UARTX_CLKSEL_26M;
			break;
		}
		case 29491200:
		{ /* clk=pclk */
			fdiv_ctrl|= UARTX_CLKSEL_PCLK;
			break;
		}
		case 32000000:
		{ /* clk=pclk */
			fdiv_n=0x3A98;
			fdiv_ctrl|= UARTX_CLKSEL_PCLK;
			break;
		}
		case 52000000:
		{ /* clk=pclk */
			fdiv_n=0x5F37;
			fdiv_ctrl|= UARTX_CLKSEL_PCLK;
			break;
		}
	}
	
	if ( uart_pnx!=NULL && !IS_ERR(uart_pnx->uartClk) ){
		/* if cgu interface is ready and pnx_serial_init */
		if (fdiv_ctrl & UARTX_CLKSEL_26M)
			parentClk=uart_pnx->pClk_26M;
		else if (fdiv_ctrl & UARTX_CLKSEL_PCLK)
			parentClk=uart_pnx->pClk_pclk;
		else 
			parentClk=uart_pnx->pClk_13M;

		if (!IS_ERR(parentClk)) {
			if (uart_pnx->enable_detection
			    || uart_pnx->clock_enable)
						serial8250_disable_clock(port);
						
			if (clk_set_parent(uart_pnx->uartClk,parentClk)!=0)
				printk(KERN_WARNING "%s: set parent failed\n",
				       __func__);

			if (uart_pnx->enable_detection
			    || uart_pnx->clock_enable)
						serial8250_enable_clock(port);
		}
	}

	writel(fdiv_m,port->membase + UART1_FDIV_M_OFFSET);
	writel(fdiv_n,port->membase + UART1_FDIV_N_OFFSET);
	writel(fdiv_ctrl,port->membase + UART1_FDIV_CTRL_OFFSET);
}

unsigned int serial8250_qos_up(struct uart_port *port)
{
	struct pnx_uart *uart = port->private_data;

	pnx_serial_pm_qos_up(uart);

	return 0;
}

unsigned int serial8250_update_qos(struct uart_port *port)
{
	struct pnx_uart *uart = port->private_data;
	
	pnx_serial_pm_update_qos(uart);

	return 0;
}

unsigned int serial8250_qos_down(struct uart_port *port)
{
	struct pnx_uart *uart = port->private_data;
	
	pnx_serial_pm_qos_down(uart);

	return 0;
}

unsigned int serial8250_enable_irq(struct uart_port *port)
{
	enable_irq(port->irq);

	return 0;
}
 
unsigned int serial8250_disable_irq(struct uart_port *port)
{
	disable_irq(port->irq);

	return 0;
}

extern void wait_for_full_xmitr(struct uart_port *port);

void serial8250_custom_pm(struct uart_port *port, unsigned int state,
			  unsigned int old)
		{
		struct pnx_uart *uart = port->private_data;

/*		printk(KERN_WARNING "serial_pnx_pm port %d : new %d, old %d\n",
 *		port->line, state, old);*/

	if (state == 0) {
			if (!uart_console(port))
			serial8250_qos_up(port);
			serial8250_enable_clock(port);
			if (old == 3)
				uart->irq_enabled = 1;
			if ((old == 7) && (uart->irq_enabled == 0)) {
				serial8250_enable_irq(port);
				uart->irq_enabled = 1;
			}
	} else if (state == 3) {
			serial8250_disable_clock(port);
			if (!uart_console(port))
			serial8250_qos_down(port);
	} else if (state == 7) {
			wait_for_full_xmitr(port);
			if (old == 0) {
				serial8250_disable_irq(port);
				uart->irq_enabled = 0;
			}
			serial8250_disable_clock(port);
			if (!uart_console(port))
			serial8250_qos_down(port);
	} else
			printk(KERN_WARNING "serial_pnx_pm not yet supported\n");

}

unsigned int serial8250_Tx_activity_detected(struct uart_port *port)
{
	return pnx_serial_tx_activity_detected();
}
