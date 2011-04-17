/*
 * vmon_pipe.c -- fifo driver for vmon
 *
 * Copyright (C) 2009 ST-Ericsson
 *
 * Part of this code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 * Part of this code comes from oprofile (included in linux-2.6.27)
 * linux/drivers/oprofile/event_buffer.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define VMON_PIPE_C

#define __NO_VERSION__          /* It's not THE file of the kernel module */

#include <linux/device.h>
#include <linux/kernel.h>       /* printk(), min(), snprintf() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t, dev_t */
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>		/* needed for copy_to_user */
#include <asm/atomic.h>			/* test_and_set_bit */

#include <linux/wait.h>

#include <nk/vmon.h>

#define VMON_P_NR_DEVS      1   /* number of pipe devices */

/* structures */
struct vmon_pipe {
	unsigned long buffer_opened;       /* explicit, isn't it? */
	wait_queue_head_t buffer_wait;     /* read queue */
	unsigned long long *buffer;             /* buffer of 4 byte elements */
	int buffersize;				/* nb of elements */
	unsigned long buffer_watershed;		/* limit before buf overflow */
    size_t buffer_pos;                 /* aligned to unsigned long, like buffersize */
    struct cdev cdev;                  /* Char device structure */
};

/* wait_event checks it outside of buffer_mutex */
static atomic_t buffer_ready = ATOMIC_INIT(0);
DEFINE_MUTEX(buffer_mutex);

/* parameters */
dev_t vmon_p_devno;                       /* Our first device number */

static struct vmon_pipe *vmon_p_devices;

/* functions */
static int vmon_p_open(struct inode *inode, struct file *filp);
static int vmon_p_release(struct inode *inode, struct file *filp);
static ssize_t vmon_p_read(struct file *filp, \
		char __user *buf, size_t count, loff_t *f_pos);

/** 
 * called by vmon thread to fill buffer
 * results will be written on output file
 * 
 * @return bytes written
 */
ssize_t vmon_write_to_buffer(unsigned long long sample)
{
	struct vmon_pipe *dev = vmon_p_devices;

	prolog ( "dev->buffer=0x%p", dev->buffer );

	/* FIXME: is mutex lock enough to access vmon_p_devices safely? */
	if (dev->buffer_pos == dev->buffersize) {
		/* do we need a wait_interruptible(kthread) in this case? */
		critical ( "overflow! position=%d", dev->buffer_pos );
		return 0;
	}

	dev->buffer[dev->buffer_pos] = sample;
	if (++(dev->buffer_pos) == dev->buffersize - dev->buffer_watershed) {
		atomic_set(&buffer_ready, 1);
		wake_up(&dev->buffer_wait);
	}

	return sizeof(unsigned long long);
}

/*
 * This is called whenever a process attempts to open the device file
 */
static int vmon_p_open(struct inode *inode, struct file *filp)
{
	struct vmon_pipe *dev; /* device information */

	prolog ("");

	dev = container_of(inode->i_cdev, struct vmon_pipe, cdev);
	filp->private_data = dev; /* for other methods */

	/* one open allowed, no reason to have more */
	if (test_and_set_bit(0, &dev->buffer_opened))
		return -EBUSY;

	return nonseekable_open(inode, filp);
}

static int vmon_p_release(struct inode *inode, struct file *filp)
{
	struct vmon_pipe *dev = filp->private_data;

	prolog ("");
	clear_bit(0, &dev->buffer_opened);
	
	return 0;
}

/*
 * This is called whenever a process attempts to read the device file
 */
static ssize_t vmon_p_read(struct file *filp, \
		char __user *buf, size_t count, loff_t *f_pos)
{
	struct vmon_pipe *dev = filp->private_data;
	int retval = -EINVAL;
	size_t const max = dev->buffersize * sizeof(unsigned long long);

	prolog ("");

	/* handling partial reads is more trouble than it's worth */
	if (count != max || *f_pos)
		return -EINVAL;
	
	wait_event_interruptible(dev->buffer_wait, atomic_read(&buffer_ready));
	if (signal_pending(current))
		return -EINTR;
	
	/* can't currently happen */
	if (!atomic_read(&buffer_ready))
		return -EAGAIN;

	mutex_lock(&buffer_mutex);
	atomic_set(&buffer_ready, 0);

	retval = -EFAULT;

	/* buffer_pos unit is unsigned long (4Bytes)
	 * count unit is Bytes */
	count = dev->buffer_pos * sizeof(unsigned long long);
 
	if (copy_to_user(buf, dev->buffer, count))
		goto out;

	epilog("\"%s\" did read %li bytes\n", current->comm, (long long)count);
	/* we expect the user always reads the entire buffer */
	
	retval = count;
	dev->buffer_pos = 0;
 
out:
	mutex_unlock(&buffer_mutex);
	epilog ("");
	return retval;
}

/* FIXME not used for now */
static loff_t vmon_p_seek (struct file *filp, loff_t offset, int whence)
{
	struct vmon_pipe *dev = filp->private_data;
	loff_t newpos;

	switch (whence) {
	case SEEK_SET:
		newpos = offset;
		break;
	default:
		critical ( "seek operation not allowed!" );
		return -EPERM;
	}

	if (newpos < 0)
		return -EINVAL;

	/* not used */
	filp->f_pos = newpos;
	/* FIXME: is it necessary? */
	dev->buffer_pos = offset;
	return newpos;
}


/*
 * This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions.
 */
const struct file_operations vmon_pipe_fops = {
	.open = vmon_p_open,
	.release = vmon_p_release,	/* a.k.a. close */
	.read = vmon_p_read,
	.llseek = vmon_p_seek,
    .owner= THIS_MODULE
};

/* Register device that userspace will read to store results in log file */
int vmon_p_init(dev_t dev)
{
	int result, devno;

	prolog ("");

	vmon_p_devno = dev;

	vmon_p_devices = \
		kmalloc(VMON_P_NR_DEVS * sizeof(struct vmon_pipe), GFP_KERNEL);

	if (vmon_p_devices == NULL) {
		critical ( "vmon_pipe couldn't be allocated!" );
		unregister_chrdev_region(dev, VMON_P_NR_DEVS);
		return 0;
	}

	prolog ( "vmon_p_devices=0x%p", vmon_p_devices );
	
	memset(vmon_p_devices, 0, VMON_P_NR_DEVS * sizeof(struct vmon_pipe));
	init_waitqueue_head(&vmon_p_devices->buffer_wait);
	vmon_p_devices->buffer_watershed = VMON_P_BUFFER_WATERSHED;

	if (vmon_p_devices->buffer_watershed >= VMON_P_BUFFERSIZE)
		return -EINVAL;

	if (!vmon_p_devices->buffer) {
		/*
		 * kmalloc (kernel)
		 * allocates contiguous memory, up to 128KB
		 * vmalloc (virtual)
		 * allocates non continuous memory, can go above 128KB
		 */

		vmon_p_devices->buffer = \
		kmalloc(sizeof(u64) * VMON_P_BUFFERSIZE, GFP_KERNEL);

		if (!vmon_p_devices->buffer) {
			critical ( "couldn't allocate vmon_p_buffer!" );
			return -ENOMEM;
		}
	}
	prolog ( "vmon_p_devices->buffer=0x%p", vmon_p_devices->buffer );
	vmon_p_devices->buffersize = VMON_P_BUFFERSIZE;

	devno = MKDEV(vmon_major, vmon_minor + 0);
	cdev_init(&vmon_p_devices->cdev, &vmon_pipe_fops);
    vmon_p_devices->cdev.owner = THIS_MODULE;
    result = cdev_add (&vmon_p_devices->cdev, devno, 1);

	/* Fail gracefully if need be */
    if (result)
		critical ("error %d adding /dev/vmon", result);

	epilog ( "device vmon major=%d, minor=%d", vmon_major, vmon_minor );
	return result;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void vmon_p_cleanup(void)
{
	prolog ("");

	if (!vmon_p_devices) {
		critical ( "vmon_p_devices already freed!" );
		return; /* nothing else to release */
	}

	if (!vmon_p_devices->buffer) {
		kfree(vmon_p_devices->buffer);
		vmon_p_devices->buffer = NULL;
	}
	vmon_p_devices->buffer_pos = 0;
	atomic_set(&buffer_ready, 0);

	cdev_del(&vmon_p_devices->cdev);
	kfree(vmon_p_devices->buffer);
	kfree(vmon_p_devices);
	unregister_chrdev_region(vmon_p_devno, VMON_P_NR_DEVS);
	vmon_p_devices = NULL; /* pedantic */
	epilog ("");
}


