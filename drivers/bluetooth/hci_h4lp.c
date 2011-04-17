/*
 *
 *  Bluetooth HCI UART driver add-on for Low Power support
 *
 *  Acknowledgements:
 *  This file is an add-on to hci_h4.c from:
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2005  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/spinlock.h>


#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#include "hci_uart.h"


#if 0
#ifdef BT_DBG
#undef BT_DBG
#endif
#define BT_DBG(format, arg...) printk(KERN_ERR "%s - " format "\n", __FUNCTION__, ## arg)
#endif

/* H4LP states */
typedef enum {
	H4LP_NO_LOW_POWER,
	H4LP_ASLEEP,
	H4LP_AWAKE,
	H4LP_AWAKE_TO_ASLEEP
}h4lp_states_e;

struct h4lp_struct {
	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
};

struct h4lp_uart_sleep_ctrl {
	struct hci_uart *hu;
	int sleep_state;
};

/* H4LP receiver States */
#define H4LP_W4_PACKET_TYPE	0
#define H4LP_W4_EVENT_HDR	1
#define H4LP_W4_ACL_HDR		2
#define H4LP_W4_SCO_HDR		3
#define H4LP_W4_DATA		4

/* Low power management */
#define H4LP_INACTIVITY_TIMEOUT     3333  /* 3.333s */

struct hci_sleep_ops {
	int (*enter)(void);
	int (*leave)(void);
};

static void h4lp_device_interupt_callback(int code);
static void h4lp_reloadInactivityTimer(void);
static void h4lp_stopInactivityTimer(void);
static void h4lp_inactivity_detected(unsigned long data);

struct hci_sleep_ops *bt_device_ops = NULL;
static h4lp_states_e h4lp_state = H4LP_NO_LOW_POWER;
static struct timer_list h4lp_inactivityTimer = TIMER_INITIALIZER(h4lp_inactivity_detected, 0, 0);
static spinlock_t h4lp_stateLock = SPIN_LOCK_UNLOCKED;
static struct h4lp_uart_sleep_ctrl *h4lp_SleepCtrl = NULL;

static inline void h4lp_releaseUartClock(void)
{
	if (h4lp_SleepCtrl != NULL)
	{
		struct uart_state *state = h4lp_SleepCtrl->hu->tty->driver_data;
		struct uart_port *port = state->port;

		serial8250_custom_pm(port,7, h4lp_SleepCtrl->sleep_state);
		h4lp_SleepCtrl->sleep_state = 7;
	}
}

static inline void h4lp_lockUartClock(void)
{
	if (h4lp_SleepCtrl != NULL)
	{
		struct uart_state *state = h4lp_SleepCtrl->hu->tty->driver_data;
		struct uart_port *port = state->port;

		serial8250_custom_pm(port,0, h4lp_SleepCtrl->sleep_state);
		h4lp_SleepCtrl->sleep_state = 0;
	}
}

/* Register low power callbacks */
void * h4lp_device_register_sleep_ops(struct hci_sleep_ops *sleep_ops)
{
	void * return_cb = NULL;
	unsigned long flags;
	BT_DBG("");
	spin_lock_irqsave(&h4lp_stateLock, flags);
	if ((sleep_ops) && (sleep_ops->enter) && (sleep_ops->leave))
	{
		bt_device_ops = sleep_ops;
		h4lp_state = H4LP_AWAKE;
		return_cb = (void *)h4lp_device_interupt_callback;
	}
	else
	{
		BT_DBG("NO customer bt chipset call back function");
		h4lp_stopInactivityTimer();
		h4lp_state = H4LP_NO_LOW_POWER;
		bt_device_ops = NULL;
	}
	spin_unlock_irqrestore(&h4lp_stateLock, flags);

	return return_cb;
}
EXPORT_SYMBOL(h4lp_device_register_sleep_ops);

/* Chip wake up request callback */
void h4lp_device_interupt_callback(int code)
{
	int result = 0; 
	unsigned long flags;
	
	spin_lock_irqsave(&h4lp_stateLock, flags);
	BT_DBG(" code[%d]", code);
	if (h4lp_state != H4LP_NO_LOW_POWER)
	{
		switch (code)
		{
			case 0:
				BT_DBG(" Received Sleep entry ACK - State[%d]", h4lp_state);
				if (h4lp_state == H4LP_AWAKE_TO_ASLEEP)
				{
					h4lp_state = H4LP_ASLEEP;
					h4lp_releaseUartClock();
				}
				break;
		
			case -EINTR:
				BT_DBG(" Received Wake up request - State[%d]", h4lp_state);
				if (h4lp_state != H4LP_AWAKE)
				{
					h4lp_lockUartClock();
					result = bt_device_ops->leave();
					h4lp_state = H4LP_AWAKE;
					if (result)
						printk(KERN_ALERT "h4lp_device_interupt_callback: Error while waking up [%d]", result);
					h4lp_reloadInactivityTimer();
				}
				break;
			
			default:
				BT_DBG(" Received Sleep entry NACK - State[%d]", h4lp_state);
				if (h4lp_state == H4LP_AWAKE_TO_ASLEEP)
				{
					h4lp_lockUartClock();
					result = bt_device_ops->leave();
					h4lp_state = H4LP_AWAKE;
					if (result)
						printk(KERN_ALERT "h4lp_device_interupt_callback: Error while waking up [%d]", result);
					h4lp_reloadInactivityTimer();

				}
				break;
		}
	}
	spin_unlock_irqrestore(&h4lp_stateLock, flags);
}

/* Timer management */
static void h4lp_reloadInactivityTimer(void)
{
	BT_DBG("");
	mod_timer(&h4lp_inactivityTimer, jiffies + msecs_to_jiffies(H4LP_INACTIVITY_TIMEOUT));
}

static void h4lp_stopInactivityTimer(void)
{
	BT_DBG("");
	del_timer(&h4lp_inactivityTimer);
}

static void h4lp_inactivity_detected(unsigned long data)
{
	int result = 0; 
	unsigned long flags;
	
	spin_lock_irqsave(&h4lp_stateLock, flags);
	BT_DBG("");
	if ((timer_pending(&h4lp_inactivityTimer)) || (h4lp_state != H4LP_AWAKE))
	{
		BT_DBG("Inactivity timer restarted by a Read or write, or BT chip already/soon awake");
	}
	else
	{
		result = bt_device_ops->enter();
		if (result)
			printk(KERN_ALERT "h4lp_inactivity_detected: Error while sleeping [%d]", result);
		else
			h4lp_state = H4LP_AWAKE_TO_ASLEEP;

	}
	spin_unlock_irqrestore(&h4lp_stateLock, flags);	
}


void h4lp_init(struct hci_uart *hu)
{
	BT_DBG("");
	h4lp_SleepCtrl = kzalloc(sizeof(*h4lp_SleepCtrl), GFP_ATOMIC);
	if (h4lp_SleepCtrl)
	{
		h4lp_SleepCtrl->hu = hu;
		h4lp_SleepCtrl->sleep_state = 0;
	}
}

void h4lp_deinit(void)
{
	unsigned long flags;
	BT_DBG("");
	spin_lock_irqsave(&h4lp_stateLock, flags);
	h4lp_stopInactivityTimer();
	h4lp_state = H4LP_NO_LOW_POWER;
	bt_device_ops = NULL;
	spin_unlock_irqrestore(&h4lp_stateLock, flags);
	
	if (h4lp_SleepCtrl)
		kfree(h4lp_SleepCtrl);
		
}

void h4lp_SignalActivity(void)
{
	BT_DBG("");
	if (h4lp_state != H4LP_NO_LOW_POWER)
	{
		h4lp_reloadInactivityTimer();
	}
}

void h4lp_WakeUpForActivity(void)
{
	BT_DBG("");
	if (h4lp_state != H4LP_NO_LOW_POWER)
	{	
		h4lp_device_interupt_callback(-EINTR);
	}
}
