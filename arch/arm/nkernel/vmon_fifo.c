/*
 * vmon_fifo.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: B. Colau <bertrand.colau-nonst@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#define VMON_C

#include <linux/types.h>	/* needed for bool */
#include <linux/kernel.h>
#include <linux/kfifo.h>	/* needed for kfifo */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <nk/vmon.h>

#define VMON_FIFO_KERNEL_SIZE (48 * 1024)

pid_t myglobalpid;

static struct kfifo *vmon_fifo_kernel;
unsigned long int vmon_buf_linux[VMON_FIFO_KERNEL_SIZE];

int vmon_kernel_fifo_init(unsigned int *init_base_adr)
{
	int error = 1;
	/* create fifo spinlock */
	DEFINE_SPINLOCK(vmon_fifo_lock);

	/* create kernel fifo */
	vmon_fifo_kernel = \
		kfifo_alloc(VMON_FIFO_KERNEL_SIZE * sizeof(unsigned long long),\
				GFP_KERNEL, &vmon_fifo_lock);

	if (vmon_fifo_kernel == ERR_PTR(-ENOMEM)) {
		error = -ENOMEM;
		goto bail_out;
	}

	memset(vmon_buf_linux, 0 , VMON_FIFO_KERNEL_SIZE);

	base_adr = init_base_adr;

	/* create buffer used by kthread */
	/*vmon_buf_linux=
	 * kmalloc(VMON_FIFO_KERNEL_SIZE * sizeof(unsigned int),GFP_KERNEL);*/

	/*if(vmon_buf_linux == NULL)
		printk("error vmon buffer\n");
	*/

bail_out:
	return error;
}

void vmon_kernel_fifo_exit(void)
{
	/* free kernel fifo */
	kfree(vmon_fifo_kernel);

	/*free buf linux */
	kfree(vmon_buf_linux);
}

unsigned int vmon_kernel_fifo_len(void)
{

    return kfifo_len(vmon_fifo_kernel);

}

void vmon_fill_kernel_fifo(pid_t pid)
{

	u64 kernelTimeStamp;
	u64 vmon_buf;
	DEFINE_SPINLOCK(vmon_lock);
	unsigned long flags;

	spin_lock_irqsave(vmon_lock, flags);

	myglobalpid = pid;

	kernelTimeStamp = vmon_stamp();

	vmon_buf =  (kernelTimeStamp << 32) | myglobalpid ;

	if (vmon_kernel_fifo_len() < \
			VMON_FIFO_KERNEL_SIZE * sizeof(unsigned long long)) {
		kfifo_put(vmon_fifo_kernel, \
				(unsigned char *) &vmon_buf, sizeof(vmon_buf));
		*base_adr = myglobalpid;
	} else
		critical("kfifo overflow! \n");

	spin_unlock_irqrestore(vmon_lock, flags);

}

void probe_sched_switch_vmon(struct rq *__rq, struct task_struct *prev,
			struct task_struct *next)
{
	u64 kernelTimeStamp;
	u64 vmon_buf;
	DEFINE_SPINLOCK(vmon_lock);
	unsigned long flags;

	spin_lock_irqsave(vmon_lock, flags);

	myglobalpid = prev->pid;

	kernelTimeStamp = vmon_stamp();

	vmon_buf =  (kernelTimeStamp << 32) | myglobalpid ;

	if (vmon_kernel_fifo_len() < \
			VMON_FIFO_KERNEL_SIZE * sizeof(unsigned long long)) {
		kfifo_put(vmon_fifo_kernel, \
				(unsigned char *) &vmon_buf, sizeof(vmon_buf));
		*base_adr = myglobalpid;
	} else
		critical("kfifo overflow! \n");

	spin_unlock_irqrestore(vmon_lock, flags);
}

unsigned long long *vmon_kernel_fifo_get(unsigned * len)
{

	*len = kfifo_len(vmon_fifo_kernel);

	if (*len > 0) {
		kfifo_get(vmon_fifo_kernel , \
				(unsigned char *) &vmon_buf_linux, \
				*len * sizeof(unsigned long long));

		return (unsigned long long *) &vmon_buf_linux;
	} else
		return (void *) 0L;
}

void vmon_kernel_fifo_reset(void)
{

	kfifo_reset(vmon_fifo_kernel);
	return ;
}

