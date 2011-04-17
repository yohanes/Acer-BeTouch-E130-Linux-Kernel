/*
 * Linux kernel module for virtual 07.10 MUX management on 
 * NXP Semiconductors Cellular System Solution 7210 Linux.
 *
 * Author: Arnaud TROEL (arnaud.troel@nxp.com)
 * Copyright (c) NXP Semiconductors 2007.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/init.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>

#define DRIVER_NAME "vmux"
#define VMUX_MAX_COUNT 16
#define VMUX_MIN_COUNT 1

/* 9P packets pool size depending on the ldisc */
#define NP_AT_PKT	1
#define NP_PPP_PKT	32


static int vmux_major = 0;
static int vmux_count = CONFIG_VMUX_CHANNELS_COUNT;
static int vmux_debug = 0;
module_param_named ( major, vmux_major, int, S_IRUGO );
module_param_named ( count, vmux_count, int, S_IRUGO );
module_param_named ( debug, vmux_debug, int, S_IRUGO );



#if defined(CONFIG_VMUX_DEBUG)

#define PTRACE(fmt, arg...) printk ( fmt, ## arg )

#else

#define PTRACE(fmt, args...) /* nothing, it's a placeholder */

#endif /* defined(CONFIG_VMUX_DEBUG) */

#define prolog(fmt, args...) \
    do \
        if ( vmux_debug & 1 )\
            PTRACE ( KERN_DEBUG "> %s " fmt "\n", __func__, ## args );\
    while ( 0 )

#define epilog(fmt, args...) \
    do \
        if ( vmux_debug & 2 )\
            PTRACE ( KERN_DEBUG "< %s " fmt "\n", __func__, ## args );\
    while ( 0 )

#define critical(fmt, args...) \
    printk ( KERN_ERR "! %s " fmt "\n", __func__, ## args )

#define warning(fmt, args...) \
    printk ( KERN_WARNING "? %s " fmt "\n", __func__, ## args )




/* global structure */
struct vmux_req_t {
	struct list_head list;
	int tag;
	int count;
	void *data;

	struct channel_t *channel;
};

struct channel_t {
    struct tty_struct *tty;

	struct p9_fid *fid;

	struct vmux_req_t twrite[NP_PPP_PKT];
	struct list_head list_free_twr;
	struct list_head list_unused_twr;
	int wr_count;
	int nb_twr;

	struct vmux_req_t tread[NP_PPP_PKT];
	struct list_head list_full_trd;
	struct list_head list_free_trd;
	struct list_head list_unused_trd;
	int nb_trd;

	int ldisc_num;
    int rx_stop;
    int ref_count;
    unsigned short index;
};

struct vmux_t {
	struct tty_driver *tty;

	int nchannel;
	struct channel_t channel[VMUX_MAX_COUNT];

	struct p9_client *client;
	struct p9_fid *fid;
    
	int ref_count;
	struct semaphore open_mutex;
    spinlock_t lock;
};

static void vmux_np_read_cb(int error, u32 count, void *data, void *cookie)
{
	struct tty_struct *tty;
	struct channel_t *channel;
	struct vmux_req_t *req = (struct vmux_req_t *)cookie;
	struct vmux_t *vmux;
	int size = 0;

	channel = req->channel;
	tty = channel->tty;
	vmux = tty->driver->driver_state;

    prolog("chan=%d b=%p c=%d", channel, data, count);
    
	if (error) {
		warning("error = %d\n", error);
		goto job_done;
}

	if (!data)
		goto job_done;

	spin_lock(&vmux->lock);
	if (!req->data)
		channel->nb_trd--;
	spin_unlock(&vmux->lock);

	size = tty_insert_flip_string(tty, data, count);
	tty_flip_buffer_push(tty);

	if (unlikely(size != count)) {
		if (size >= 0) {
			req->count = count > size ? count - size : count;
			req->data = kmalloc(req->count, GFP_KERNEL);
			memcpy(req->data, data + size, count - size);
		}
		spin_lock(&vmux->lock);
		list_add_tail(&req->list, &channel->list_full_trd);
		spin_unlock(&vmux->lock);
		goto bail_out;
}

job_done:

	spin_lock(&vmux->lock);
	if (unlikely(channel->nb_trd >= channel->ldisc_num)) {
		list_add_tail(&req->list, &channel->list_unused_trd);
		spin_unlock(&vmux->lock);
		goto bail_out;
}

	if (unlikely(channel->rx_stop)) {
		list_add_tail(&req->list, &channel->list_free_trd);
		spin_unlock(&vmux->lock);
		goto bail_out;
}

	channel->nb_trd++;
	spin_unlock(&vmux->lock);

	if (req->data) {
		kfree(req->data);
		req->count = 0;
		req->data = NULL;
	}

	p9_client_post_aio(channel->fid, req->tag, 0,
					vmux->client->msize - P9_IOHDRSZ,
					NULL, vmux_np_read_cb, req);

bail_out:
    epilog ( "" );

	return;
}

static void vmux_np_write_cb(int error, u32 count, void *data, void *cookie)
{
	struct tty_struct *tty;
	struct channel_t *channel;
	struct vmux_req_t *req = (struct vmux_req_t *)cookie;
	struct vmux_t *vmux;

	channel = req->channel;
	tty = channel->tty;
	vmux = tty->driver->driver_state;

    prolog("chan=%d b=%p c=%d", channel, data, count);

	if (error)
		warning("Rwrite returned error %d\n", error);

	if (count != req->count)
		warning("%d bytes written, should be %d\n", req->count, count);

	req->data = NULL;
	spin_lock(&vmux->lock);
	channel->wr_count -= req->count;
	req->count = 0;
	if (--channel->nb_twr >= channel->ldisc_num)
		list_add_tail(&req->list, &channel->list_unused_twr);
	else
		list_add_tail(&req->list, &channel->list_free_twr);
	spin_unlock(&vmux->lock);
	tty_wakeup(tty);
    
    epilog ( "" );

	return;
}


/* tty call-backs */
static int
vmux_tty_write_room ( struct tty_struct * tty )
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
	int length;

	spin_lock(&vmux->lock);
	if (channel->nb_twr > channel->ldisc_num)
		length = 0;
	else
		length = channel->ldisc_num - channel->nb_twr;
	spin_unlock(&vmux->lock);
    
	return length * (vmux->client->msize - P9_IOHDRSZ);
}

static int
vmux_tty_chars_in_buffer ( struct tty_struct * tty )
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
    int length;

	spin_lock(&vmux->lock);
	length = channel->wr_count;
	spin_unlock(&vmux->lock);

    return length;
}

static int
vmux_tty_write ( struct tty_struct * tty, 
                 const unsigned char * buffer, 
                 int count )
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
    int retval = 0;
    
    prolog("t=%p b=%p c=%d", tty, buffer, count);

	spin_lock(&vmux->lock);
    
	while (count > 0) {
        int len;
		struct vmux_req_t *req;
        
		if (list_empty(&channel->list_free_twr))
			goto end;
		req = list_first_entry(&channel->list_free_twr,
					struct vmux_req_t, list);
		list_del(&req->list);
        
		if (count > vmux->client->msize - P9_IOHDRSZ)
			len = vmux->client->msize - P9_IOHDRSZ;
                else
                    len = count;
                
		req->count = len;
		channel->nb_twr++;
		p9_client_post_aio(channel->fid, req->tag, 0, len,
					(char *)buffer, vmux_np_write_cb, req);
                
		channel->wr_count += len;
		count -= len;
                retval += len;
                buffer += len;
            }

end:
	spin_unlock(&vmux->lock);

    epilog ( "%d", retval );

    return retval;
}

static void
vmux_tty_throttle ( struct tty_struct * tty )
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
    
	prolog("channel = %d", tty->index);

	spin_lock(&vmux->lock);
	channel->rx_stop = 1;
	spin_unlock(&vmux->lock);
    
	epilog("");
}

static void
vmux_tty_unthrottle ( struct tty_struct * tty )
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
	struct vmux_req_t *req;
    
	prolog("channel = %d", tty->index);

	spin_lock(&vmux->lock);
	channel->rx_stop = 0;

	while ((!list_empty(&channel->list_full_trd))
			&& (channel->rx_stop == 0)) {

		req = list_first_entry(&channel->list_full_trd,
						struct vmux_req_t, list);
		list_del(&req->list);
		vmux_np_read_cb(0, req->count, req->data, req);
	}

	while ((!list_empty(&channel->list_free_trd))
			&& (channel->rx_stop == 0)) {

		req = list_first_entry(&channel->list_free_trd,
						struct vmux_req_t, list);
		list_del(&req->list);
		vmux_np_read_cb(0, 0, NULL, req);
	}
    
	spin_unlock(&vmux->lock);

	epilog("");
}
    

/* This callback will never be called.
 * We need to implement sysfs attribute to set the packet number,
 * and to add latency field in 9P requests in order to
 * have dynamic latency setting.
 * We keep this function as example.
 */
static void vmux_tty_set_ldisc(struct tty_struct *tty)
{
	struct vmux_t *vmux = tty->driver->driver_state;
	struct channel_t *channel = &vmux->channel[tty->index];
	struct vmux_req_t *req;

	spin_lock(&vmux->lock);

	if (tty->termios->c_line == N_PPP) {
		channel->ldisc_num = NP_PPP_PKT;
		while (!list_empty(&channel->list_unused_trd)) {
			req = list_first_entry(&channel->list_unused_trd,
						struct vmux_req_t, list);
			list_del(&req->list);
			vmux_np_read_cb(0, 0, NULL, req);
		}
		while (!list_empty(&channel->list_unused_twr)) {
			req = list_first_entry(&channel->list_unused_twr,
						struct vmux_req_t, list);
			list_del(&req->list);
			list_add_tail(&req->list, &channel->list_free_twr);
		}
	} else
		channel->ldisc_num = NP_AT_PKT;

	spin_unlock(&vmux->lock);
}

static struct vmux_t vmux_global;

static int vmux_tty_open(struct tty_struct *tty,
                struct file * file )
{
    struct vmux_t *vmux = tty->driver->driver_state;
    struct channel_t * channel = &vmux->channel[tty->index];
	char *path[] = { "vmux", "255" };
    int i, retval = 0;

    /* enter open/close critical section */
    if ( down_interruptible ( &vmux->open_mutex ) )
        return -ERESTARTSYS;
    
    prolog ( "t=%p f=%p", tty, file );

    /* check for first open of the channel */
    if (channel->ref_count++ == 0) {
		if (unlikely(!vmux->client)) {
			vmux->client = p9_client_create(DRIVER_NAME,
								"trans=xoscore,"
								"noextend,"
								"latency=0");
			if (IS_ERR(vmux->client)) {
				retval = PTR_ERR(vmux->client);
				vmux->client = NULL;
				critical("cannot create 9P client");
            }
        }

		if (unlikely(!vmux->fid)) {
			struct p9_fid *fid;
			fid = p9_client_attach(vmux->client, NULL,
								"nobody",
								0, "");

			if (IS_ERR(fid)) {
				retval = PTR_ERR(fid);
				critical("cannot attach remote 9P file system");
                goto bail_out;
            }
			vmux->fid = fid;
        }

		snprintf(path[1], 3, "%u", channel->index);
		channel->fid = p9_client_walk(vmux->fid,
							ARRAY_SIZE(path),
							path, 1);

		if (unlikely(IS_ERR(channel->fid))) {
			retval = PTR_ERR(channel->fid);
			channel->fid = NULL;
			critical("cannot walk remote 9P file /%s/%s",
					path[0], path[1]);

                goto bail_out;
            }

		retval = p9_client_open(channel->fid, P9_ORDWR);
		if (unlikely(retval)) {
			p9_client_clunk(channel->fid);
			channel->fid = NULL;
			retval = -ENODEV;
			channel->ref_count = 0;
			critical("cannot open remote 9P file /%s/%s",
					path[0], path[1]);

			goto bail_out;
		}

		spin_lock(&vmux->lock);
		INIT_LIST_HEAD(&channel->list_free_twr);
		INIT_LIST_HEAD(&channel->list_unused_twr);
		for (i = 0; i < ARRAY_SIZE(channel->twrite); i++) {
			INIT_LIST_HEAD(&channel->twrite[i].list);
			channel->twrite[i].tag =
				p9_client_create_aio(channel->fid);

			channel->twrite[i].channel = channel;
		}

		INIT_LIST_HEAD(&channel->list_unused_trd);
		INIT_LIST_HEAD(&channel->list_free_trd);
		INIT_LIST_HEAD(&channel->list_full_trd);
		for (i = 0; i < ARRAY_SIZE(channel->tread); i++) {
			INIT_LIST_HEAD(&channel->tread[i].list);
			channel->tread[i].tag =
				p9_client_create_aio(channel->fid);
	
			channel->tread[i].channel = channel;
		}

		channel->nb_twr = 0;
		channel->nb_trd = 0;
		channel->rx_stop = 0;

        channel->tty = tty;
        tty->driver_data = channel;

        tty->low_latency = 1;

		vmux->ref_count++;

		if (tty->termios->c_line == N_PPP)
			channel->ldisc_num = NP_PPP_PKT;
		else
			channel->ldisc_num = NP_AT_PKT;
            
		for (i = 0; i < channel->ldisc_num; i++)
			list_add_tail(&channel->twrite[i].list,
						&channel->list_free_twr);
		for (; i < ARRAY_SIZE(channel->twrite); i++)
			list_add_tail(&channel->twrite[i].list,
						&channel->list_unused_twr);
                            
		for (i = 0; i < channel->ldisc_num; i++)
			vmux_np_read_cb(0, 0, NULL, &channel->tread[i]);
		for (; i < ARRAY_SIZE(channel->tread); i++)
			list_add_tail(&channel->tread[i].list,
						&channel->list_unused_trd);
		spin_unlock(&vmux->lock);
    }
 
bail_out:
    epilog ( "%d", retval );
    
    /* leave critical section */
    up ( &vmux->open_mutex );
    
    return retval;
}

static void vmux_tty_close(struct tty_struct *tty,
                 struct file * file )
{
	struct vmux_t *vmux = tty->driver->driver_state;
    struct channel_t * channel = &vmux->channel[tty->index];
	int i;
    
    down ( &vmux->open_mutex );
    
    prolog ( "t=%p f=%p", tty, file );

	if (channel->ref_count == 1) {
		channel->ref_count = 0;
        
		if (channel->fid) {
			for (i = 0; i < ARRAY_SIZE(channel->twrite); i++) {
				p9_client_destroy_aio(channel->fid,
							channel->twrite[i].tag);
				channel->twrite[i].tag = 0;
			}

			for (i = 0; i < ARRAY_SIZE(channel->tread); i++) {
				p9_client_destroy_aio(channel->fid,
							channel->tread[i].tag);
				channel->tread[i].tag = 0;
			}

			p9_client_clunk(channel->fid);

			channel->fid = NULL;
        channel->tty = NULL;
		}
        /* check for very last close */
        if ( vmux->ref_count == 1 )
			vmux->ref_count = 0;
		else if (vmux->ref_count > 1)
			vmux->ref_count--;
		}
	else if (channel->ref_count > 1) {
		/* decrease channel use */
		channel->ref_count--;
	}
    
    epilog ( "" );

    up ( &vmux->open_mutex );
}

static struct tty_operations vmux_serial_ops = {
    .open            = vmux_tty_open,
    .close           = vmux_tty_close,
    .write           = vmux_tty_write,
    .write_room      = vmux_tty_write_room,
    .chars_in_buffer = vmux_tty_chars_in_buffer,
    .throttle        = vmux_tty_throttle,
    .unthrottle      = vmux_tty_unthrottle,
	.set_ldisc		 = vmux_tty_set_ldisc,
};

/* Platform Driver Definitions */

static int vmux_serial_probe(struct platform_device *pdev)
{
	struct vmux_t * vmux = &vmux_global;
	int i;

	dev_info(&pdev->dev, "initializing\n");

	for (i = 0; i < vmux->tty->num; i++)
		tty_register_device(vmux->tty, i, &pdev->dev);

	return 0;
}

static int vmux_serial_remove(struct platform_device *pdev)
{
	struct vmux_t * vmux = &vmux_global;
	int i;

	for (i = 0; i < vmux->tty->num; i++)
		tty_unregister_device(vmux->tty, i);

	return 0;
}

static struct platform_driver vmux_serial_driver = {
	.probe		= vmux_serial_probe,
	.remove		= vmux_serial_remove,

	.driver 	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

/* Platform Device Definitions */

static void vmux_device_release(struct device *dev)
{
	/* Nothing to be released */
}

static struct platform_device vmux_serial_device = {
	.name		= DRIVER_NAME,
	.id		= 0,
	.dev		= {
		.release = vmux_device_release,
	},
};

/* Tty initializatioin */

static int vmux_initialize ( void )
{
    int retval, i;
    struct vmux_t * vmux = &vmux_global;
    
    prolog ( "" );

    /* nice message for debug */
#if !defined(CONFIG_VMUX_DEBUG)
    if ( vmux_debug != 0 )
        warning ( "module was compiled without debug support." );
#endif
    
    /* bound device count */
    if (vmux_count > VMUX_MAX_COUNT)
		vmux_count = VMUX_MAX_COUNT;

    if (vmux_count < VMUX_MIN_COUNT)
		vmux_count = VMUX_MIN_COUNT;

    vmux->nchannel = vmux_count;
    
	for (i = 0; i < vmux->nchannel; ++i) {
        struct channel_t * channel = vmux->channel + i;

        channel->ref_count = 0;
        channel->index     = i;
        channel->tty       = NULL;
    }

	vmux->client = NULL;
	vmux->fid = NULL;

    spin_lock_init ( &vmux->lock );
    init_MUTEX ( &vmux->open_mutex );
    vmux->ref_count = 0;
    
    /* allocate tty driver */
    vmux->tty = alloc_tty_driver ( vmux->nchannel );
    if (vmux->tty == NULL) {
        critical ( "failed to allocate tty driver." );
        retval = -ENOMEM;
        goto bail_out;
    }

    /* initialize the tty driver */
    vmux->tty->owner        = THIS_MODULE;
    vmux->tty->driver_name  = DRIVER_NAME;
    vmux->tty->name         = "ttyU";
    vmux->tty->name_base    = 0;
    vmux->tty->major        = vmux_major;
    vmux->tty->minor_start  = 0;
    vmux->tty->minor_num    = VMUX_MAX_COUNT;
    vmux->tty->type         = TTY_DRIVER_TYPE_SERIAL;
    vmux->tty->subtype      = SERIAL_TYPE_NORMAL;
    vmux->tty->flags        = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    vmux->tty->init_termios = tty_std_termios;

    /* force standard control settings */
    vmux->tty->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    
    /* force echo disabled */
    vmux->tty->init_termios.c_lflag &= ~( ECHO | ECHOE | ECHOK | ECHONL );
    
    tty_set_operations ( vmux->tty, &vmux_serial_ops );
    
	vmux->tty->driver_state = &vmux_global;

    /* register the tty driver */
    retval = tty_register_driver ( vmux->tty );
    if (retval) {
        critical ( "failed to register vmux tty driver." );
        put_tty_driver ( vmux->tty );
        goto bail_out;
    }

bail_out:
    epilog ( "%d", retval );
        
    return retval;
}

static void vmux_finalize ( void )
{
    struct vmux_t * vmux = &vmux_global;

    prolog ( "" );

    /* unregister the tty driver */
    tty_unregister_driver ( vmux->tty );
    put_tty_driver(vmux->tty);
    
    epilog ( "" );
}

static int __init vmux_serial_init(void)
{
	int ret;

	printk(KERN_INFO "Serial: Vmux driver\n");

	ret = vmux_initialize();
	if (ret)
		return ret;

	ret = platform_driver_register(&vmux_serial_driver);
	if (ret)
		goto err_register_vmux_driver;

	ret = platform_device_register(&vmux_serial_device);
	if (ret)
		goto err_register_vmux_device;

	return ret;

err_register_vmux_device:
	platform_driver_unregister(&vmux_serial_driver);

err_register_vmux_driver:
	vmux_finalize();

	return ret;
}

static void __exit vmux_serial_exit(void)
{
	platform_device_unregister(&vmux_serial_device);
	platform_driver_unregister(&vmux_serial_driver);
	vmux_finalize();
}

module_init(vmux_serial_init);
module_exit(vmux_serial_exit);

MODULE_AUTHOR("Arnaud TROEL, Maxime COQUELIN"
			"<{arnaud.troel, maxime.coquelin-nonst}"
			"@stericsson.com>");
MODULE_DESCRIPTION ( "Linux plug-in for virtual 07.10 MUX management on "
                     "NXP Semiconductors Cellular Sy.Sol. 7210 Linux" );
MODULE_LICENSE ( "GPL" );
