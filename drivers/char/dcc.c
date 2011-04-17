/*
 * linux/drivers/char/dcc.c
 *
 * Generic driver for JTAG DCC
 *
 * Copyright (C) 2009 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/console.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <asm/delay.h>

#ifndef CONFIG_DCC_DROP
#define CONFIG_DCC_DROP 0
#endif

struct dcc_ring {
	spinlock_t lock;
	char buffer[128];
	int count;
	int head;
};

static int dcc_ring_size(const struct dcc_ring *ring)
{
	return ARRAY_SIZE(ring->buffer);
}

static int dcc_ring_count(const struct dcc_ring *ring)
{
	return ring->count;
}

static void dcc_ring_flush(struct dcc_ring *ring)
{
	spin_lock(&ring->lock);
	ring->head = 0;
	ring->count = 0;
	spin_unlock(&ring->lock);
}

static void dcc_ring_init(struct dcc_ring *ring)
{
	spin_lock_init(&ring->lock);
	dcc_ring_flush(ring);
}

static int dcc_ring_reserve(struct dcc_ring *ring, char **ptr)
{
	int tail, size;

	spin_lock(&ring->lock);
	tail = ring->head + ring->count;
	size = dcc_ring_size(ring);
	if (tail >= size) {
		tail -= size;
		size = ring->head - tail;
	} else
		size -= tail;
	*ptr = &ring->buffer[tail];
	spin_unlock(&ring->lock);

	return size;
}

static void dcc_ring_commit(struct dcc_ring *ring, int len)
{
	spin_lock(&ring->lock);
	ring->count += len;
	spin_unlock(&ring->lock);
}

static int dcc_ring_decommit(struct dcc_ring *ring, char **ptr)
{
	int tail, size;

	spin_lock(&ring->lock);
	tail = ring->head + ring->count;
	size = dcc_ring_size(ring);
	if (tail > size)
		size -= ring->head;
	else
		size = ring->count;
	*ptr = &ring->buffer[ring->head];
	spin_unlock(&ring->lock);

	return size;
}

static void dcc_ring_release(struct dcc_ring *ring, int len)
{
	spin_lock(&ring->lock);
	ring->head += len;
	ring->head %= dcc_ring_size(ring);
	ring->count -= len;
	spin_unlock(&ring->lock);
}

struct dcc_device {
	unsigned refcount;
	struct dcc_ring ring;
	struct completion thread_started;
	struct completion flush;
	struct task_struct *thread;
	struct tty_struct *tty;
};

#ifndef __LINUX_ARM_ARCH__
#error __LINUX_ARM_ARCH__ is not defined
#endif

#if __LINUX_ARM_ARCH__ >= 6

#define DCC_READ_MASK   (1 << 30)
#define DCC_WRITE_MASK  (1 << 29)
#define DCC_STATUS_REGS "c0, c1, 0"
#define DCC_READ_REGS   "c0, c5, 0"
#define DCC_WRITE_REGS  "c0, c5, 0"

#else

#define DCC_READ_MASK   (1 << 0)
#define DCC_WRITE_MASK  (1 << 1)
#define DCC_STATUS_REGS "c0, c0"
#define DCC_READ_REGS   "c1, c0"
#define DCC_WRITE_REGS  "c1, c0"

#endif

static inline unsigned long dcc_status(void)
{
	volatile unsigned long status;
	asm("mrc 14, 0, %0, " DCC_STATUS_REGS "\n" : "=r" (status));
	return status;
}

static inline int dcc_can_read(unsigned long status)
{
	return status & DCC_READ_MASK;
}

static inline int dcc_can_write(unsigned long status)
{
	return !(status & DCC_WRITE_MASK);
}

static inline char dcc_read_char(void)
{
	volatile char c;
	asm("mrc 14, 0, %0, " DCC_READ_REGS "\n" : "=r" (c));
	return c;
}

static inline void dcc_write_char(char c)
{
	asm("mcr 14, 0, %0, " DCC_WRITE_REGS "\n" : : "r" (c));
}

#define DCC_BUSY_WAITING_THRESHOLD (1 << 9)
#define DCC_NICE_WAITING_THRESHOLD (1 << 18)

static int dcc_thread(void *data)
{
	struct dcc_device *dcc;
	unsigned long waiting;
	int c, written;

	dcc = data;
	complete(&dcc->thread_started);

	waiting = DCC_BUSY_WAITING_THRESHOLD;
	c = -1;
	written = 0;

	for (;;) {
		unsigned long timeout;
		unsigned long status;
		int len;
		char *ptr;

		status = dcc_status();

		len = dcc_ring_decommit(&dcc->ring, &ptr);
		if (!len) {
			if (written) {
				written = 0;
				tty_wakeup(dcc->tty);
			}
			if (!waiting)
				waiting = DCC_BUSY_WAITING_THRESHOLD;
			goto write_done;
		}
		if (dcc_can_write(status)) {
			dcc_write_char(*ptr);
			dcc_ring_release(&dcc->ring, 1);
			written = 1;
			waiting = 0;
		} else if (!waiting)
			waiting = 1;

	write_done:
		if (test_bit(TTY_THROTTLED, &dcc->tty->flags)) {
			if (!waiting)
				waiting = DCC_BUSY_WAITING_THRESHOLD;
			goto read_done;
		}
		if (c == -1) {
			if (dcc_can_read(status))
				c = dcc_read_char();
			else if (!waiting)
				waiting = 1;
		}
		if (c != -1 && tty_insert_flip_char(dcc->tty, c, TTY_NORMAL)) {
			tty_flip_buffer_push(dcc->tty);
			c = -1;
			waiting = 0;
		}

	read_done:
		if (waiting < DCC_BUSY_WAITING_THRESHOLD) {
			waiting *= 2;
			udelay(waiting);
			continue;
		}

		if (waiting < DCC_NICE_WAITING_THRESHOLD)
			waiting *= 2;
#if CONFIG_DCC_DROP > 0
		else {
			dcc_ring_flush(&dcc->ring);
			written = 1;
		}
#endif


		if (kthread_should_stop())
			break;

		timeout = usecs_to_jiffies(waiting);
		wait_for_completion_timeout(&dcc->flush, timeout);
	}

	return 0;
}

static int dcc_tty_write(struct tty_struct *tty, const unsigned char *buffer,
			 int count)
{
	struct dcc_device *dcc;
	int left, len;
	char *ptr;

	dcc = tty->driver_data;
	left = count;
	while (left > 0 && (len = dcc_ring_reserve(&dcc->ring, &ptr))) {
		if (left < len)
			len = left;
		left -= len;
		memcpy(ptr, buffer, len);
		buffer += len;
		dcc_ring_commit(&dcc->ring, len);
	}

	return count - left;
}

static int dcc_tty_put_char(struct tty_struct *tty, unsigned char c)
{
	struct dcc_device *dcc;
	int len;
	char *ptr;

	dcc = tty->driver_data;
	len = dcc_ring_reserve(&dcc->ring, &ptr);
	if (len) {
		*ptr = c;
		dcc_ring_commit(&dcc->ring, 1);
	}

	return len;
}

static int dcc_tty_write_room(struct tty_struct *tty)
{
	struct dcc_device *dcc;
	int count;

	dcc = tty->driver_data;
	count = dcc_ring_size(&dcc->ring) - dcc_ring_count(&dcc->ring);

	return count;
}

static int dcc_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct dcc_device *dcc;
	int count;

	dcc = tty->driver_data;
	count = dcc_ring_count(&dcc->ring);

	return count;
}

static void dcc_tty_flush(struct tty_struct *tty)
{
	struct dcc_device *dcc;

	dcc = tty->driver_data;
	complete(&dcc->flush);
}

static struct tty_driver *tty_driver = NULL;
static struct dcc_device dcc_device[1];
static int dcc_major = 0;

static int dcc_tty_open(struct tty_struct *tty, struct file *file)
{
	struct dcc_device *dcc;
	int err;

	dcc = &dcc_device[tty->index];
	if (likely(dcc->refcount++))
		return 0;

	dcc->tty = tty;
	tty->driver_data = dcc;
	dcc_ring_init(&dcc->ring);
	init_completion(&dcc->flush);
	init_completion(&dcc->thread_started);

	dcc->thread = kthread_create(dcc_thread, dcc, "ttyDCC%d", tty->index);
	if (IS_ERR(dcc->thread)) {
		err = PTR_ERR(dcc->thread);
		dcc->thread = NULL;
	} else {
		kthread_bind(dcc->thread, 0);
		wake_up_process(dcc->thread);
		err = wait_for_completion_interruptible(&dcc->thread_started);
	}

	return err;
}

static void dcc_tty_close(struct tty_struct *tty, struct file *file)
{
	struct dcc_device *dcc;

	dcc = tty->driver_data;

	if (!--dcc->refcount)
		if (NULL != dcc->thread)
			kthread_stop(dcc->thread);
}

static struct tty_operations dcc_tty_ops = {
	.open = dcc_tty_open,
	.close = dcc_tty_close,
	.write = dcc_tty_write,
	.put_char = dcc_tty_put_char,
	.flush_chars = dcc_tty_flush,
	.write_room = dcc_tty_write_room,
	.chars_in_buffer = dcc_tty_chars_in_buffer,
	.flush_buffer = dcc_tty_flush,
	.unthrottle = dcc_tty_flush,
};

static void dcc_console_write(struct console *console, const char *buffer,
			      unsigned count)
{
	struct dcc_device *dcc;
	int retry;

	dcc = &dcc_device[console->index];

	while (count--) {
		retry = CONFIG_DCC_DROP;
		while (!dcc_can_write(dcc_status())) {
#if CONFIG_DCC_DROP > 0
			if (!retry--)
				return;
#endif
		}

		if (unlikely(*buffer == '\n')) {
			dcc_write_char('\r');

			retry = CONFIG_DCC_DROP;
			while (!dcc_can_write(dcc_status())) {
#if CONFIG_DCC_DROP > 0
				if (!retry--)
					return;
#endif
			}
		}
		dcc_write_char(*buffer++);
	}
}

static struct tty_driver *dcc_console_device(struct console *console,
					     int *index)
{
	*index = console->index;

	return tty_driver;
}

static int __init dcc_console_setup(struct console *console, char *options)
{
	return console->index >= tty_driver->minor_num ? -ENODEV : 0;
}

static struct console dcc_console_ops = {
	.name = "ttyDCC",
	.write = dcc_console_write,
	.device = dcc_console_device,
	.setup = dcc_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};

static int __init dcc_initialize(void)
{
	int err, num;

	for (num = 0; num < ARRAY_SIZE(dcc_device); num += 1)
		dcc_device[num].refcount = 0;

	tty_driver = alloc_tty_driver(num);
	if (NULL == tty_driver) {
		err = -ENOMEM;
		goto bail_out;
	}

	tty_driver->owner = THIS_MODULE;
	tty_driver->driver_name = "dcc";
	tty_driver->name = "ttyDCC";
	tty_driver->name_base = 0;
	tty_driver->major = dcc_major;
	tty_driver->minor_start = 0;
	tty_driver->minor_num = num;
	tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	tty_driver->subtype = SERIAL_TYPE_NORMAL;
	tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW;
	tty_driver->init_termios = tty_std_termios;

	tty_set_operations(tty_driver, &dcc_tty_ops);

	err = tty_register_driver(tty_driver);
	if (err)
		goto bail_out;

	register_console(&dcc_console_ops);

	err = 0;

bail_out:
	return err;
}

static void __exit dcc_finalize(void)
{
	if (NULL != tty_driver)
		tty_unregister_driver(tty_driver);
}

module_init(dcc_initialize);
module_exit(dcc_finalize);

module_param_named(major, dcc_major, int, S_IRUGO);

MODULE_AUTHOR("Arnaud TROEL - Copyright (C) 2009 ST-Ericsson");
MODULE_DESCRIPTION("Generic driver for JTAG DCC");
MODULE_LICENSE("GPL");
