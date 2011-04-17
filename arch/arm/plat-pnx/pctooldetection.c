/*
 * linux/arch/arm/plat-pnx/pctooldetection.c
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Loic Pallardy (LPA), loic.pallardy@stericsson.com
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/serial_reg.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>

#include <linux/sysfs.h>
#include <linux/kobject.h>

#include <asm/io.h>

 /* /sys/plat_pnx/ support */
#include "pnx_sysfs.h"

#define BASE_BAUD	(7372800)

static unsigned char pnx_pctool_status = 0xFF;

static unsigned int __init serial_in(struct uart_port *port, int offset)
{
	int off = offset << port->regshift;
	return readb(port->membase + off);
}

static void __init serial_out(struct uart_port *port, int offset, int value)
{
	int off = offset << port->regshift;
	writeb(value, port->membase + off);
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static void __init wait_for_xmitr(struct uart_port *port)
{
	unsigned int status;

	for (;;) {
		status = serial_in(port, UART_LSR);
		if ((status & BOTH_EMPTY) == BOTH_EMPTY)
			return;
		cpu_relax();
	}
}

static void __init my_putc(struct uart_port *port, int c)
{
	wait_for_xmitr(port);
	serial_out(port, UART_TX, c);
}

static unsigned char get_char(struct uart_port *port)
{
	unsigned char ch= 0;
	int lsr = serial_in(port, UART_LSR);
	if((lsr & UART_LSR_DR))
	{
		ch = serial_in(port, UART_RX);
	}
	return ch;
}

static void __init
pcdetect_serial8250_write(struct uart_port *port,
		const char *s, unsigned int count)
{
	unsigned int ier;

	/* Save the IER and disable interrupts */
	ier = serial_in(port, UART_IER);
	serial_out(port, UART_IER, 0);

	uart_console_write(port, s, count, my_putc);

	/* Wait for transmitter to become empty and restore the IER */
	wait_for_xmitr(port);
	serial_out(port, UART_IER, ier);
}

static void __init init_port(struct uart_port *port,int baud )
{
	unsigned int divisor;
	unsigned char c;

	serial_out(port, UART_LCR, 0x3);	/* 8n1 */
	serial_out(port, UART_IER, 0);		/* no interrupt */
	serial_out(port, UART_FCR, 0);		/* no fifo */
	serial_out(port, UART_MCR, 0x3);	/* DTR + RTS */

	divisor = port->uartclk / (16 * baud);
	c = serial_in(port, UART_LCR);
	serial_out(port, UART_LCR, c | UART_LCR_DLAB);
	serial_out(port, UART_DLL, divisor & 0xff);
	serial_out(port, UART_DLM, (divisor >> 8) & 0xff);
	serial_out(port, UART_LCR, c & ~UART_LCR_DLAB);

	serial8250_set_custom_clock(port);
}

static unsigned char __init pnx_pctooldetect(struct uart_port *port)
{
	int i=3;
	unsigned char ch =0;

	/* send 3 time Request code 0x49 to PC and check PC answer */
	do
	{
		pcdetect_serial8250_write(port, "I", 1);
		{
			int i;
			volatile int j = 0;
                        /*ACER_Fabio added for TAT connect DUT by UART*/
			//for(i=0; i<0x80000; i++) /* ~10 ms */
			for(i=0; (i<0x80000)&& (ch==0); i++)
			{
                                ch = get_char(port);
				j++;
			}
                        /*End ACER_Fabio added for TAT connect DUT by UART*/
		}
		//ch = get_char(port);	//Remove by Ethan Tsai@20090820 for UART detection
		i--;
	} while(i>0 && ch ==0);
	/* if PC synchronisation byte --> echo */
	if(ch != 0)
	{
		pcdetect_serial8250_write(port, &ch, 1);
	}
	else
	{
		ch = 0xFF;
	}
	return ch;
}

//MJA patch tat on uart (variable global shared with 8250_pnx.c 
//it disables extint 
int tatonuart=0;
static int __init pnx_pc_detection_init(void)
{
	struct uart_port port;
	int baud = 115200;

	memset(&port, 0, sizeof(port));

	/* configure UART port to 115200n8 */
	port.iotype = UPIO_MEM;
	port.uartclk = BASE_BAUD;
	port.mapbase = UART1_BASE_ADDR;
	port.membase = (void __iomem*)UART1_BASE;
	port.regshift = 2;
	port.irq = IRQ_UART1;
	port.line = 0;

	/* enable UART IP clock*/
	serial8250_enable_clock(&port);

	early_serial_setup(&port);
	init_port(&port, baud);

	/* send request code and check synchronization byte */
	pnx_pctool_status = pnx_pctooldetect(&port);

	/* disable UART IP clock*/
	serial8250_disable_clock(&port);

	/* if pc tool detected disable console over uart */
	if(pnx_pctool_status != 0xFF)
	{
		pcdetect_serial8250_write(&port,&pnx_pctool_status, 1);
		update_console_cmdline("ttyS", 0, "tty", 0, NULL);

		// MJA patch 
		tatonuart=0;
	}
	return 0;
}
console_initcall(pnx_pc_detection_init);

/********************/
/* sysfs definition */
/********************/
extern struct kobject *pnx_kobj;

static ssize_t pctooldetect_show(struct kobject *kobj,
	 struct kobj_attribute *attr, char *buf)
{
	int len = sprintf(buf,"pc tool detection status = 0x%x\n",
			pnx_pctool_status);
   return len;
}

PNX_ATTR_RO(pctooldetect);

static struct attribute * attrs[] = {
		&pctooldetect_attr.attr,
		NULL,
};

static struct attribute_group pctooldetect_attr_group = {
	.attrs = attrs,
	.name = "pctooldetect",
};

static int __init pctool_sysfs_init(void)
{
	return sysfs_create_group(pnx_kobj,&pctooldetect_attr_group);
}
arch_initcall(pctool_sysfs_init);







