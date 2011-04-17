/*
 * Linux plug-in for software inter-OS monitoring on NXP Semiconductors
 * Cellular System Solution 7210 Linux.
 *
 * Copyright (C) 2009 ST-Ericsson
 *
 * Kernel thread timer scheduling courtesy borrowed to Gilles Maigne's
 * VirtualLogix performance monitor.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define VMON_C

#include <linux/kernel.h>  /* printk ( )      */
#include <linux/module.h>  /* MODULE_ macros  */
#include <linux/init.h>    /* __init, __exit  */
#include <linux/kthread.h> /* threading stuff */
#include <asm/div64.h>
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */

#include <nk/xos_fifo.h>   /* inter-OS data support */
#include <nk/xos_ctrl.h>   /* inter-OS ctrl support */
#include <nk/xos_area.h>   /* inter-OS area support */

#include <osware/osware.h>
#include <linux/xoscommon.h>

#include <linux/device.h>
#include <linux/types.h>	/* needed for dev_t */
#include <linux/kdev_t.h>	/* needed for MKDEV */
#include <linux/cdev.h>
#include <linux/kfifo.h>    /* needed for kfifo */
#include <trace/sched.h>

#include <nk/vmon.h>

#define VMON_SHOW_DECIMALS
#define RTK	0x00000000
#define LINUX	0x80000000
#define NOSAMPLE 0xffffffff

unsigned int vmon_major = 0, vmon_minor = 0;
static bool vmon_device_initialized;

static unsigned int vlx_l2m_idle_switch;
static unsigned int vlx_l2m_irq_switch;
static unsigned int vlx_l2m_xirq_switch;
static unsigned int vlx_m2l_irq_switch;
static unsigned int vlx_m2l_idle_switch;

unsigned int vmon_debug;
static unsigned int vmon_version = 2;
static unsigned int vmon_fifoparam = VMON_FIFO_SIZE;
static unsigned int vmon_OnOff;
static unsigned int vmon_outputfile;
static unsigned int sample_origine = RTK;

xos_area_handle_t shared;
unsigned int *base_adr;

static xos_fifo_handle_t vmon_fifo;
static xos_ctrl_handle_t vmon_ctrl;

static struct timer_list vmon_timer;
static struct semaphore vmon_semaphore;

/* 
 * vmon_stats[0]: Idle
 * vmon_stats[1]: RTK
 * vmon_stats[2]: Linux
 * vmon_stats[3]: Total
 * vmon_stats[4]: Task0
 * vmon_stats[5]: Task1
 * (...)
 * vmon_stats[128]: Int0
 * vmon_stats[129]: Int1
 * vmon_stats[253]: stateless_nkidle, vlx_m2l_idle_switch 
 * vmon_stats[252]: nkidle_restart, vlx_m2l_irq_switch
 * vmon_stats[251]: stateless_indidrect_hdl, vlx_l2m_irq_switch
 * vmon_stats[250]: xintrhdl, vlx_l2m_xirq_switch
 *
 * all members should be initialized to 0 with following definition
 */
static unsigned int vmon_stats[VMON_NB_OF_RTK_STATES \
			    + VMON_NB_OF_LINUX_STATES + 1];
static unsigned int vmon_ints[VMON_NB_OF_INTS];
static unsigned int vmon_flow_error;
static unsigned int vmon_timestamp_errors;
static unsigned long long lastsample_RTK = NOSAMPLE;
static unsigned long long lastsample_Linux = NOSAMPLE;
#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
static unsigned vmon_laststate = 0xFF;
static unsigned vmon_lastlaststate = 0xFF;
static unsigned before_overrated_state = 0xFF;
static unsigned vmon_flow_error_state;
static unsigned vmon_dropped_state;
#endif
static struct task_struct *vmon_thread_id;
static unsigned int * vmon_fifosize;
/**
 * This structure hold information about the /proc file
 *
 */
struct proc_dir_entry *vmon_results_proc_file;
struct proc_dir_entry *vmon_on_off_proc_file;
struct proc_dir_entry *vmon_details_proc_file;

static char *find_name_from_thread_id(unsigned int pid)
{

	struct task_struct *task;

	pid -= XOSMON_LINUX_SHIFT;

	if (pid == 0)
		return "swapper";

	for_each_process(task) {
		if (task->pid == pid)
			return task->comm;
	}

	return "";
}

static unsigned int find_thread_id_of_kvmond(void)
{

	struct task_struct *task;

	for_each_process(task) {
		if (!strcmp(task->comm, "kvmond"))
			return task->pid;
	}

	return 0;
}

static int vmon_print_stat_percents_decimals(char *buffer,
		unsigned int stat, unsigned int total,
		const char *name, unsigned int iter)
{
	int ret = 0;
	uint64_t percents =	stat;
	/* Total time is divided by 100 to get rounded
	 * percentages in integer
	 * This prevents calculation error when a state
	 * time is greater than (2^32)/100=42949672us */
	uint64_t remainder = do_div(percents, total);

#ifdef VMON_SHOW_DECIMALS
	/* similar to (remainder *= 100) */
	remainder = (remainder << 6) + (remainder << 5) + (remainder << 2);
	do_div(remainder, total);
	/* 'remainder' contains the decimals, 'percents' the ratio */
	if (iter)
		ret += sprintf(buffer+ret,
				"%s%8uus, %3u.%02u%% %7u\n",
				name, stat, (unsigned int)percents,
				(unsigned int)remainder, iter);
	else
		ret += sprintf(buffer+ret, \
				"%s%8uus, %3u.%02u%%\n", name, stat,
				(unsigned int)percents,
				(unsigned int)remainder);
#else
	ret += sprintf(buffer+ret, "%s%8uus, %3u%%\n", name, stat,
			(unsigned int)percents);
#endif

	return ret;
}


static int vmon_print_switches_freq(char *buffer,
		unsigned int swi, unsigned int total, const char *name)
{
	int ret = 0;
	uint64_t freq = swi*10000;
	
	do_div(freq, total);
	ret += sprintf(buffer+ret, "%s%9uHz,%9u\n",
			name,
			(unsigned int)freq, swi);
	
	return ret;
}


static int vmon_global_results_procfs ( char *buffer )
{
	unsigned int total = vmon_stats[3];
	int ret = 0;

	prolog ( "" );

	if (unlikely(!total)) {
		critical( "impossible, total time is null");
		return ret;
	}

	/* workaround before changing calling scripts */
	if (!vlx_l2m_idle_switch)
		vlx_l2m_idle_switch = vmon_l2m_idle_switch;

	/* Total time is divided by 100 to get rounded percentages in integer
	 * This prevents calculation error when a state
	 * time is greater than (2^32)/100=42949672us */
	do_div(total,100);

	ret += sprintf(buffer+ret, "------ Global states -----------------\n");
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_POWERDOWN_STATE] \
			+ vmon_stats[XOSMON_TASK_SHIFT] \
			+ vmon_stats[XOSMON_LINUX_SHIFT],
			total, "CPU free:  ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_TASK_SHIFT],
			total, "  Task0:   ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_POWERDOWN_STATE],
			total, "  Idle:    ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_LINUX_SHIFT],
			total, "  IdleLinux:", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_RTKE_STATE],
			total, "RTK total:  ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_LINUX_STATE],
			total, "Linux total:",
			vlx_m2l_idle_switch + vlx_m2l_irq_switch);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_STATELESS_NKIDLE],
			total, "  StNkidle:", vlx_m2l_idle_switch);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_NKIDLE_RESTART],
			total, "  NkidleRe:", vlx_m2l_irq_switch);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[find_thread_id_of_kvmond() \
			+ XOSMON_LINUX_SHIFT],
			total, "  kvmond  :", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_INDIRECT_HDL],
			total, "IndirHdl:  ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[XOSMON_XINTRHDL],
			total, "XIntrHdl:  ", 0);
	ret += vmon_print_stat_percents_decimals(buffer+ret,
			vmon_stats[3],
			total, "Total:     ", 0);
	
	ret += sprintf(buffer+ret, "------ Context switches ---------------\n");

	/* EVI: total(us) has already been divided by 100,
	 * then multiply ns by 10000 to get switches/s */
	ret += vmon_print_switches_freq(buffer+ret, vlx_l2m_idle_switch,
			total, "modem2linux idle:");
	ret += vmon_print_switches_freq(buffer+ret,
			vlx_m2l_idle_switch - vlx_l2m_idle_switch,
			total, "modem2nkern only:");
	ret += vmon_print_switches_freq(buffer+ret, vlx_m2l_irq_switch,
			total, "modem2linux irq: ");
	ret += vmon_print_switches_freq(buffer+ret, vlx_l2m_idle_switch,
			total, "linux2modem idle:");
	ret += vmon_print_switches_freq(buffer+ret, vlx_l2m_irq_switch,
			total, "linux2modem irq: ");
	ret += vmon_print_switches_freq(buffer+ret, vlx_l2m_xirq_switch,
			total, "linux2modem xirq:");

	ret += sprintf ( buffer+ret, "------ Errors -------------------------\n");
	ret += sprintf ( buffer+ret, "Overflow:         %10u\n", vmon_flow_error );
	ret += sprintf(buffer+ret, "Timestamp:        %10u\n",
			vmon_timestamp_errors);

#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
	if (vmon_timestamp_errors) {
		ret += sprintf ( buffer+ret, "  Before overrated sta: 0x%04x\n",
			(before_overrated_state < XOSMON_TASK_SHIFT) \
			? before_overrated_state|0xD0 :
			(before_overrated_state < XOSMON_NESTED_SHIFT) \
			? before_overrated_state-XOSMON_TASK_SHIFT :
			(before_overrated_state < XOSMON_INT_SHIFT) \
			? (before_overrated_state-XOSMON_NESTED_SHIFT)<<8 :
			(before_overrated_state < XOSMON_XINTRHDL) \
			? (before_overrated_state-XOSMON_INT_SHIFT)<<8 :
			before_overrated_state);
		ret += sprintf ( buffer+ret, "  Last overrated state: 0x%04x\n",
			(vmon_flow_error_state < XOSMON_TASK_SHIFT) \
			? vmon_flow_error_state|0xD0 :
			(vmon_flow_error_state < XOSMON_NESTED_SHIFT) \
			? vmon_flow_error_state-XOSMON_TASK_SHIFT :
			(vmon_flow_error_state < XOSMON_INT_SHIFT) \
			? (vmon_flow_error_state-XOSMON_NESTED_SHIFT)<<8 :
			(vmon_flow_error_state < XOSMON_XINTRHDL) \
			? (vmon_flow_error_state-XOSMON_INT_SHIFT)<<8 :
				vmon_flow_error_state);
		ret += sprintf ( buffer+ret, "  Last dropped state :  0x%04x\n",
			(vmon_dropped_state < XOSMON_TASK_SHIFT) \
			? vmon_dropped_state|0xD0 :
			(vmon_dropped_state < XOSMON_NESTED_SHIFT) \
			? vmon_dropped_state-XOSMON_TASK_SHIFT :
			(vmon_dropped_state < XOSMON_INT_SHIFT) \
			? (vmon_dropped_state-XOSMON_NESTED_SHIFT)<<8 :
			(vmon_dropped_state < XOSMON_XINTRHDL) \
			? (vmon_dropped_state-XOSMON_INT_SHIFT)<<8 :
				vmon_dropped_state);
	}
#endif

	return ret;
}


static int vmon_detailed_results_procfs ( char *buffer )
{
	int ret = 0;
	unsigned int total = vmon_stats[3];
	unsigned long long i = 4;
	uint64_t remainder, percents;

	if (unlikely(!total)) {
		critical( "impossible, total time is null");
		return ret;
	}

	prolog ( "start, buffer=%p", buffer );

	ret += sprintf(buffer+ret, "-Task/IT|--------Time|---Ratio|--Iters|\n");
	
	if (likely(vmon_stats[4] <= 0))
		return ret;

		/* Report tasks and ints information */
	for ( ; i < (VMON_NB_OF_RTK_STATES + VMON_NB_OF_LINUX_STATES); i++) {
		if (vmon_stats[i] <= 0)
			continue;

/*              |+ Total time is divided by 100 to get rounded
 *              percentages in integer*/
/*              * This prevents calculation error when a state
 *               time is greater than (2^32)/100=42949672us +|*/
/*              do_div(total, 100);*/

				/* similar to (...)*100 */
				percents =	(vmon_stats[i] << 6) +
							(vmon_stats[i] << 5) +
							(vmon_stats[i] << 2);
				remainder = do_div(percents, total);
				/* similar to (remainder *= 100) */
		remainder = (remainder << 6) + \
			    (remainder << 5) + (remainder << 2);
				do_div(remainder, total);
		/* 'remainder' contains the decimals,
		 * 'percents' the ratio */

				/* Handling tasks */
		if (i < XOSMON_NESTED_SHIFT) {
			ret += sprintf(buffer+ret,
					"t 0x%04x %10uus  %3u.%02u%%\n",
					(unsigned int)(i-4),
					vmon_stats[i],
							(unsigned int)percents,
							(unsigned int)remainder );
				/* Handling nested interrupts */
		} else if (i >= XOSMON_NESTED_SHIFT \
				&& i < XOSMON_INT_SHIFT) {
			/* /!\ if int 64 (0x40) gets nested,
			 * vmon_stats[64-64+128]=vmon_stats[128] (0x80)
			 * ghost interrupt will be erroneously
			 * incremented here... */
			vmon_stats[i-XOSMON_NESTED_SHIFT+XOSMON_INT_SHIFT] \
				+= vmon_stats[i];

				/* Handling interrupts */
		} else if (i < XOSMON_XINTRHDL) {
			ret += sprintf(buffer+ret,
					"i 0x%02x00 %10uus  %3u.%02u%% %7u\n",
					(unsigned int)(i-XOSMON_INT_SHIFT),
					vmon_stats[i],
					(unsigned int)percents,
					(unsigned int)remainder,
					(u32)vmon_ints[i-XOSMON_INT_SHIFT]);
		} else if ((i >= XOSMON_LINUX_SHIFT) \
				&& (vmon_version == 3)) {
			ret += sprintf(buffer+ret,
					"l %6d %10uus  %3u.%02u%% %s\n",
					(u32)(i-XOSMON_LINUX_SHIFT),
					vmon_stats[i],
							(unsigned int)percents,
							(unsigned int)remainder,
					find_name_from_thread_id(i));
		}

	}
	
	return ret;

}

static void vmon_reset_stats ( void )
{
	unsigned long long i = 0;

    prolog ( "Stats are being reset!" );

	lastsample_RTK = NOSAMPLE;
	lastsample_Linux = NOSAMPLE;
	vmon_timestamp_errors = 0;

	/* reset context switch counters */
	vmon_l2m_idle_switch = 0;
	vlx_l2m_idle_switch = 0;
	vlx_l2m_irq_switch = 0;
	vlx_l2m_xirq_switch = 0;
	vlx_m2l_irq_switch = 0;
	vlx_m2l_idle_switch = 0;

	for ( ; i < VMON_NB_OF_INTS ; i++) {
		vmon_stats[i] = 0;
		vmon_ints[i] = 0;
	}
	for ( ; i < VMON_NB_OF_LINUX_STATES + VMON_NB_OF_RTK_STATES ; i++)
		vmon_stats[i] = 0;
}


static void vmon_overflow ( unsigned event, void * cookie )
{
	warning ( "Inter-OS fifo overflow." );
	vmon_reset_stats();
	vmon_flow_error += 1;
}


static void vmon_wakeup ( unsigned long data )
{
    up ( &vmon_semaphore );
}


void vmon_write_to_stats ( unsigned int state, unsigned int dt )
{
	switch (state & 0x80000000) {
	case 0x00000000:
		switch (state) {
	case XOSMON_POWERDOWN_STATE:
		vmon_stats[XOSMON_POWERDOWN_STATE] += dt;
		break;
	case XOSMON_RTKE_STATE:
		vmon_stats[XOSMON_RTKE_STATE] += dt;
		break;
	case XOSMON_LINUX_STATE:
		vlx_m2l_idle_switch++;
		vmon_stats[XOSMON_LINUX_STATE] += dt;
		break;
	case XOSMON_TASK0_STATE:
	case XOSMON_TASK_SHIFT:
		/* task 0x0 */
		vmon_stats[XOSMON_TASK_SHIFT] += dt;
		break;
	case XOSMON_STATELESS_NKIDLE:
		vlx_m2l_idle_switch++;
		vmon_stats[XOSMON_STATELESS_NKIDLE] += dt;
		vmon_stats[XOSMON_LINUX_STATE] += dt;
		break;
	case XOSMON_NKIDLE_RESTART:
		vlx_m2l_irq_switch++;
		vmon_stats[XOSMON_NKIDLE_RESTART] += dt;
		vmon_stats[XOSMON_LINUX_STATE] += dt;
		break;
	case XOSMON_INDIRECT_HDL:
		vlx_l2m_irq_switch++;
		vmon_stats[XOSMON_INDIRECT_HDL] += dt;
		break;
	case XOSMON_XINTRHDL:
		vlx_l2m_xirq_switch++;
		vmon_stats[XOSMON_XINTRHDL] += dt;
		break;
	default:
		/* RTK tasks and ints */
		vmon_stats[state] += dt;
		/* added to RTK stats as well */
		vmon_stats[XOSMON_RTKE_STATE] += dt;
		
			if ((state) > XOSMON_INT_SHIFT)
				vmon_ints[(state) - XOSMON_INT_SHIFT]++;

			break;
		}
		break;
	case 0x80000000:
	default:
		vmon_stats[XOSMON_LINUX_SHIFT + (state & 0x7FFFFFFF)] += dt;
		vmon_stats[XOSMON_LINUX_STATE] += dt;
		break;
	}

	vmon_stats[3] += dt;
}

static inline unsigned int results_v3(unsigned int dt, unsigned int state)
		{
	ssize_t count = 0;

	if (vmon_device_initialized) {
		mutex_lock(&buffer_mutex);

		/*
		 * FIXME Forget sample suffering
		 * from non monotonic timestamp
		 */

		if (likely(dt < 15000000)) {
			if (sample_origine == RTK)
				count = vmon_write_to_buffer(lastsample_RTK);
			else {
				lastsample_Linux |= sample_origine;
				count = vmon_write_to_buffer(lastsample_Linux);
		}
		
			epilog("written %d bytes to buffer", count);
		} else {

		/*
		 * We have been handling this bug
		 * (LMSqb95780) for a long time...
		 */

		count = vmon_write_to_buffer(NOSAMPLE);
		vmon_timestamp_errors++;
	}
	
		mutex_unlock(&buffer_mutex);
		return count;

	} else {

		/*
		 * FIXME Forget sample suffering
		 * from non monotonic timestamp
		 */

		if (likely(dt < 15000000)) {
			vmon_write_to_stats(state | sample_origine, dt);

		} else {

			/*
			 * We have been handling this
			 * bug (LMSqb95780) for a
			 * long time...
			 */

			vmon_timestamp_errors++;
#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
			before_overrated_state = vmon_lastlaststate;
			vmon_flow_error_state = vmon_laststate;
			vmon_dropped_state = lastsample_RTK & VMON_STATES_MASK;
#endif
}

#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
		vmon_lastlaststate = vmon_laststate;
		vmon_laststate = lastsample_RTK & VMON_STATES_MASK;
#endif
	}
return 1;
}

static int vmon_thread_v3(void *cookie)
{
    unsigned int timeout, mintimeout;
	unsigned size;

	prolog ( "" );

    sema_init ( &vmon_semaphore, 0 );

    init_timer ( &vmon_timer );
    vmon_timer.data     = 0;
    vmon_timer.function = vmon_wakeup;
	
	/* HZ=100 on 6710 Icebird, then timeout=128 */
    /* for ( timeout = 1; HZ < timeout; timeout *= 2 ); */

	/* timeout every second */	
	timeout = HZ;

	mintimeout = timeout / 16;

	if (!mintimeout)
	mintimeout = 8;

	vmon_fifoparam = *vmon_fifosize;
	prolog ( "FIFO size = %d Bytes", vmon_fifoparam );
	prolog ( "HZ = %d timer interrupts per second", HZ );
	
	report ( "timeout = %d ticks", timeout );

	while (!kthread_should_stop()) {
		unsigned len_RTK , len_linux = 0 ;
		unsigned long long *buf_linux , *buf;

		/* From VirtualLogix perf monitor */
		/* Note that FIFO filling status "never" reaches 75%
		 * thus timeout is set to HZ above */

		if ((xos_fifo_count(vmon_fifo) >= (*vmon_fifosize) * 3 / 4) \
			|| (vmon_kernel_fifo_len() > 48 * 1024 * 3 / 4)) {

		    timeout /= 2;

			if (timeout < mintimeout)
				timeout = mintimeout;

		} else if (timeout < HZ)

		    timeout *= 2;

		while (likely((buf_linux = vmon_kernel_fifo_get(&len_linux)) \
			&& (buf = xos_fifo_decommit(vmon_fifo, &len_RTK)))) {
			unsigned int t0 = 0, t1, dt, state;

			size = len_RTK;

			while (len_RTK || len_linux) {

				if (likely((lastsample_RTK != NOSAMPLE) \
					|| (lastsample_Linux != NOSAMPLE))) {

					/* 32bit timestamp */

					if (((((*buf) >> 32) < \
						((*buf_linux) >> 32)) && \
						(len_RTK)) || (!len_linux)) {

						t1 = ((*buf) >> 32);
						state = lastsample_RTK \
							& VMON_STATES_MASK;
						sample_origine = RTK;
						if (state & 0x80000000)
							vlx_m2l_irq_switch++;

					} else {
						t1 = ((*buf_linux) >> 32);
						state = lastsample_Linux\
							& VMON_STATES_MASK;
						sample_origine = LINUX;
					}

					dt = t1 - t0;

					/*
					 * Convert timestamp into us
					 * since it is not done
					 * in xosmon anymore
					 */

					dt = dt - dt/13;
	
					if (!results_v3(dt, state)) {
						/* len is not null here,
						 * then deduct it */
						xos_fifo_release(vmon_fifo,
								size - len_RTK);
						goto bed;
					}

					if (sample_origine == RTK) {
						len_RTK -= sizeof(u64);
						lastsample_RTK = *buf;
						buf += 1;
					} else {
						lastsample_Linux = *buf_linux;
						len_linux -= sizeof(u64);
						buf_linux += 1;
					}

					/*
					 * reinitialise t0 temp by current
					 * task time
					 */
					t0 = t1;

				} else {
					lastsample_RTK = *buf;
					len_RTK -= sizeof(unsigned long long);
					buf += 1;

					lastsample_Linux = *buf_linux;
					len_linux -= sizeof(unsigned long long);
					buf_linux += 1;

					/* choose more recent sample to
					 * initialise sample history */

					if ((lastsample_RTK >> 32) < \
					    (lastsample_Linux >> 32)) {

						t0 = (lastsample_RTK >> 32);
						sample_origine = RTK;
					} else {
						t0 = (lastsample_Linux >> 32);
						sample_origine = LINUX;

					}
				}
			}
			/* hopefully len==0 here */
			xos_fifo_release(vmon_fifo, size);
		}
		lastsample_RTK = NOSAMPLE;
		lastsample_Linux = NOSAMPLE;
bed:
		/* go to sleep */
		vmon_timer.expires = jiffies + timeout;
		add_timer(&vmon_timer);
		down(&vmon_semaphore);

		/* just woke up */
	}

    return 0;
}

static inline unsigned int results_v2(unsigned int dt, unsigned int state)
					{

	ssize_t count = 0;

	if (vmon_device_initialized) {
						mutex_lock(&buffer_mutex);
						
		/*
		 * FIXME Forget sample suffering
		 * from non monotonic timestamp
		 */

		if (likely(dt < 15000000)) {
			count = vmon_write_to_buffer(lastsample_RTK);
							epilog ( "written %d bytes to buffer", count );
		} else {

			/*
			 * We have been handling this bug
			 * (LMSqb95780) for a long time...
			 */

			count = vmon_write_to_buffer(NOSAMPLE);
							vmon_timestamp_errors++;
						}
						
						mutex_unlock(&buffer_mutex);
		return count;

	} else {

		/*
		 * FIXME Forget sample suffering
		 * from non monotonic timestamp
		 */
						
		if (likely(dt < 15000000)) {
							vmon_write_to_stats(state, dt);

		} else {

			/*
			 * We have been handling this
			 * bug (LMSqb95780) for a
			 * long time...
			 */

							vmon_timestamp_errors++;
#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
			before_overrated_state = vmon_lastlaststate;
							vmon_flow_error_state = vmon_laststate;
			vmon_dropped_state = lastsample_RTK & VMON_STATES_MASK;
#endif
						}

#ifdef VMON_DEBUG_TIMESTAMP_ERRORS
						vmon_lastlaststate = vmon_laststate;
		vmon_laststate = lastsample_RTK & VMON_STATES_MASK;
#endif
					}

return 1;
				}	

static int vmon_thread(void *cookie)
{
	unsigned int timeout, mintimeout;
	unsigned size;

	prolog("");

	sema_init(&vmon_semaphore, 0);

	init_timer(&vmon_timer);
	vmon_timer.data     = 0;
	vmon_timer.function = vmon_wakeup;

	/* HZ=100 on 6710 Icebird, then timeout=128 */
	/* for ( timeout = 1; HZ < timeout; timeout *= 2 ); */

	/* timeout every second */
	timeout = HZ;

	mintimeout = timeout / 16;

	if (!mintimeout)
		mintimeout = 8;

	vmon_fifoparam = *vmon_fifosize;
	prolog("FIFO size = %d Bytes", vmon_fifoparam);
	prolog("HZ = %d timer interrupts per second", HZ);

	report("timeout = %d ticks", timeout);

	while (!kthread_should_stop()) {
		unsigned len;
		unsigned long long *buf;

		/* From VirtualLogix perf monitor */
		/* Note that FIFO filling status "never" reaches 75%
		 * thus timeout is set to HZ above */

		if (xos_fifo_count(vmon_fifo) >= (*vmon_fifosize) * 3 / 4) {

			timeout /= 2;

			if (timeout < mintimeout)
				timeout = mintimeout;

		} else if (timeout < HZ)

			timeout *= 2;

		while (likely(buf = xos_fifo_decommit(vmon_fifo, &len))) {
			size = len;

			while (len) {
				unsigned int t0 , t1, dt, state;

				if (likely((lastsample_RTK != NOSAMPLE))) {

					t0 = (lastsample_RTK >> 32);
					t1 = ((*buf) >> 32);

					dt = t1 - t0;
					/* Convert timestamp into us */
					dt = dt - dt/13;
					state = lastsample_RTK \
						& VMON_STATES_MASK;

					if (!results_v2(dt, state)) {
						/* len is not null here,
						 * then deduct it */
						xos_fifo_release(vmon_fifo,
								size - len);
						goto bed;
					}

				}
				lastsample_RTK = *buf;
				len -= sizeof(unsigned long long);
				buf += 1;
			}
			/* hopefully len==0 here */
			xos_fifo_release ( vmon_fifo, size );
		}
bed:
		/* go to sleep */
		vmon_timer.expires = jiffies + timeout;
		add_timer ( &vmon_timer );
		down ( &vmon_semaphore );

		/* just woke up */
	}

    return 0;
}

/** 
 * This function is called when the /proc file is read
 * 
 */
int vmon_global_results_procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	
	prolog ( "(/proc/%s) called", VMON_RESULTS_PROCFS_NAME );
	epilog ( "buffer %p, *buffer_location %p, offset %3lu, count %d, *eof %d",
			buffer, *buffer_location, offset, count, *eof);
	
	if (offset)
		return ret;

	ret += vmon_global_results_procfs ( buffer+ret );
	
	/* FIXME does not prevent second call to read */
	if (count > ret)
		count = ret;
	*eof = 1;

	epilog ( "ret %d", ret );
	
	return ret;
}


/** 
 * This function is called when the /proc file is read
 */
int vmon_detailed_results_procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int count, int *eof, void *data)
{
	int ret = 0;
	
	prolog ( "(/proc/%s) called", VMON_DETAILS_PROCFS_NAME );
	epilog ( "buffer %p, *buffer_location %p, offset %3lu, count %d, *eof %d",
			buffer, *buffer_location, offset, count, *eof);
	
	if (offset)
		return ret;

	ret += vmon_detailed_results_procfs ( buffer+ret );
	
	/* FIXME does not prevent second call to read */
	if (count > ret)
		count = ret;
	*eof = 1;

	epilog ( "ret %d", ret );
	
	return ret;
}

static inline int setup_xosmon_v3(void)
{
	int ret;

	ret = register_trace_sched_switch(\
			probe_sched_switch_vmon);
	if (ret) {
		critical("sched trace: "
			"Couldn't activate tracepoint"
			" probe to kernel_sched_schedule\n");
		return 0;
	}
	sample_origine = LINUX;
	return 1;
}


/**
 * This function is called when the /proc file is written
 *
 */
int vmon_on_off_procfile_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
{
	prolog ( "" );

	vmon_OnOff = (vmon_OnOff+1)%2;

	if (vmon_OnOff) {
		/* reset stats before starting monitoring session */
		vmon_reset_stats ( );
		vmon_flow_error = 0;

		if (vmon_version == 3) {
			if (!setup_xosmon_v3())
				goto fail;
	}


	} else {
		/*
		 * at the end of monitoring,
		 * save current linux2modem idle switch counter */
		vlx_l2m_idle_switch = vmon_l2m_idle_switch;
		if ( vmon_device_initialized )
			warning ( "%d timestamp errors", vmon_timestamp_errors );
		unregister_trace_sched_switch(probe_sched_switch_vmon);

	}

	/* send vmon event to start/stop the monitoring */
	xos_ctrl_raise ( vmon_ctrl, 0 );
	
	return count;
fail:
	vmon_OnOff = 0;
	critical("vmon stop\n");
	return count;
}


static int vmon_create_procfs_files ( void )
{
    int error = 1;

	prolog ( "" );

	vmon_on_off_proc_file = create_proc_entry(VMON_ON_OFF_PROCFS_NAME,
			0444, NULL);
	
	if (vmon_on_off_proc_file == NULL) {
		remove_proc_entry(VMON_ON_OFF_PROCFS_NAME, NULL);
		critical("Error: Could not initialize /proc/%s",
				VMON_ON_OFF_PROCFS_NAME);
		error = -ENOMEM;
		goto bail_out;
	}

	vmon_on_off_proc_file->write_proc= vmon_on_off_procfile_write;
	vmon_on_off_proc_file->owner 	 = THIS_MODULE;
	vmon_on_off_proc_file->mode 	 = S_IFREG | S_IWUGO;
	vmon_on_off_proc_file->uid		 = 0;
	vmon_on_off_proc_file->gid		 = 0;
	vmon_on_off_proc_file->size		 = 37;

	epilog ( "/proc/%s created", VMON_ON_OFF_PROCFS_NAME);	

	vmon_results_proc_file = create_proc_entry(VMON_RESULTS_PROCFS_NAME,
			0444, NULL);
	
	if (vmon_results_proc_file == NULL) {
		remove_proc_entry(VMON_RESULTS_PROCFS_NAME, NULL);
		critical( "Error: Could not initialize /proc/%s",
		       VMON_RESULTS_PROCFS_NAME);
		error = -ENOMEM;
		goto bail_out;
	}

	vmon_results_proc_file->read_proc = vmon_global_results_procfile_read;
	vmon_results_proc_file->owner 	 = THIS_MODULE;
	vmon_results_proc_file->mode 	 = S_IFREG | S_IRUGO;
	vmon_results_proc_file->uid		 = 0;
	vmon_results_proc_file->gid		 = 0;
	vmon_results_proc_file->size	 = 37;

	epilog ( "/proc/%s created", VMON_RESULTS_PROCFS_NAME);

	vmon_details_proc_file = create_proc_entry(VMON_DETAILS_PROCFS_NAME,
			0444, NULL);
	
	if (vmon_details_proc_file == NULL) {
		remove_proc_entry(VMON_DETAILS_PROCFS_NAME, NULL);
		critical( "Error: Could not initialize /proc/%s",
		       VMON_DETAILS_PROCFS_NAME);
		error = -ENOMEM;
		goto bail_out;
	}

	vmon_details_proc_file->read_proc = vmon_detailed_results_procfile_read;
	vmon_details_proc_file->owner 	 = THIS_MODULE;
	vmon_details_proc_file->mode 	 = S_IFREG | S_IRUGO;
	vmon_details_proc_file->uid		 = 0;
	vmon_details_proc_file->gid		 = 0;
	vmon_details_proc_file->size	 = 37;

	epilog ( "/proc/%s created", VMON_DETAILS_PROCFS_NAME);

bail_out:
	return error;
}


/* Register device that userspace will read to store results in log file */
static int vmon_initialize_device(void)
{
	int result;
	dev_t dev;

	prolog ("");

	/*
	 * Register the character device (at least try),
	 * 0 in parameter requests dynamic major allocation
	 */

	if (vmon_major) {
		dev = MKDEV(vmon_major, VMON_MINOR);
		result = register_chrdev_region(dev, 1, VMON_DEVICE_NAME);
	} else {
		result = alloc_chrdev_region(&dev, vmon_minor,
				1, VMON_DEVICE_NAME);

		/* get major number dynamically */
		vmon_major = MAJOR(dev);
	}
	if (result < 0) {
		critical ("can't get/register major %d", vmon_major);
		return result;
	}

	vmon_p_init(dev);

	return 0;
}


static int __init vmon_initialize ( void )
{
    int error = 0;

	NkPhAddr physaddr;
    
    prolog ( "" );

	report("XOSMON-3.0 module started");
	
	if (vmon_outputfile) {
		error = vmon_initialize_device();
		if (error) {
			error = -ENODEV;
			goto bail_out;
		}
		vmon_device_initialized = true;
	}

    /* connect to inter-OS ctrl local end-point */
    vmon_ctrl = xos_ctrl_connect ( VMON_NAME, VMON_EVENT_COUNT );
	if (vmon_ctrl == 0) {
        critical ( "failed to connect to inter-OS ctrl local end-point." );
        error = -ENODEV;
        goto bail_out;
    }

	/* Get virtual address of FIFO size shared variable */
	physaddr = nkops.nk_dev_lookup_by_type( \
			(unsigned long)NK_DEV_ID_VMON_FIFO, 0);

	if (physaddr == 0) {
        error = -ENOMEM;
		critical ("unable to retrieve shared vmon FIFO size variable");
        goto bail_out;
    }

    physaddr += sizeof ( NkDevDesc );
    vmon_fifosize  = nkops.nk_ptov ( physaddr );
	
	if (!vmon_fifosize) {
		critical ( "cannot get shared variable vmon_fifosize." );
		error = -ENOMEM;
		goto bail_out;
	}
	*vmon_fifosize = vmon_fifoparam;
   
	/*
	 * send vmon event to allocate interOS FIFO
	 * using updated vmon_fifosize
	 */
    xos_ctrl_raise ( vmon_ctrl, 1 );

	/* connect to inter-OS fifo local end-point */
    vmon_fifo = xos_fifo_connect ( VMON_NAME, *vmon_fifosize );
	if (vmon_fifo == 0) {
        critical ( "failed to connect to inter-OS fifo local end-point." );
        error = -ENODEV;
        goto bail_out;
    }

	prolog ( "vmon_fifoparam = %d Bytes", vmon_fifoparam );

	if (!vmon_create_procfs_files()) {
		critical ( "cannot create PROCFS files" );
		error = -ENODEV;
		goto bail_out;
	}
	
	/* initialise inter-OS area for kernel PID */
	shared = xos_area_connect("monitorPID", sizeof(unsigned int));
	base_adr = xos_area_ptr(shared);
	*base_adr = 0xFFFFFFFF;

	/* register to overflow vmon event */
    xos_ctrl_register ( vmon_ctrl, 0, vmon_overflow, NULL, 1 );

    /* spawn kernel thread */
	if (vmon_version == 3) {
		vmon_kernel_fifo_init(base_adr);
		vmon_thread_id = kthread_run(vmon_thread_v3, 0, "kvmond");
	} else
	vmon_thread_id = kthread_run ( vmon_thread, 0, "kvmond" );

	if (vmon_thread_id == NULL) {
		critical ( "cannot spawn kernel thread" );
		error = -ENOMEM;
		goto bail_out;
	}
	
bail_out:
    epilog ( "%d", error );
    
    return error;
}


static void __exit vmon_finalize ( void )
{
	prolog ( "" );

	if (vmon_device_initialized) {
		/* unregister char device */
		vmon_p_cleanup();
	}

	vmon_device_initialized = false;
	
    /* unregister from vmon event */
    xos_ctrl_unregister ( vmon_ctrl, 0 );

    /* kill kernel thread */
    kthread_stop ( vmon_thread_id );

	remove_proc_entry(VMON_RESULTS_PROCFS_NAME, NULL);
	epilog ( "/proc/%s removed", VMON_RESULTS_PROCFS_NAME);

	remove_proc_entry(VMON_ON_OFF_PROCFS_NAME, NULL);
	epilog ( "/proc/%s removed", VMON_ON_OFF_PROCFS_NAME);

	remove_proc_entry(VMON_DETAILS_PROCFS_NAME, NULL);
	epilog ( "/proc/%s removed", VMON_DETAILS_PROCFS_NAME);

    epilog ( "" );
}

module_init ( vmon_initialize );
module_exit ( vmon_finalize );

module_param_named(version, vmon_version, uint, S_IRUGO | S_IWUSR);
module_param_named ( debug, vmon_debug, uint, S_IRUGO );
module_param_named ( fifosize, vmon_fifoparam, uint, S_IRUGO | S_IWUSR );
module_param_named ( onOff, vmon_OnOff, uint, S_IRUGO );
module_param_named ( outputfile, vmon_outputfile, uint, S_IRUGO | S_IWUSR );

MODULE_AUTHOR ( "Arnaud TROEL, Emeric VIGIER - Copyright (C) ST-Ericsson 2009" );
MODULE_DESCRIPTION ( "xosmon, inter OS performance monitoring tool"
                     "ST-Ericsson 6710 Linux" );
MODULE_VERSION("XOSMON-3.0");
MODULE_LICENSE ( "GPL" );
