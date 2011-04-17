/*
 * linux/arch/arm/plat-pnx/include/mach/serial.h
 *
 * Description:  
 *
 * Created:      23.02.2009 13:15:25
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef  __SERIAL_H
#define  __SERIAL_H

#include <linux/clk.h>
#include <linux/wakelock.h>
struct pnx_uart {
	struct clk *uartClk;
	struct clk *pClk_13M;
	struct clk *pClk_26M;
	struct clk *pClk_pclk;
	char   uart_name[7];
	/* timer for pm_qos */
	struct timer_list pm_qos_timer;
	int				  pm_qos_status;
	/* console activity detection */
	int enable_detection;
	int rx_triggered;
	int tx_triggered;
	int rx_activity;
	int tx_activity;
	int clock_enable;
	int irq_enabled;
	struct delayed_work rx_work;
	struct delayed_work tx_work;
	struct workqueue_struct *workqueue;
	struct wake_lock lock;
};

enum t_pnx_uart_pm_qos_status{
	PNX_UART_PM_QOS_DOWN,
	PNX_UART_PM_QOS_UP,
	PNX_UART_PM_QOS_UP_XFER,
	PNX_UART_PM_QOS_UP_FORCE
};

enum t_pnx_uart_activity_type{
	PNX_UART_TX_ACTIVITY,
	PNX_UART_RX_ACTIVITY
};

inline unsigned int pnx_serial_pm_qos_up(struct pnx_uart *uart);
inline unsigned int pnx_serial_pm_update_qos(struct pnx_uart *uart);
inline unsigned int pnx_serial_pm_qos_down(struct pnx_uart *uart);
unsigned int pnx_serial_tx_activity_detected(void);

extern int tatonuart;
#endif   /* ----- #ifndef __SERIAL_H  ----- */
