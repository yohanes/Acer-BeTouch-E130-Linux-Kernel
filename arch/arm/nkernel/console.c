/*
 ****************************************************************
 *
 * Component = Linux console driver on top of the nanokernel
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)console.c 1.24     05/11/25 Jaluna"
 *
 * Contributor(s):
 *   Vladimir Grouzdev (grouzdev@jaluna.com) Jaluna SA
 *   Francois Armand (francois.armand@jaluna.com) Jaluna SA
 *   Guennadi Maslov (guennadi.maslov@jaluna.com) Jaluna SA
 *   Gilles Maigne (gilles.maigne@jaluna.com) Jaluna SA
 *   Chi Dat Truong <chidat.truong@jaluna.com>
 *
 ****************************************************************
 */
#include "osware/osware.h"

#include <linux/autoconf.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <asm/uaccess.h>
#include <asm/nkern.h>

#define SERIAL_NK_NAME	  "ttyNK"
#define SERIAL_NK_NAME_NB	  "ttyNK0"
#define SERIAL_NK_MAJOR	  254
#define SERIAL_NK_MINOR	  0

#define	NKLINE(tty)	((tty)->index)
#define	NKPORT(tty) ( (NkPort*)((tty)->driver_data))

#define	SERIAL_NK_TIMEOUT	(HZ/10)	/* ten times per second */
#define	SERIAL_NK_RXLIMIT	256	/* no more than 256 characters */
 struct NkConsole {
  NkXIrq	linux_xirq;           /* cross interrupt for Linux */
  NkOsId        linux_id ;
};
struct NkConsole *ptConsole;
typedef struct NkPort NkPort;
extern NkDevOps nkops; /* nano kernel reference */

struct NkPort {
#if 0
    struct timer_list timer;
#else
    NkXIrqId xirq;
#endif
    unsigned int      poss;
    char	      pchar;
    volatile int      stop;
    unsigned	      count;
    int		      (*poll)(NkPort*, int*);

};

#define MAX_PORT 2
struct NkPort serial_port[MAX_PORT];

static struct tty_driver  serial_driver;
static struct tty_struct* serial_table[2];
static struct ktermios*    serial_termios[2];
static struct ktermios*    serial_termios_locked[2];

    static int
nk_poll(NkPort* port, int* c)
{
    int res;
    unsigned long flags;
    char ch;

    hw_raw_local_irq_save(flags);
    res = os_ctx->dops.poll(os_ctx, &ch);
    hw_raw_local_irq_restore(flags);

    *c = ch;

    return res;
}

    static int
nk_poll_hist(NkPort* port, int* pc)
{
    int c;
    unsigned long flags;

    for (;;) {
	hw_raw_local_irq_save(flags);
    	c = os_ctx->hgetc(os_ctx, port->poss);
	hw_raw_local_irq_restore(flags);
	if (c) {
	    break;
	}
	port->poss++;
    }

    if (c > 0) {
	port->poss++;
	*pc = c;
    }

    return c;
}

    static inline void
nk_cons_write (const char* buf, unsigned int size)
{
    os_ctx->dops.write(os_ctx, buf, size);
}

    void
printnk (const char* fmt, ...)
{
	va_list args;
    int     size;
    char    buf[256];

    va_start(args, fmt);
    size = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (size >= sizeof(buf)) {
	size = sizeof(buf)-1;
    }

    nk_cons_write(buf, size);
}
EXPORT_SYMBOL(printnk);



    static int
serial_write_room (struct tty_struct* tty)
{
    return 4096;
}

    static void
serial_send_xchar (struct tty_struct* tty, char c)
{
}

    static void
serial_throttle (struct tty_struct* tty)
{
    if (NKLINE(tty)) {
        NKPORT(tty)->stop = 1;
    }
}

    static void
serial_unthrottle (struct tty_struct* tty)
{
    if (NKLINE(tty)) {
        NKPORT(tty)->stop = 0;
    }
}

    static inline void
_serial_put_char (struct tty_struct* tty, char c)
{
    NkPort* port = NKPORT(tty);
    if (port->pchar) {
	if (c != '\n') {
	    nk_cons_write(&(port->pchar), 1);
	}
	port->pchar = 0;
    }
    if (c == '\r') {
	port->pchar = '\r';
	return;
    }
    nk_cons_write(&c, 1);
}

    static inline void
_serial_flush (struct tty_struct* tty)
{
    NkPort* port = NKPORT(tty);
    if (port->pchar) {
        nk_cons_write(&(port->pchar), 1);
	port->pchar = 0;
    }
}

    static int
serial_write (struct tty_struct* tty,
	      const u_char*      buf,
	      int                count)
{
    int  scount;

    if (NKLINE(tty)) {
	return count;
    }

    scount = count;
    while (scount--) {
        char c;

	c = *buf++;
	_serial_put_char(tty, c);
    }

    _serial_flush(tty);

    return count;
}

    static int
serial_put_char (struct tty_struct* tty, u_char c)
{
	if (!NKLINE(tty)) {
		_serial_put_char(tty, c);
		if (c == '\n') {
			_serial_flush(tty);
		}
	}
	return 0;
}

    static int
serial_chars_in_buffer (struct tty_struct* tty)
{
    return 0;
}

    static void
serial_flush_buffer (struct tty_struct* tty)
{
}

    static void
serial_set_termios (struct tty_struct* tty, struct ktermios* old)
{
}

    static void
serial_stop (struct tty_struct* tty)
{
}

    static void
serial_start(struct tty_struct* tty)
{
}

    static void
serial_wait_until_sent (struct tty_struct* tty, int timeout)
{
}
#if 0
    static void
serial_timeout (unsigned long data)
#else
static void serial_timeout(void* data, NkXIrq xirq)
#endif
{
    struct tty_struct*	tty  = (struct tty_struct*)data;
    unsigned int	size = SERIAL_NK_RXLIMIT;
    int			c;
    NkPort*          port = NKPORT(tty);

    while (!port->stop && size && port->poll(port, &c) > 0 ) {
	tty_insert_flip_char(tty, c, TTY_NORMAL);
	size--;
    }

    if (size < SERIAL_NK_RXLIMIT) {
	tty_flip_buffer_push(tty);
    }
#if 0
    port->timer.expires = jiffies + SERIAL_NK_TIMEOUT;
    add_timer(&(port->timer));
#endif
}

    static int
serial_open (struct tty_struct* tty, struct file* filp)
{
    int line;
    line = NKLINE(tty);
    if (line < MAX_PORT) {
	NkPort* port = line + serial_port;
	port->count++;

	if (port->count == 1) {
	    port->pchar		 = 0;
	    tty->driver_data	 = port;
	    port->stop           = 0;
	    port->poss           = 0;
	    port->poll		 = nk_poll;
	    if (line == 1) {
		    port->poll = nk_poll_hist;
	    }
#if 0
	    init_timer(&port->timer);
	    port->timer.data     = (unsigned long)tty;
	    port->timer.function = serial_timeout;
	    port->timer.expires  = jiffies + SERIAL_NK_TIMEOUT;
	    add_timer(&port->timer);
#else
	port->xirq = nkops.nk_xirq_attach((NkXIrq )(ptConsole->linux_xirq),(NkXIrqHandler)( serial_timeout), (void *)(tty));
#endif
	}
	return 0;
    }
    return -ENODEV;
}

    static void
serial_close (struct tty_struct* tty, struct file* filp)
{
    NkPort* port = (NkPort*)tty->driver_data;
    port->count--;
    if (port->count == 0) {
#if 0
	del_timer(&(port->timer));
#else
     nkops.nk_xirq_detach(port->xirq);
#endif
	_serial_flush(tty);
    }

}

static const struct tty_operations serial_ops = {
    .open            = serial_open,
    .close           = serial_close,
    .write           = serial_write,
    .put_char        = serial_put_char,
    .write_room      = serial_write_room,
    .chars_in_buffer = serial_chars_in_buffer,
    .flush_buffer    = serial_flush_buffer,
    .throttle        = serial_throttle,
    .unthrottle      = serial_unthrottle,
    .send_xchar      = serial_send_xchar,
    .set_termios     = serial_set_termios,
    .stop            = serial_stop,
    .start           = serial_start,
    .wait_until_sent = serial_wait_until_sent,
};

    static int __init
serial_init (void)
{
    serial_driver.owner           = THIS_MODULE;
    serial_driver.magic           = TTY_DRIVER_MAGIC;
    serial_driver.driver_name     = "nkconsole";
    serial_driver.name            = SERIAL_NK_NAME;
    //serial_driver.devfs_name      = SERIAL_NK_NAME;
    serial_driver.major           = SERIAL_NK_MAJOR;
    serial_driver.minor_start     = SERIAL_NK_MINOR;
	serial_driver.name_base		  = 0;
    serial_driver.num             = 2;
    serial_driver.type            = TTY_DRIVER_TYPE_SERIAL;
    serial_driver.subtype         = SERIAL_TYPE_NORMAL;
    serial_driver.init_termios    = tty_std_termios;
    serial_driver.init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    serial_driver.flags           = TTY_DRIVER_REAL_RAW;
    serial_driver.refcount        = 0;
    serial_driver.ttys            = serial_table;
    serial_driver.termios         = serial_termios;
    serial_driver.termios_locked  = serial_termios_locked;

	tty_set_operations(&serial_driver, &serial_ops);

    if (tty_register_driver(&serial_driver)) {
    	printk(KERN_ERR "Couldn't register NK console driver\n");
    }

    return 0;
}

    static void __exit
serial_fini(void)
{
    unsigned long flags;

    local_irq_save(flags);

    if (tty_unregister_driver(&serial_driver)) {
	printk(KERN_ERR "Unable to unregister NK console driver\n");
    }

    local_irq_restore(flags);
}

    static int
nk_console_setup (struct console* c, char* unused)
{
    return 1;
}

    static void
nk_console_write (struct console* c, const char* buf, unsigned int size)
{
    nk_cons_write(buf, size);
}

    static struct tty_driver*
nk_console_device (struct console* c, int* index)
{
    *index = c->index;
    return &serial_driver;
}

static struct console nkcons =
{
	.name =		SERIAL_NK_NAME,
	.write =	nk_console_write,
	.setup =	nk_console_setup,
	.device =   nk_console_device,
	.flags =	CON_ENABLED,
	.index =	-1,

#if 0
name:	SERIAL_NK_NAME_NB, //SERIAL_NK_NAME,
	write:	nk_console_write,
	device:	nk_console_device,
	setup:	nk_console_setup,
	flags:	CON_ENABLED,
	index:	-1,
#endif
};
#define NK_DEV_ID_CONSOLE 'nxCO'

void late_console_init(void)
{
	NkPhAddr		pPhyConsole ;        /* Physical address  */
    struct NkConsole*     pVirtual ;    /* address  shared structure */


    pPhyConsole =  nkops.nk_dev_lookup_by_type (NK_DEV_ID_CONSOLE, 0) ;
    if (pPhyConsole)
    {

    /* get virtual address */
    pVirtual = (struct NkConsole *) nkops.nk_ptov(pPhyConsole + sizeof(NkDevDesc));

    /* alocate and attach Linux cross interrupt */
    pVirtual->linux_xirq =  nkops.nk_xirq_alloc(1 /* 1 cross IT */ );
    pVirtual->linux_id = nkops.nk_id_get();

	ptConsole=(struct NkConsole *)pVirtual;
	}
}
    void __init
nk_console_init (void)
{
    register_console(&nkcons);
    late_console_init();
}

module_init(serial_init);
module_exit(serial_fini);


