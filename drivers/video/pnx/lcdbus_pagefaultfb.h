/*
 * linux/drivers/video/pnx/lcdbus_pagefaultfb.h
 *
 * PNX framebuffer
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_PNX_LCDBUS_PAGEFAULTFB_H
#define __DRIVERS_VIDEO_PNX_LCDBUS_PAGEFAULTFB_H

#include <mach/pnxfb.h>
#include <video/pnx/lcdctrl.h>

/*
 * Number of Frame buffer devices
 * FIXME : To be modified if the number of framebuffer devices change
 */
#define LCDBUS_FB_MAX 2

/*
 * struct lcdfb_mapping - data for one userspace mapping of the frame buffer
 * @link:	used to add this mapping to a list
 * @vma:	pointer to the vm area struct of that mapping
 * @refcnt:	number of userspace applications using this particular mapping
 * @faults:	number of pagefaults generated in this mapping
 * @drvdata:	pointer to the data of this driver instance
 * */
struct lcdfb_mapping {
	struct list_head link;
	struct vm_area_struct *vma;
	atomic_t refcnt;
	int faults;
	struct lcdfb_drvdata *drvdata;
};

/*
 * struct lcdfb_drvdata - data for one instance of this driver
 * @ldev:		pointer to the lcdfb_device this instance uses
 * @info:		pointer to the kernel fb_info struct
 * @fb:			pointer to the vmalloc'd frame buffer
 * @blit_addr:  pointer to the fb blit addr, usefull for the panning function
 * @blit_addr_phys:  physical pointer to the fb blit addr, used by HW
 *
 * @x: top-left x coordinate of the display area to update.
 * @y: top-left y coordinate of the display area to update.
 * @w: width of the display area to update.
 * @h: height of the display area to update.
 * 
 * @specific_config: display specific info (power, explicit blit...)
 * @splash_info: splash screen information
 *
 * @blank: fb power state
 *
 * @num_pages:	number of physical memory pages this vmalloc uses
 * @pages:		pointers to the kernel page structs
 * @thread_id:	pid of the refresh thread
 * @thread_exit:	used to wait for the threat to exit
 * @thread_start:	used to wait for the threat to start 
 * @wq:			the refresh_timer wakes up this wait queue, all threads
 *				register to it
 * @refresh_timer:	timer which starts the next refresh by waking up wq
 * @refresh_timeout: timer for the extra screen udpate when there is no
 *				more pagefault.
 * @mappings:		list of lcdfb_mappings
 * @mappings_lock:	mutex to protect the mappings list
 * */
struct lcdfb_drvdata {
	struct lcdfb_device *ldev;

	/* framebuffer related */
	struct fb_info *fb_info;
	u8 *fb_mem;

	u8 *blit_addr;
	u32 blit_addr_phys;

	/* Partial or full display refresh */
	u16 x;
	u16 y;
	u16 w;
	u16 h;

	struct lcdfb_specific_config *specific_config;
	struct lcdfb_splash_info splash_info;

	int blank;
	int old_blank;

	unsigned int num_pages;
	struct page **pages;

	int outputs[PNXFB_OUTPUT_MAX];

	/* refresh variables */
	struct task_struct *thread;
	struct completion thread_exit;
	struct completion thread_start;
	struct timer_list refresh_timer;
	struct timer_list refresh_timeout;
	wait_queue_head_t wq;
	int need_refresh;

	/* per mmap mapping */
	struct list_head mappings;
	struct mutex mappings_lock;

	/* fb proc */
	char proc_name[10];
};


#endif // __DRIVERS_VIDEO_PNX_LCDBUS_PAGEFAULTFB_H
