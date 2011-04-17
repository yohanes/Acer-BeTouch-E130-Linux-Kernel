/*
 * linux/drivers/video/pnx/lcdbus_pagefaultfb.c
 *
 * PNX framebuffer
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


/* Icebird includes */
#include <mach/pnxfb.h>
#include <mach/hwmem_api.h>
#include <video/pnx/lcdctrl.h>

#include "lcdbus_pagefaultfb.h"
#include "lcdbus_debug_proc.h"


/* PNG/RAW Splash screen interface */
#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
#include "splash/splash_png.h"
#endif


/*
 * THINGS TO BETTERIFY:
 *
 * FIXME reboot command does not re-init properly the lcd all the time!
 * FIXME a close of the fb closes ALL fb session for all users!
 * FIXME klcdfbd thread seems to be sometimes in D state (before we were
 *       using interruptible_sleep_on() instead of schedule().
 * TODO  implement splash animation.
 * TODO  Propose an implementation for a faster splash screen (and animated)
 *   (use a different __init for lcd/vde/fb)
 * TODO  cross check compilation flags.
 * TODO  lcdfb_blank: it is not very clear in the LCD spec if
 *       it is permit to update the screen in LCD OFF mode. In this
 *       implementation, no refreshes are allowed in LCD OFF mode.
 * FIXME sometimes it needs 2 cat to send an entire image to fb, but
 *       only one cp...
 *
 * */

/*
 *
 */
static const char lcdfb_name[] = "lcdbus_pagefaultfb";

/*
 *  Framebuffer data structure (defined in fbmem.c) 
 */
extern struct fb_info *registered_fb[];

/*
 * ------------------------------------------------
 * FRAMEBUFFER LCD REFRESH ALGO BASED ON PAGEFAULTS
 * ------------------------------------------------
 * In the page fault logic, the global "fps" is used to define the maximum
 * time left for the user to fill the framebuffer before the refresh thread
 * use it. When a user writes in the "mmaped" framebuffer memory, the
 * pagefault logic starts a timer (linked to the parameter "fps").
 * When this timer expired, the refresh thread is waked up, and a full
 * LCD screen update is performed. The refresh thread will do several
 * (frames_before_trying_to_sleep) full LCD refresh at "fps" speed, before
 * re-invalidated the pages with the zap_page_range function for a future
 * wake up with pagefault detections...
 * This algo is not good for the sleep mode because, when a pagefault comes,
 * a timer is started before the LCD refresh, means the phone will be waked
 * up during at least 1 jiffies=10ms (minimum) + the blit time.
 * The only good solution for sleep mode is a synchronous blocking blit,
 * synchronous to be sure to send only updated data, and blocking because
 * it will help to know when the phone can go back in sleep mode...
 *
 * fps values above 50 will all give 1 jiffie @ HZ=100 (means 10ms)
 * While choosing the fps value, please keep in mind the time reqired for
 * the transfer. For instance, if fps=30, means 3 jiffies @ HZ=100,
 * 30ms between each timer... compared to 1/fps=33ms... there are only
 * 3ms for the LCD transfer!
 *
 *   >50fps --> 1 jiffy
 *    50fps --> 2 jiffies
 *    33fps --> 3 jiffies
 *    25fps --> 4 jiffies
 *
 * */
static unsigned int fps = 50;
module_param(fps, uint, 0644);
MODULE_PARM_DESC(fps, "The refresh rate in frames per second.");

/*
 * During screen refresh, user data are sometimes no detected by the
 * pagefault logic, so an extra full screen update in necessary when there
 * is no more detected pagefault.
 * */
static unsigned int refresh_timeout_ms = 100;
module_param(refresh_timeout_ms, uint, 0644);
MODULE_PARM_DESC(refresh_timeout_ms, "The refresh timeout in ms, used to "
		"send an extra full screen frame when there is no more pagefault.");

/*
 * The LCD refresh thread will do "frames_before_trying_to_sleep" refreshes
 * before re-invalidated the pages to re-start the wake-up with pagefault
 * detections...
 * */
static unsigned int frames_before_trying_to_sleep = 20;
module_param(frames_before_trying_to_sleep, uint, 0644);
MODULE_PARM_DESC(frames_before_trying_to_sleep, "The number of frames "
		"to blit before re-activating the pagefault logic.");


/* ----------------------------------------------------------------------------
 * Helper functions
 * ---- */

/** 
 * @brief Wake up the refresh thread
 * 
 * @param drvdata 
 */
static inline void lcdfb_wakeup_refresh(struct lcdfb_drvdata *drvdata) 
{
	drvdata->need_refresh = 1;
	wake_up(&drvdata->wq);
}


/** 
 * @brief Flush all "mmapped" memory spaces (mmap is cached and buffered)
 * 
 * @param driver_data driver data
 * @param ofs Offset address of the memory to flush
 * @param size Size of the memory to flush 
 *
 * TODO Avoid to flush all mmap by detecting only the modified ones,  
 *	or using partial rendering ?
 *		-> Yes, with pagefaults but not so efficient...
 *		-> Check that the function dmac_flush_range works with virtual
 *		   address (because actually we flush all the cache)
 *		-> Update also the IPP to use partial rendering params
 */
void flush_fb_mmap_mem(void *driver_data, u32 ofs, u32 size)
{
	struct lcdfb_drvdata *drvdata = (struct lcdfb_drvdata *)driver_data;
	struct lcdfb_mapping *map;
	unsigned long int start, end;
	u32 i = 0;

	pr_debug("%s() drvdata 0x%p, ofs %u, size %u\n", 
			__FUNCTION__, drvdata, ofs, size);

	list_for_each_entry(map, &drvdata->mappings, link) {
		start = map->vma->vm_start + ofs;
		end = start + size;

		/* Reset to the initial value if the offset is out of the 
		 * VMA range */
		if ((start > map->vma->vm_end) || (end > map->vma->vm_end)) {
			start = map->vma->vm_start;
			end = map->vma->vm_end;
		}

		/* Flush memory (Align start & end address) */
		dmac_flush_range((const void *)(start & PAGE_MASK), 
						 (const void *)PAGE_ALIGN(end));

		i++;
		pr_debug("%d: vm_start %lx vm_end %lx, start %lx end %lx\n", 
				i, 
				map->vma->vm_start, map->vma->vm_end,
				start, end);
	}
}
EXPORT_SYMBOL(flush_fb_mmap_mem);


static int lcdfb_do_refresh(struct lcdfb_drvdata *drvdata) 
{
	int output;
	struct list_head transfers;
	struct lcdfb_transfer transfer;
	struct fb_info *fbi = drvdata->fb_info;
	u32 ofs, size;

	pr_debug("%s()\n", __FUNCTION__);

	/* Full screen data transfer preparation */
	INIT_LIST_HEAD(&transfers);
	list_add_tail(&transfer.link, &transfers);

	/* Draw fps on the screen using 
	 * drvdata->blit_addr before disp->write() */
	lcdbus_debug_fps_on_screen(drvdata);

	/* Save info for proc */
	lcdbus_proc_time_before_transfer(fbi->node);

	/* Send the full screen display to all active ouputs */
	for (output = 0; output < PNXFB_OUTPUT_MAX; output++) {

		/* Send the full screen to the display */
		if (drvdata->outputs[output] != 0) {
			struct lcdfb_drvdata *gdrvdata = registered_fb[output]->par;
			struct device *dev = &gdrvdata->ldev->dev;
			struct lcdfb_ops *disp = gdrvdata->ldev->ops;

			/* Prepare the transfer */
			transfer.x = drvdata->x;
			transfer.y = drvdata->y;
			transfer.w = drvdata->w;
			transfer.h = drvdata->h;
			ofs = fbi->var.yoffset * fbi->fix.line_length;
			transfer.addr_phys = fbi->fix.smem_start + ofs;
			transfer.addr_logical = drvdata->fb_mem + ofs;

			/* Flush "mmapped" memory (one screen size) */
			/* TODO In case of dual outputs, it is not necessary
			 * to flush 2 times the same fb memory! Move
			 * the flush outside the loop */
			size = fbi->var.yres * fbi->fix.line_length;
			flush_fb_mmap_mem((void *)drvdata, ofs, size);
						
			/* Write data to the HW */
			disp->write(dev, &transfers);

			/* Debug (Update stats & Draw fps ) */
			lcdbus_debug_update(gdrvdata, output);
		}
	}

	/* Save info for proc */
	lcdbus_proc_time_after_transfer(fbi->node);

	drvdata->need_refresh = 0;
	return 0;
}

/* lcdfb_count_num_pages
 *
 *  Calculates the number of needed pages
 */
static unsigned int lcdfb_count_num_pages(struct fb_info *info)
{
	return ((info->screen_size + PAGE_SIZE - 1) >> PAGE_SHIFT);
}


/* lcdfb_configure_vm
 *
 *  Allocate & configure drvdata vm memory 
 */
static int lcdfb_configure_vm(struct device *dev, struct lcdfb_drvdata *drvdata)
{
	int i;

	drvdata->pages = krealloc(drvdata->pages,
			sizeof(drvdata->pages[0]) * drvdata->num_pages, GFP_KERNEL);

	if (drvdata->pages == NULL) {
		dev_err(dev, "No more memory (drvdata->pages) !\n");
		return -ENOMEM;
	}

	for (i = 0; i < drvdata->num_pages; i++)
		drvdata->pages[i] = vmalloc_to_page(drvdata->fb_mem + i * PAGE_SIZE);

	return 0;
}


/**
 * lcdfb_zap_mappings - zap's the pages off the mappings
 * @mappings: list of lcdfb_mapping's
 *
 * This function interates over all lcdfb_mappings in mappings. If there has
 * been atleast one pagefault (faults > 0) in a mapping, zap all the pages it
 * contains.
 */
static inline void lcdfb_zap_mappings(struct list_head *mappings)
{
	struct lcdfb_mapping *map;

	list_for_each_entry(map, mappings, link) {
		if (!map->faults)
			continue;

		zap_page_range(map->vma,
				map->vma->vm_start,
				map->vma->vm_end - map->vma->vm_start,
				NULL);

		map->faults = 0;
	}
}

/**
 * lcdfb_run_timer - call timer after frame duration has elapsed
 * @timer: timer to modify
 */
static inline void lcdfb_run_timer(struct timer_list *timer)
{
	if (!timer_pending(timer)) {
		if (!fps)
			fps = 1;

		mod_timer(timer, jiffies + HZ / fps);
	}
}

/**
 * lcdfb_run_refresh_timeout - call timer for one extra screen update
 * There is no test to check if the timer is pending because the goal
 * of this extra screen update is to be used as late as possible, means
 * refresh_timeout_ms ms after the last user screen udpate
 * @timer: timer to modify
 */
static inline void lcdfb_run_refresh_timeout(struct timer_list *timer)
{
	mod_timer(timer, jiffies + (HZ * refresh_timeout_ms) / 1000);
}


/* ----------------------------------------------------------------------------
 * Refresh-the-display functions
 * ---- */

/**
 * lcdfb_timer - interrupt routine for the refresh_timer
 * @data: a pointer to the data of the driver instance
 *
 * Wakes up the wait_queue. This causes all refresh_thread's to continue.
 */
static void lcdfb_timer(unsigned long data)
{
	/* Wake up the refresh thread */
	lcdfb_wakeup_refresh((struct lcdfb_drvdata *)data);
}

/**
 * lcdfb_timeout - interrupt routine for the refresh_timeout
 * @data: a pointer to the data of the driver instance
 *
 * Wakes up the wait_queue. This causes the refresh_thread to continue
 * one more time for a full screen refresh.
 */
static void lcdfb_timeout(unsigned long data)
{
	/* Wake up the refresh thread */
	lcdfb_wakeup_refresh((struct lcdfb_drvdata *)data);
}

/**
 * lcdfb_refresh_thread - pushes frame after frame to the display
 * In the page fault logic, a refresh timer is systematically
 * started after a refresh to be sure to update the whole screen
 * with the more recent data when there is no more nopage fault
 * because user data updates are not detected until zap_mapping,
 * means they are not detected during the disp->write process.
 * @data: pointer to the device it should use
 */
static int lcdfb_refresh_thread(void *data)
{
	unsigned int refresh, device_id = (int )data;
	struct lcdfb_drvdata *drvdata = registered_fb[device_id]->par;
	struct lcdfb_mapping *map;

	/* Be sure thread can die...*/
	allow_signal(SIGTERM);

	pr_debug("%s(): Starting up...\n", __FUNCTION__);

	refresh = 0;

	/* Notify that the thread is started */
	complete(&drvdata->thread_start);

	while (1) {
		if (kthread_should_stop())
			break;

		/* Waiting for transferts */
		wait_event_interruptible(drvdata->wq, drvdata->need_refresh);

		/* Send the full screen display to all active ouputs */
		drvdata->x = 0;
		drvdata->y = 0;
		drvdata->w = drvdata->fb_info->var.xres;
		drvdata->h = drvdata->fb_info->var.yres;
		lcdfb_do_refresh(drvdata);

		/* Prepare next refresh (only if the explicit refresh is not used) */
		if (!drvdata->specific_config->explicit_refresh) {
			u8 modified_pages = 0;

			/* are there modified pages? */
			list_for_each_entry(map, &drvdata->mappings, link)
				if (map->faults) {
					modified_pages = 1;
					break;
				}

			if (modified_pages) {
				refresh++;
				/* all refreshes are finished? if no, start a new timer
				 * for the next one, if yes, invalid all pages for future
				 * pagefault detection and start a timer to keep a screen
				 * up-to-date...*/
				if (refresh < frames_before_trying_to_sleep)
					lcdfb_run_timer(&drvdata->refresh_timer);
				else {
					refresh = 0;
					mutex_lock(&drvdata->mappings_lock);
					lcdfb_zap_mappings(&drvdata->mappings);
					mutex_unlock(&drvdata->mappings_lock);
					/* start a timer for an extra refresh to be sure
					 * the screen is up-to-date after user writes */
					lcdfb_run_refresh_timeout(&drvdata->refresh_timeout);
				}
			}
		}
	} /* while (1) */

	drvdata->thread = NULL;
	complete_and_exit(&drvdata->thread_exit, 0);

	pr_debug("%s(): Exiting...\n", __FUNCTION__);
}

/* ----------------------------------------------------------------------------
 * Framebuffer operations
 * ---- */

/** 
  * @brief Open framebuffer device
  * 
  * @param info fb info containing driver data
  * @param user 
  * 
  * @return 
  */
static int lcdfb_open(struct fb_info *info, int user)
{
	struct lcdfb_drvdata *drvdata = info->par;

	int ret = -ENODEV;

	/* Re-init the panning in case a new app want to see its writes on
	 * the screen. 
	 * Note: - fb_set_par() will not call fb_pan in this case
	 *       - if there are already a fb user (user > 1), we do not change 
	 *         the panning (to not disturb all fb users).
	 *
	 * TODO it may be good to reinit more parameters than only yoffset 
	 * */
	if (user > 1)
		return 0;

	drvdata->blit_addr = drvdata->fb_mem;
	drvdata->blit_addr_phys = drvdata->fb_info->fix.smem_start;
	drvdata->fb_info->var.yoffset = 0;
	if (drvdata->fb_info->fbops->fb_set_par)
		ret = drvdata->fb_info->fbops->fb_set_par(drvdata->fb_info);

	/* Only warn if fb_set_par is not working */
	if (ret) 
		printk(KERN_WARNING "%s(): yoffset is maybe invalid\n", __FUNCTION__);

	return 0;
}


/**
 * lcdfb_write - the userspace interface write function
 * @file: file information struct
 * @buf: data to transfer
 * @count: nb bytes to transfer
 * @offset: offset in the fb
 *
 * TODO TO UNDERSTAND: when doing cp image.raw /dev/fb/0, the lcdfb_write
 *      function * is called 19 times!
 *
 * "cp image.raw /dev/fb/0" or "cat image.raw > /dev/fb/0"
 *
 * nb: for reading the framebuffer, "cat /dev/fb/0 > image.raw"
 */
static ssize_t
lcdfb_write(struct fb_info *info, __user const char *buf, size_t count,
		loff_t * offset)
{
	unsigned long off;
	struct lcdfb_drvdata *drvdata;
	struct lcdfb_device *ldev;
	struct lcdfb_ops *disp;
	struct list_head transfers;

	/*
	 * Stop the kernel splash screen animation when the first user mmap
	 * the fb memory. */
#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
	//splash_stop(drvdata);
#endif

	/* Skip if senseless :) */
	if (!count)
		return 0;

	/* --------------------------------------------------------------------
	 * Sanity checks
	 * ---- */
	if (!info->screen_base || !info->par) {
		return -EINVAL;
	}

	off = *offset;

	drvdata = info->par;
	ldev = drvdata->ldev;
	disp = ldev->ops;

	if (!disp || !disp->write) {
		return -EINVAL;
	}

	if (off > info->screen_size)
		return -ENOSPC;

	if ((count + off) > info->screen_size)
		return -ENOSPC;

	/* --------------------------------------------------------------------
	 * Copy from userspace to the fb memory
	 * ---- */
	count -= copy_from_user(info->screen_base + off, buf, count);
	*offset += count;

	/* --------------------------------------------------------------------
	 * Update the display
	 * ---- */
	INIT_LIST_HEAD(&transfers);

	/* Update the screen in case it has been modified
	 * during the OFF mode.*/
	lcdfb_wakeup_refresh(drvdata);

	/* --------------------------------------------------------------------
	 * Return nicely
	 * ---- */
	if (count)
		return count;

	/* we should always be able to write atleast one byte */
	return -EFAULT;
}


/** 
 * @brief 
 * 
 * @param regno 
 * @param red 
 * @param green 
 * @param blue 
 * @param transp 
 * @param info 
 */
static int
lcdfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
		unsigned transp, struct fb_info *info)
{
	if ((regno >= info->cmap.len) || (regno >= 16))
		return 1; /* Incorrect palette entry */

	((u32 *) (info->pseudo_palette))[regno] = 0; /* We do not use cmap */

	return 0;
}


/**
 * lcdfb_pan_display - Pan the display horizontally (line by line
 * step is possible)
 * @var: fb parameters including new panning offset
 * @info: fb info containing driver data
 * */
static int lcdfb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct fb_fix_screeninfo *fix = &info->fix;
	u32 offset;
	u16 x, y, w, h;

	/* Check if the yoffset is in the framebuffer y range */
	if (var->yoffset + var->yres > var->yres_virtual)
		return -EINVAL;

	/* Update the new start address of the framebuffer
	 * TODO Add a mutex
	 * */
	offset = var->yoffset * fix->line_length;
	drvdata->blit_addr = drvdata->fb_mem + offset;
	drvdata->blit_addr_phys = drvdata->fb_info->fix.smem_start + offset;

	/* Save usefull var info.
	 * Note: There is no need to save reserved[1] & reserved[2] because
	 * these information are already in drvdata x, y, w & h. Only
	 * reserved[0] is saved to indicate the use of partial rendering */
	drvdata->fb_info->var.yoffset = var->yoffset;
	drvdata->fb_info->var.reserved[0] = var->reserved[0];

	/* Check if a partial update is required (only in explicit refresh mode) */
	if ((drvdata->specific_config->explicit_refresh) && 
			(var->reserved[0] == PNXFB_PARTIAL_UPDATE)) {
		/* Extract partial rendering infos from fb reserved fields */
		x = var->reserved[1] & 0xFFFF;
		y = var->reserved[1] >> 16;
		w = var->reserved[2] & 0xFFFF;
		h = var->reserved[2] >> 16;

		/* Check partial refresh values */
		if ((x > var->xres - 1) || (y > var->yres - 1) ||
			(w > var->xres) || (h > var->yres)) {
			printk(KERN_WARNING "Incorrect fb partial refresh values "
					"(%d, %d) %dx%d. Full screen refresh instead.",
					x, y, w, h);
			x = 0;
			y = 0;
			w = var->xres;
			h = var->yres;
		}

		/* Save refresh parameters in the driver */
		// TODO Add a mutex
		drvdata->x = x;
		drvdata->y = y;
		drvdata->w = w;
		drvdata->h = h;

		pr_debug("LCD partial update (%d, %d) - (%d, %d) %dx%d\n",
				x, y, x + w - 1, y + h - 1, w, h);
	}

	/* Synchronous refresh */
	if (var->activate & FB_ACTIVATE_VBL) {
		lcdfb_do_refresh(drvdata);
	} else {
		/* ASynchronous refresh */
		lcdfb_wakeup_refresh(drvdata);
	}

	return 0;
}

/**
 * lcdfb_check_var - Check display parameters (resolution, rotation...) by
 * calling the related display function check_var
 * @var: fb parameters to check
 * @info: fb info containing driver data
 * */
static int lcdfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct lcdfb_ops *disp = drvdata->ldev->ops;
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

	/* call the driver for checking new parameters */
	ret = disp->check_var(&drvdata->ldev->dev, var);

	if (ret == 0) {
		unsigned int new_size, curr_size;
		struct fb_var_screeninfo *fb_var = &drvdata->fb_info->var;

		/* Calculates the current & new framebuffer size */
		curr_size = fb_var->xres * fb_var->yres * (fb_var->bits_per_pixel/8);
		new_size = var->xres * var->yres * (var->bits_per_pixel/8);

		if (new_size > curr_size) {
			void *fb_mem = hwmem_realloc(drvdata->fb_mem, new_size);

			/* Fail to allocate memory */
			if (fb_mem != drvdata->fb_mem) {
				ret = -ENOMEM;
			}
		}
	}

	return ret;
}


/**
 * lcdfb_set_par - Set display parameters (resolution, rotation...) by
 * calling the related display function set_par
 * @var: fb parameters to set
 * @info: fb info containing driver data
 * */
static int lcdfb_set_par(struct fb_info *info)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct lcdfb_ops *disp = drvdata->ldev->ops;
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

	/* call the driver for setting new parameters */
	ret = disp->set_par(&drvdata->ldev->dev, info);

	if (ret == 0) {
		
		struct fb_fix_screeninfo fix;

		/* Get new screen info */
		disp->get_vscreeninfo(&drvdata->ldev->dev, &drvdata->fb_info->var);
		disp->get_fscreeninfo(&drvdata->ldev->dev, &fix);
		
		/* Set new colors (due to fbmem mechanism) */
		info->var.red   = drvdata->fb_info->var.red;
		info->var.green = drvdata->fb_info->var.green;
		info->var.blue  = drvdata->fb_info->var.blue;

		/* Set new virtual resolution */
		info->var.xres_virtual = drvdata->fb_info->var.xres_virtual;
		info->var.yres_virtual = drvdata->fb_info->var.yres_virtual;

		/* Set the new y panning */
		info->fix.ypanstep = fix.ypanstep;

		/* Set the new line_length */
		info->fix.line_length = fix.line_length;

		/* Set the new memory size */
		info->screen_size = info->fix.line_length * info->var.yres_virtual;
		info->fix.smem_len = info->screen_size;

		/* Configure the virtual memory */
		drvdata->num_pages = lcdfb_count_num_pages(info);
		lcdfb_configure_vm(&drvdata->ldev->dev, drvdata);

		/* FIXME : Need to modify other params ? */
	}

	return ret;
}


/**
 * lcdfb_blank - Set display power mode
 * @blank: power mode to set
 * @info: fb info containing driver data
 * */
static int lcdfb_blank(int blank, struct fb_info *info)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct lcdfb_ops *disp = drvdata->ldev->ops;
	int error = 0;

	switch (blank) {

		/* DISPLAY ON */
		case FB_BLANK_UNBLANK:
			error = disp->display_on(&drvdata->ldev->dev);

			/* Update the screen in case it has
			 * been modified during the OFF mode.*/
			if (!error)
				lcdfb_wakeup_refresh(drvdata);
			break;

		/* DISPLAY ? */
		case FB_BLANK_NORMAL:
			/* TODO implement sleep feature */
			printk(KERN_WARNING "Display FB_BLANK_NORMAL mode not yet "
				   "implemented.\n");
			break;

		/* DISPLAY STANDBY (OR SLEEP) */
		case FB_BLANK_VSYNC_SUSPEND:
			error = disp->display_standby(&drvdata->ldev->dev);
			break;

		/* DISPLAY SUSPEND (OR DEEP STANDBY) */
		case FB_BLANK_HSYNC_SUSPEND:
			error = disp->display_deep_standby(&drvdata->ldev->dev);
			break;

		/* DISPLAY OFF */
		case FB_BLANK_POWERDOWN:
			error = disp->display_off(&drvdata->ldev->dev);
			break;

		default:
			printk(KERN_DEBUG "%s(): BAD value\n", __FUNCTION__);
			error = -EINVAL;
	}

	/* Save power state if new power state has been set without error */
	if (error >= 0) {
		drvdata->old_blank = drvdata->blank;
		drvdata->blank = blank;
	}

	return error;
}


/**
 * lcdfb_is_output_available - Check if the given output is available
 *
 * @fb: the frame buffer id
 * @output: the output id
 *
 * */
int lcdfb_is_output_available(int fb, int output)
{
	int i, available = 0;
	struct lcdfb_drvdata *drvdata;

	for (i = 0; i < LCDBUS_FB_MAX; i++) {
		if (i == fb)
			continue;
		else {
			drvdata = registered_fb[i]->par;

			if (drvdata->outputs[output] == 0)
				continue;
			else
				break;
		}
	}

	if (i >= LCDBUS_FB_MAX)
		available = 1;

	return available;
}

/**
 * lcdfb_ioctl - Perform fb specific ioctl
 *
 * @info: fb info containing driver data
 * @cmd : ioctl command
 * @arg : ioclt command argument
 *
 * */
static int lcdfb_ioctl(struct fb_info *info,
		unsigned int cmd, unsigned long arg)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct lcdfb_ops *disp = drvdata->ldev->ops;
	int fb_id = info->node;
	int i, value, ret;

	pr_debug("%s()\n", __FUNCTION__);

	ret = value = 0;

	switch(cmd)	{
	case PNXFB_SET_OUTPUT_CONNECTION:
		if (get_user(value, (int __user *)arg)) {
			ret = -EFAULT;
		}
		else {
			int out, new_outputs[PNXFB_OUTPUT_MAX];

			// Clear current outputs
			memset(&new_outputs, 0, sizeof(int) * PNXFB_OUTPUT_MAX);

			// Set new requested outputs
			for (i = 0; i < PNXFB_OUTPUT_MAX; i++) {
				out = (value & (1 << i));
				if (out) {
					if (lcdfb_is_output_available(fb_id, out-1)) {
						new_outputs[out-1] = 1;
					}
					else {
						ret = -EINVAL;
						printk(KERN_WARNING "%s(): "
								"Output already used !\n", __FUNCTION__);
						break;
					}
				}
			}

			if (ret == 0) {
				memcpy(drvdata->outputs,
						&new_outputs,
						sizeof(int) * PNXFB_OUTPUT_MAX);
			}
		}
		break;

	/*  */
	case PNXFB_GET_OUTPUT_CONNECTION:
		for (i = 0; i < PNXFB_OUTPUT_MAX; i++)
			if (drvdata->outputs[i] != 0)
				value |= 1 << i;

		if (put_user(value, (int __user *)arg)) {
			ret = -EFAULT;
		}
		break;

	/*  */
	case PNXFB_GET_ALL_OUTPUTS:
		value = PNXFB_OUTPUT_CONN_1 | PNXFB_OUTPUT_CONN_2;
		if (put_user(value, (int __user *)arg)) {
			ret = -EFAULT;
		}
		break;

	/* Call default handler */
	default:
		if (disp->ioctl) {
			ret = disp->ioctl(&drvdata->ldev->dev, cmd, arg);
		}
		else {
			printk(KERN_WARNING " %s(): Unknown IOCTL command !\n", 
					__FUNCTION__);
			ret = -ENOIOCTLCMD;
		}
	}

	return ret;
}

/* ----------------------------------------------------------------------------
 * MMAP implementation
 * ---- */

/*
 * lcdfb_vm_open -
 * @vma:
 */
static void lcdfb_vm_open(struct vm_area_struct *vma)
{
	struct lcdfb_mapping *map = vma->vm_private_data;

	/* Increment the mapping's reference count */
	atomic_inc(&map->refcnt);
}

/*
 * lcdfb_vm_close -
 * @vma:
 */
static void lcdfb_vm_close(struct vm_area_struct *vma)
{
	struct lcdfb_mapping *map = vma->vm_private_data;
	struct lcdfb_drvdata *drvdata = map->drvdata;

	mutex_lock(&drvdata->mappings_lock);

	/* Decrement the reference count, and if 0, delete the mapping struct */
	if (atomic_dec_and_test(&map->refcnt)) {
		list_del(&map->link);
		kfree(map);
	}

	mutex_unlock(&drvdata->mappings_lock);
}

/*
 * lcdfb_vm_io_fault - return requested page in fault
 * @vma:
 * @vmf:
 */
static int lcdfb_vm_io_fault(struct vm_area_struct *vma,
		struct vm_fault *vmf)
{
	struct lcdfb_mapping *map = vma->vm_private_data;
	struct lcdfb_drvdata *drvdata = map->drvdata;
	struct page *page;
	unsigned offset = vmf->pgoff << PAGE_SHIFT;
	unsigned page_num = offset >> PAGE_SHIFT;

	/* Check if the page is in the range */
	if (page_num >= drvdata->num_pages)
		return VM_FAULT_SIGBUS;

	/* Make the page available and remember that there has been a fault */
	page = drvdata->pages[page_num];
	get_page(page);

	/* No need to manage timer and pagefaults in explicit refresh mode */
	if (!drvdata->specific_config->explicit_refresh) {
		mutex_lock(&drvdata->mappings_lock);
		map->faults++;
		mutex_unlock(&drvdata->mappings_lock);

		lcdfb_run_timer(&drvdata->refresh_timer);
	}

	/* Return nicely */
	page->index = vmf->pgoff;

	vmf->page = page;
	return 0;
}

/* vm function pointers */
struct vm_operations_struct lcdfb_vm_ops = {
	.open = lcdfb_vm_open,
	.close = lcdfb_vm_close,
	.fault = lcdfb_vm_io_fault,
};

/*
 * lcdfb_mmap - mmap the framebuffer
 * @info: fb info structure containing driver data
 * @vma: virtual memory info
 */
static int lcdfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct lcdfb_drvdata *drvdata = info->par;
	struct lcdfb_mapping *map;
	int num_pages;

	/* Stop the kernel splash screen animation when the first user mmap
	 * the fb memory. */
#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
	//splash_stop(drvdata);
#endif

	/* Sanity checking */
	if (!(vma->vm_flags & (VM_WRITE | VM_READ))) {
		printk(KERN_WARNING "%s(): need writable or/and readable "
				"mapping\n", __FUNCTION__);
		return -EINVAL;
	}
	if (!(vma->vm_flags & VM_SHARED)) {
		printk(KERN_WARNING "%s(): need shared mapping\n", __FUNCTION__);
		return -EINVAL;
	}
	if (vma->vm_pgoff != 0) {
		printk(KERN_WARNING "%s(): need offset 0 "
				"(vm_pgoff=%lu)\n", __FUNCTION__, vma->vm_pgoff);
		return -EINVAL;
	}

	num_pages = (vma->vm_end - vma->vm_start + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (num_pages > drvdata->num_pages) {
		printk(KERN_WARNING "%s(): mapping to big (%ld > %lu)\n", __FUNCTION__,
		       vma->vm_end - vma->vm_start, info->screen_size);
		return -EINVAL;
	}

	if (vma->vm_ops) {
		printk(KERN_WARNING "%s(): vm_ops already set\n", __FUNCTION__);
		return -EINVAL;
	}

	/* Allocate a mapping struct ... */
	map = kmalloc(sizeof(*map), GFP_KERNEL);
	if (map == NULL) {
		printk(KERN_WARNING "%s(): out of memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	/* ... and fill it in */
	map->vma = vma;
	map->faults = 0;
	map->drvdata = drvdata;
	atomic_set(&map->refcnt, 1);

	mutex_lock(&drvdata->mappings_lock);
	list_add(&map->link, &drvdata->mappings);
	mutex_unlock(&drvdata->mappings_lock);

	/* Fill the vma struct */
	vma->vm_ops = &lcdfb_vm_ops;
	vma->vm_flags |= VM_RESERVED | VM_DONTEXPAND;
	vma->vm_private_data = map;

	pr_debug("%s() vm_start 0x%lx, vm_end 0x%lx\n",
			__FUNCTION__, vma->vm_start, vma->vm_end);

	return 0;
}


/* framebuffer function pointers */
static struct fb_ops lcdfb_ops = {
	.owner              = THIS_MODULE,
	.fb_open            = lcdfb_open,
	.fb_write           = lcdfb_write,
	.fb_setcolreg       = lcdfb_setcolreg,
	.fb_fillrect        = cfb_fillrect,
	.fb_imageblit       = cfb_imageblit,
	.fb_copyarea        = cfb_copyarea,
	.fb_mmap            = lcdfb_mmap,
	.fb_blank           = lcdfb_blank,
	.fb_set_par         = lcdfb_set_par,
	.fb_check_var       = lcdfb_check_var,
	.fb_pan_display     = lcdfb_pan_display,
	.fb_ioctl           = lcdfb_ioctl,
};


/*****************************************************************************
 *
 *       EARLY SUSPEND
 *
 ****************************************************************************/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void lcdfb_suspend(struct early_suspend *pes)
{
	int i = 0;
	struct lcdfb_drvdata *drvdata;

	/* Switch off FB device if needed */
	for (i=0; i < LCDBUS_FB_MAX; i++) {		
		drvdata = registered_fb[i]->par;

		if (drvdata == NULL) {
			/* Should NEVER happen */
			printk("%s (FATAL ERROR, please update your LCDBUS_FB_MAX define)\n",
				   	__FUNCTION__);
		}
		else if (drvdata->blank != FB_BLANK_POWERDOWN) {
			struct fb_info *info = registered_fb[i];

			if (!lock_fb_info(info)) {
				printk("%s (FATAL ERROR, unable to lock FBINFO (%d))",
					   	__FUNCTION__, i);
				continue;
			}

			lcdfb_blank(FB_BLANK_POWERDOWN, info);

			unlock_fb_info(info);
		}
	}
}

static void lcdfb_resume(struct early_suspend *pes)
{
	int i = 0;
	struct lcdfb_drvdata *drvdata;

	/* Restore old FB device state */
	for (i=0; i < LCDBUS_FB_MAX; i++) {		
		drvdata = registered_fb[i]->par;

		if (drvdata == NULL) {
			/* Should NEVER happen */
			printk("%s (FATAL ERROR, please update your LCDBUS_FB_MAX define)\n",
				   	__FUNCTION__);
		}
		else if (drvdata->old_blank != FB_BLANK_POWERDOWN) {
			struct fb_info *info = registered_fb[i];

			if (!lock_fb_info(info)) {
				printk("%s (FATAL ERROR, unable to lock FBINFO (%d))",
					   	__FUNCTION__, i);
				continue;
			}

			lcdfb_blank(drvdata->old_blank, info);

			unlock_fb_info(info);
		}
	}
}

static struct early_suspend lcdfb_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = lcdfb_suspend,
	.resume = lcdfb_resume,
};
#endif


/*
 * lcdfb_probe - device driver probe
 * */
static int __devinit lcdfb_probe(struct device *dev)
{
	struct lcdfb_drvdata *drvdata;
	struct lcdfb_device *ldev = to_lcdfb_device(dev);
	struct lcdfb_ops *disp = ldev->ops;
	struct fb_info *info;
	int ret = -ENODEV;

#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
	struct lcdfb_splash_info splash_info;
#endif

	/* --------------------------------------------------------------------
	 * Do we like that device?
	 * ---- */
	if (!disp || !disp->write || !disp->get_fscreeninfo ||
	       !disp->get_vscreeninfo || !disp->get_specific_config) {
		return -EINVAL;
	}

	/* --------------------------------------------------------------------
	 * Allocate driver data and save it in a global variable
	 * ---- */
	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		dev_err(dev, "No more memory (drvdata) !\n");
		return -ENOMEM;
	}

	drvdata->ldev = ldev;

	INIT_LIST_HEAD(&drvdata->mappings);

	mutex_init(&drvdata->mappings_lock);

	init_waitqueue_head(&drvdata->wq);

	init_timer(&drvdata->refresh_timer);
	drvdata->refresh_timer.function = lcdfb_timer;
	drvdata->refresh_timer.data = (unsigned long)drvdata;

	init_timer(&drvdata->refresh_timeout);
	drvdata->refresh_timeout.function = lcdfb_timeout;
	drvdata->refresh_timeout.data = (unsigned long)drvdata;

	/* --------------------------------------------------------------------
	 * Setup info. Note that framebuffer_alloc also allocates a specified
	 * number of bytes more than need. That is used for the pseudo_palette
	 * here.
	 * ---- */
	ret = -ENOMEM;
	info = framebuffer_alloc(sizeof(u32) * 256, dev);
	if (info == NULL) {
		dev_err(dev, "No more memory (info) !\n");
		goto out_free_drvdata;
	}

	info->pseudo_palette = info->par;
	info->par = drvdata;
	info->flags = FBINFO_FLAG_DEFAULT;

	/* Get screen FIX & VAR info */
	disp->get_vscreeninfo(dev, &info->var);
	disp->get_fscreeninfo(dev, &info->fix);

	info->screen_size = info->fix.line_length * info->var.yres_virtual;
	info->fix.smem_len = info->screen_size;
	info->fbops = &lcdfb_ops;

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret < 0) {
		dev_err(dev, "No more memory (info->cmap) !\n");
		goto out_free_info;
	}

	/* --------------------------------------------------------------------
	 * Attach info to ldata
	 * ---- */
	drvdata->fb_info = info;

	/* --------------------------------------------------------------------
	 * Get number of memory pages used for fb memory, and store
	 * references to them, so we can fasten up the nopage handler
	 * ---- */
	drvdata->num_pages = lcdfb_count_num_pages(info);

	/* --------------------------------------------------------------------
	 * Setup framebuffer memory
	 * We allocate more than screen size due to the page granularity
	 * ---- */
	drvdata->fb_mem = (u8 *)hwmem_alloc(drvdata->num_pages * PAGE_SIZE);
	if (drvdata->fb_mem == NULL) {
		dev_err(dev, "No more memory (drvdata->fb_mem) !\n");
		ret = -ENOMEM;
		goto out_free_info;
	}

	/* Save the physical memory address */
	info->fix.smem_start = hwmem_pa(drvdata->fb_mem);
	info->screen_base = drvdata->fb_mem;
	drvdata->blit_addr = drvdata->fb_mem;
	drvdata->blit_addr_phys = info->fix.smem_start;

	ret = lcdfb_configure_vm(dev, drvdata);
	if (ret < 0) {
		goto out_free_fb;
	}

	/* --------------------------------------------------------------------
	 * Register the framebuffer userspace device file
	 * ---- */
	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_err(dev, "Unable to regsiter framebuffer !\n");
		goto out_kill_thread;
	}

	/* --------------------------------------------------------------------
	 * Create SYSFS entries
	 * ---- */
	 if (disp->get_device_attrs) {
		unsigned int size;
		struct device_attribute *device_attrs;

		disp->get_device_attrs(dev, &device_attrs, &size);
		
		for (; size > 0; size--) {
			ret = device_create_file(info->dev, &device_attrs[size - 1]);
			if (ret) {
				dev_err(dev, "Unable to create SYSFS entry %s (%d)\n",
						device_attrs[size-1].attr.name, ret);
			}
		}
	 }

	/* --------------------------------------------------------------------
	 * Attach drvdata to the device
	 * ---- */
	dev_set_drvdata(dev, (void *)info->node);

	drvdata->outputs[info->node] = 1;

	/* --------------------------------------------------------------------
	 * Setup the thread_exit completion & spawn the kernel thread
	 * ---- */
	init_completion(&drvdata->thread_exit);
	init_completion(&drvdata->thread_start);

	drvdata->thread = kthread_run(lcdfb_refresh_thread, (void *)info->node,
			"kfbrefresh/%d", info->node);
	if (drvdata->thread == 0) {
		dev_err(dev, "Unable to create refresh thread !\n");
		ret = -EIO;
		goto out_free_page_transfers;
	}

	/* Wait to the thread to start */
	wait_for_completion(&drvdata->thread_start);

	/* --------------------------------------------------------------------
	 * Get specific display config (power, explicit refresh)
	 * ---- */
	ret = disp->get_specific_config(dev, &drvdata->specific_config);
	if ((ret < 0) || (drvdata->specific_config == NULL)) {
		dev_err(dev, "Unable to set specific config !\n");
		ret = -ENOENT;
		goto out_kill_thread;
	}

	/* Save initial power state */
	drvdata->blank     = drvdata->specific_config->boot_power_mode;
	drvdata->old_blank = drvdata->specific_config->boot_power_mode;

	/* Set default refresh parameters to full screen */
	drvdata->x = 0;
	drvdata->y = 0;
	drvdata->w = drvdata->fb_info->var.xres;
	drvdata->h = drvdata->fb_info->var.yres;

	/* --------------------------------------------------------------------
	 * Splash screen management if activated
	 * ---- */
#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
	/* Show the splash screen on the framebuffer */
	ret = disp->get_splash_info(dev, &splash_info);
	drvdata->splash_info = splash_info;
	if (drvdata->splash_info.data != NULL) {
		if (splash_start(drvdata) < 0)
			printk(KERN_WARNING "fb%d - %s(): Memory or image data problems "
					"for the splash screen!\n", info->node, info->fix.id);
	}
#endif

	/* --------------------------------------------------------------------
	 * Init debug info
	 * ---- */
	lcdbus_debug_proc_init(drvdata);

	/* --------------------------------------------------------------------
	 * Print fb device related info
	 * ---- */
	printk(KERN_INFO "fb%d ready (see /proc/fb%d for more information)\n", 
			info->node, info->node);

#ifdef CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN
	printk(KERN_INFO "fb%d: kernel splash screen\n", info->node);
#endif

	/* return nicely */
	return 0;

	/* Error management */
out_kill_thread:
	if (drvdata->thread) {
		send_sig(SIGTERM, drvdata->thread, 1);
		wait_for_completion(&drvdata->thread_exit);
	}

out_free_page_transfers:
	kfree(drvdata->pages);

out_free_fb:
	hwmem_free(drvdata->fb_mem);

out_free_info:
	framebuffer_release(drvdata->fb_info);

out_free_drvdata:
	kfree(drvdata);

	/* return the error */
	return ret;

}

/**
 * lcdfb_remove - Kill the refresh thread and clean memory allocated
 * by the driver...
 * */
static int __devexit lcdfb_remove(struct device *dev)
{
	int device_id = (int )dev_get_drvdata(dev);
	struct lcdfb_drvdata *drvdata = registered_fb[device_id]->par;

	/* Kill the refresh thread */
	if (drvdata->thread) {
		send_sig(SIGTERM, drvdata->thread, 1);
		wait_for_completion(&drvdata->thread_exit);
	}

	/* Stop timers */
	del_timer_sync(&drvdata->refresh_timer);
	del_timer_sync(&drvdata->refresh_timeout);

	/* Debug/Proc cleanup */
	lcdbus_debug_proc_deinit(drvdata, device_id);

	/* Free the memory */
	kfree(drvdata->pages);

	hwmem_free(drvdata->fb_mem);

	framebuffer_release(drvdata->fb_info);
	kfree(drvdata);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Module generic stuff
 * ---- */
struct device_driver lcdfb_driver = {
	.name = lcdfb_name,
	.bus = &lcdfb_bustype,
	.probe = lcdfb_probe,
	.remove = lcdfb_remove,
};

static int __init lcdfb_init(void)
{
	 /* Early suspende registration */
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&lcdfb_earlys);
#endif

	return driver_register(&lcdfb_driver);
}

static void __exit lcdfb_exit(void)
{
	 /* Early suspende Unregistration */
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lcdfb_earlys);
#endif

	driver_unregister(&lcdfb_driver);
}

module_init(lcdfb_init);
module_exit(lcdfb_exit);

MODULE_AUTHOR("Dirk HOERNER, Philippe CORNU, Faouaz TENOUTIT (ST Ericsson)");
MODULE_LICENSE("GPL");
