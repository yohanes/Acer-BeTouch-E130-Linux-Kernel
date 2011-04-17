/*
 * linux/drivers/video/pnx/lcdbus_debug_proc.c
 *
 * PNX framebuffer - proc and debug
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/seq_file.h>
#include <linux/fb.h>

#include "lcdbus_debug_proc.h"



/*****************************************************************************
 *****************************************************************************
 *																			 *
 *																			 *
 *				               DEBUG SECTION								 *
 *																			 *
 *																			 *
 *****************************************************************************
 *****************************************************************************/

/*
 * TODO  __DEBUG_FPS__ add 24 & 32bpp support, manage res and bpp dynamycally,
 *       Check if the font is readable on a TV output.
 *       Print fps debug traces only if fb is unblank (need get_blank)
 */

/* ----------------------------------------------------------------------------
 * DEBUG FPS FUNCTIONS
 * Use __DEBUG_FPS__ flag to see in the console the VDE output framerate.
 * Modify DEBUG_FPS_RATE_IN_SEC according to your desired FPS refresh rate.
 *
 * Use __DEBUG_FPS_WITH_TIMER__ if you want to see the fps updates in the 
 * console even if there is no screen refresh.
 *
 * Use __DEBUG_FPS_ON_SCREEN__ if you want to see the VDE/TVO output 
 * framerates printed on the LCD/TVO screens. CPU impact is negligible.
 * Note: If there is no screen update, the fps value on the screen
 * is not updated (we do not want to send extra buffers only for debug!)
 *
 * NOTE: Activate __DEBUG_FPS__ if you want to use __DEBUG_FPS_WITH_TIMER__
 * or/and __DEBUG_FPS_ON_SCREEN__.
 *
 * */
//#define __DEBUG_FPS__
//#define __DEBUG_FPS_WITH_TIMER__
//#define __DEBUG_FPS_ON_SCREEN__

/****** FPS debug parameters ******/
#define DEBUG_FPS_RATE_IN_SEC          4


/* FPS debug flag checks */
#if ((defined __DEBUG_FPS_WITH_TIMER__) && !(defined __DEBUG_FPS__))
	#error Please, activate __DEBUG_FPS__
#endif

#if ((defined __DEBUG_FPS_ON_SCREEN__) && !(defined __DEBUG_FPS__))
	#error Please, activate __DEBUG_FPS__
#endif


/*
 *
 */
#ifdef __DEBUG_FPS__

#define DEBUG_FPS_RATE_IN_NSEC         (DEBUG_FPS_RATE_IN_SEC * 1000000000L)

static u32 fps_frames[LCDBUS_FB_MAX];
static u32 fps_current[LCDBUS_FB_MAX];
static u32 fps_total_frames[LCDBUS_FB_MAX];
static u32 fps_total_time[LCDBUS_FB_MAX];
static unsigned long fps_current_time[LCDBUS_FB_MAX];
static struct timer_list timer_debug_fps[LCDBUS_FB_MAX];

#endif /* __DEBUG_FPS__ */


/*****************************************************************************
 *****************************************************************************
 *																			 *
 *																			 *
 *				               /PROC SECTION								 *
 *																			 *
 *																			 *
 *****************************************************************************
 *****************************************************************************/
/* todo comment me */
#define PROC_FPS_UPDATE_TIME_S           (5)

#define PROC_FPS_UPDATE_TIME_MS          (PROC_FPS_UPDATE_TIME_S * 1000)
#define TIMEVAL_DELTA_MS(b, e)           (((e)->tv_sec - (b)->tv_sec) * 1000 + \
                                          ((e)->tv_usec - (b)->tv_usec) / 1000)

/* fps measurement globals */
static u32 proc_fps_current[LCDBUS_FB_MAX];
static u32 proc_fps_avr[LCDBUS_FB_MAX];
static u32 proc_fps_max[LCDBUS_FB_MAX];
static u32 proc_fps_frames[LCDBUS_FB_MAX];
static struct timeval proc_time_fps[LCDBUS_FB_MAX];

/* time transfer measurement globals */
static u32 proc_transfer_current_ms[LCDBUS_FB_MAX];
static u32 proc_transfer_min_ms[LCDBUS_FB_MAX];
static u32 proc_transfer_avr_ms[LCDBUS_FB_MAX];
static u32 proc_transfer_max_ms[LCDBUS_FB_MAX];
static struct timeval proc_time_before[LCDBUS_FB_MAX];
static struct timeval proc_time_after[LCDBUS_FB_MAX];

/* fb_fix_screeninfo types and visual */
static char *fb_fix_type_str[] = 
	{
		"PACKED PIXELS", 
		"PLANES", 
		"INTERLEAVED PLANES",
	   	"TEXT", 
		"VGA PLANES"
	};

static char *fb_fix_visual_str[] =
	{
		"MONO01", 
		"MONO10", 
		"TRUECOLOR", 
		"PSEUDOCOLOR", 
		"DIRECTCOLOR", 
		"STATIC_PSEUDOCOLOR"
	};

/* fb_var_screeninfo rotate */
static char *fb_var_rotate_str[] = {
	"0 degree - normal orientation", 
	"90 degrees - clockwise orientation", 
	"180 degrees - upside down orientation", 
	"270 degrees - counterclockwise orientation"};

static char *fb_power_str[] = {
	"FB_BLANK_UNBLANK",
	"FB_BLANK_NORMAL",
	"FB_BLANK_VSYNC_SUSPEND",
	"FB_BLANK_HSYNC_SUSPEND",
	"FB_BLANK_POWERDOWN"};

static char *fb_power_display_str[] = {
	"display on", 
	"NOT YET IMPLEMENTED", 
	"display standby or sleep",
	"display suspend or deep standby",
	"display off"};


/*
 */
static int lcdbus_proc_seq_open(struct inode *inode, struct file *file);

static struct file_operations lcdbus_proc_ops = {
	.owner    = THIS_MODULE,
	.open     = lcdbus_proc_seq_open,
	.read     = seq_read,
	.llseek   = seq_lseek,
	.release  = single_release
};


/*****************************************************************************
 *****************************************************************************
 *																			 *
 *																			 *
 *				               DEBUG SECTION								 *
 *																			 *
 *																			 *
 *****************************************************************************
 *****************************************************************************/

/** 
 * @brief Draw the fps value on the screen with a small font
 * 
 * @param drvdata 
 */
void lcdbus_debug_fps_on_screen(struct lcdfb_drvdata *drvdata)
{
#ifdef __DEBUG_FPS_ON_SCREEN__
	u32 bpp  = drvdata->fb_info->var.bits_per_pixel;
	u32 xres = drvdata->fb_info->var.xres;
	u32 n    = drvdata->fb_info->node;
	u8  *fb8;
	u16 *fb16, *tmp16;
	u32 *fb32;
	u32 i, j, k, d;
	u32 bgd_w_px, bgd_h_px;
	static int first = 1; /* Use for font init */

	pr_debug("%s(): fb%d\n", __FUNCTION__, n);

	/* FIXME does not work with two fb with different px formats...*/
	if (first) {
		/* Init the font according to the screen color format */
		for(i = 0; i < FPS_FONT_HEIGHT; i++) 
			for(j = 0; j < 10 * FPS_FONT_WIDTH; j++) 
				if (font[i][j])
					font[i][j] = FPS_BPP_OR(bpp); 
		first = 0;
	}

	/* Background size */
	bgd_w_px = FPS_NB_DIGITS * FPS_FONT_WIDTH + FPS_EXTRA_BORDER_PX * 2 +
			FPS_BETWEEN_DIGITS_PX * (FPS_NB_DIGITS - 1);
	bgd_h_px = FPS_FONT_HEIGHT + FPS_EXTRA_BORDER_PX * 2;

	fb8  = (u8  *)drvdata->blit_addr;
	fb32 = (u32 *)drvdata->blit_addr;

	/* Clean the background. Could be done with a memset
	 * if the color is always 0, but we keep the loop
	 * in case we need a different color */
	fb16 = (u16 *)drvdata->blit_addr;
	fb16 += (FPS_POSX + FPS_POSY * xres);
	for(i = 0; i < bgd_h_px; i++) {
		for(j = 0; j < bgd_w_px; j++) {
			//(*fb16) &= FPS_BACK_16BPP;
			(*fb16) = FPS_BACK_16BPP;
			fb16++;
		}
		fb16 += (xres - bgd_w_px);
	}

	/* Print the fps */
	fb16 = (u16 *)drvdata->blit_addr;
	fb16 += (FPS_POSX + FPS_EXTRA_BORDER_PX + 
			(FPS_POSY + FPS_EXTRA_BORDER_PX) * xres);
	tmp16 = fb16;

	/* TODO Should not be useful but just in case fps > 100fps */
	if (fps_current[n] > 99)
		fps_current[n] = 99;
	d = fps_current[n] / 10;
	for(k = (d? 0 : 1); k < FPS_NB_DIGITS; k++) { /* no 1st digit if 0 */
		tmp16 = fb16 + k * FPS_FONT_WIDTH;
		if (k)
			tmp16 += (k * FPS_BETWEEN_DIGITS_PX);
		for(i = 0; i < FPS_FONT_HEIGHT; i++) {
			for(j = 0; j < FPS_FONT_WIDTH; j++) {
				(*tmp16) |= (u16)font[i][d * FPS_FONT_WIDTH + j];
				tmp16++;
			}
			tmp16 += (xres - FPS_FONT_WIDTH);
		}
		d = fps_current[n] - d * 10;
	}
#endif /* __DEBUG_FPS_ON_SCREEN__ */ 
}

#ifdef __DEBUG_FPS__

/** 
 * @brief Fps debug timer function used only with __DEBUG_FPS_WITH_TIMER__
 * 
 * @param timer 
 */
inline void lcdfb_run_timer_debug_fps(struct timer_list *timer)
{
	pr_debug("%s()\n", __FUNCTION__);
	if (!timer_pending(timer)) {
		mod_timer(timer, jiffies + (HZ * DEBUG_FPS_RATE_IN_SEC));
	}
}

#ifdef __DEBUG_FPS_ON_SCREEN__

/****** FPS debug on screen parameters ******/
#define FPS_POSX                       32  /* Screen position */
#define FPS_POSY                       32
#define FPS_BETWEEN_DIGITS_PX          3   /* Pixel between each digits */
#define FPS_EXTRA_BORDER_PX            3   /* Frame border in pixel */
/* Background color */
#define FPS_BACK_16BPP                 (u16)(0x0)
#define FPS_BACK_24BPP                 (u8) (0x0)
#define FPS_BACK_32BPP                 (u32)(0x0)
/* Number color */
#define FPS_FRONT_OR_16BPP             (u16)(0xFFFF) /* 11111 111111 11111b */
#define FPS_FRONT_OR_24BPP             (u8) (0x0FF)
#define FPS_FRONT_OR_32BPP             (u32)(0x00FFFFFF)
#define FPS_BPP_OR(bpp)                ((bpp == 16)? FPS_FRONT_OR_16BPP:   \
                                        (bpp == 24)? FPS_FRONT_OR_24BPP:   \
                                        (bpp == 32)? FPS_FRONT_OR_32BPP:0)

/* Font description */
#define FPS_NB_DIGITS                  2
#define FPS_FONT_WIDTH                 3
#define FPS_FONT_HEIGHT                5
u32 font[FPS_FONT_HEIGHT][10 * FPS_FONT_WIDTH]={
		{1,1,1, 0,1,0, 1,1,1, 1,1,1, 1,0,0, 1,1,1, 1,1,1, 1,1,1, 1,1,1, 1,1,1},
		{1,0,1, 0,1,0, 0,0,1, 0,0,1, 1,0,0, 1,0,0, 1,0,0, 0,0,1, 1,0,1, 1,0,1},
		{1,0,1, 0,1,0, 1,1,1, 0,1,1, 1,1,1, 1,1,1, 1,1,1, 0,1,0, 1,1,1, 1,1,1},
		{1,0,1, 0,1,0, 1,0,0, 0,0,1, 0,0,1, 0,0,1, 1,0,1, 1,0,0, 1,0,1, 0,0,1},
		{1,1,1, 0,1,0, 1,1,1, 1,1,1, 0,0,1, 1,1,1, 1,1,1, 1,0,0, 1,1,1, 1,1,1}};
/* smooth font (to keep)
	{0,1,0, 0,1,0, 1,1,0, 1,1,0, 1,0,0, 1,1,1, 0,1,1, 1,1,1, 0,1,0, 0,1,0},
	{1,0,1, 0,1,0, 0,0,1, 0,0,1, 1,0,0, 1,0,0, 1,0,0, 0,0,1, 1,0,1, 1,0,1},
	{1,0,1, 0,1,0, 0,1,0, 0,1,0, 1,1,1, 1,1,0, 1,1,0, 0,1,0, 0,1,0, 0,1,1},
	{1,0,1, 0,1,0, 1,0,0, 0,0,1, 0,0,1, 0,0,1, 1,0,1, 1,0,0, 1,0,1, 0,0,1},
	{0,1,0, 0,1,0, 0,1,1, 1,1,0, 0,0,1, 1,1,0, 0,1,0, 1,0,0, 0,1,0, 1,1,0}};*/


#endif /* __DEBUG_FPS_ON_SCREEN__ */


/** 
 * @brief Compute the fps and print it in the console 
 * 
 * @param data drvdata 
 */
void lcdbus_debug_timer_fps(unsigned long data)
{
	struct lcdfb_drvdata *drvdata = (struct lcdfb_drvdata *)data;
	u32 n = drvdata->fb_info->node;
	unsigned long delta;

	pr_debug("%s(): fb%d\n", __FUNCTION__, n);

	delta = sched_clock() - fps_current_time[n];

#ifndef __DEBUG_FPS_WITH_TIMER__
	/* We check the elapsed time in case there is no timer */
	if (delta > (unsigned long)DEBUG_FPS_RATE_IN_NSEC)
#endif 
	{
		fps_total_frames[n] += fps_frames[n];
		fps_total_time[n] += (delta / 1000000000L);
		fps_current[n] = fps_frames[n] / DEBUG_FPS_RATE_IN_SEC;

		/* NOTE: It is not really fbX but more outputX... */
		printk(KERN_INFO "fb%d: %dFPS (mean %dFPS, %d frames in %ds)\n",
				n, fps_current[n], fps_total_frames[n] / fps_total_time[n],
				fps_total_frames[n], fps_total_time[n]);

		fps_frames[n] = 0;
		fps_current_time[n] = sched_clock();

#ifdef __DEBUG_FPS_WITH_TIMER__
		/* Re arm the timer */
		lcdfb_run_timer_debug_fps(&timer_debug_fps[n]);
#endif 

	}
}

#endif /* __DEBUG_FPS__ */

/*
 *
 *
 *
 */
void lcdbus_debug_proc_init(struct lcdfb_drvdata *drvdata)
{
	struct proc_dir_entry *pde;
	int fb_id = drvdata->fb_info->node;

	snprintf(drvdata->proc_name, 9, "fb%d", fb_id);

	pde = proc_create_data(drvdata->proc_name, S_IRUGO, NULL, 
			&lcdbus_proc_ops, drvdata);

	if (!pde)
		printk(KERN_WARNING "Unable to create /proc/%s\n", drvdata->proc_name);

	lcdbus_proc_reset_values(fb_id);

#ifdef __DEBUG_FPS__
	fps_frames[fb_id] = 0;
	fps_current[fb_id] = 0;
	fps_total_frames[fb_id] = 0;
	fps_total_time[fb_id] = 0;
	fps_current_time[fb_id] = sched_clock();
	printk(KERN_INFO "fb%d: fps debug trace (%ds)", 
			fb_id, DEBUG_FPS_RATE_IN_SEC);

	#ifdef __DEBUG_FPS_WITH_TIMER__
		init_timer(&timer_debug_fps[fb_id]);
		timer_debug_fps[fb_id].function = lcdbus_debug_timer_fps;
		timer_debug_fps[fb_id].data = (unsigned long)drvdata;
		/* Run the timer */
		lcdfb_run_timer_debug_fps(&timer_debug_fps[fb_id]);
		printk(", with timer");
	#endif /* __DEBUG_FPS_WITH_TIMER__ */

#ifdef __DEBUG_FPS_ON_SCREEN__
		printk(", on screen");
#endif /* __DEBUG_FPS_ON_SCREEN__ */

	printk("\n");
#endif /* __DEBUG_FPS__ */
}


/*
 *
 *
 */
void lcdbus_debug_proc_deinit(struct lcdfb_drvdata *drvdata, int device_id)
{
	/* Remove /proc/fb entry */
	remove_proc_entry(drvdata->proc_name, NULL);

#ifdef __DEBUG_FPS_WITH_TIMER__
	del_timer_sync(&timer_debug_fps[device_id]);
#endif
}

/*
 *
 *
 */
void lcdbus_debug_update(struct lcdfb_drvdata *drvdata, int output)
{
#ifdef __DEBUG_FPS__
	fps_frames[output]++;

	#ifndef __DEBUG_FPS_WITH_TIMER__
			lcdfb_debug_timer_fps((unsigned long)drvdata);
	#endif
#endif /* __DEBUG_FPS__ */
}



/*****************************************************************************
 *****************************************************************************
 *																			 *
 *																			 *
 *				               /PROC SECTION								 *
 *																			 *
 *																			 *
 *****************************************************************************
 *****************************************************************************/

/** 
 * @brief Call this function before display->write command. 
 * 
 * @param n framebuffer number (fb_info->node)
 */
inline void lcdbus_proc_time_before_transfer(int n)
{
	u32 delta_ms, new_fps;
	
	do_gettimeofday(&proc_time_before[n]);

	/* Update fps (max and sliding average) */
	delta_ms = TIMEVAL_DELTA_MS(&proc_time_fps[n], &proc_time_before[n]);
	proc_fps_frames[n]++;

	if (delta_ms > PROC_FPS_UPDATE_TIME_MS) {
		new_fps = (1000 * proc_fps_frames[n]) / delta_ms;
		proc_fps_current[n] = new_fps;
		proc_fps_avr[n] = (proc_fps_avr[n] + new_fps) / 2;

		if (new_fps > proc_fps_max[n])
			proc_fps_max[n] = new_fps;

		/* Prepare future fps computation */
		proc_fps_frames[n] = 0;
		proc_time_fps[n] = proc_time_before[n];
	}
}


/** 
 * @brief Call this function after display->write command. 
 *  
 * @param n framebuffer number (fb_info->node)
 */
inline void lcdbus_proc_time_after_transfer(int n)
{
	u32 delta_ms;
	
	do_gettimeofday(&proc_time_after[n]);

	/* Update transfer times (min, max and sliding average) */
	delta_ms = TIMEVAL_DELTA_MS(&proc_time_before[n], &proc_time_after[n]);

	proc_transfer_current_ms[n] = delta_ms;

	proc_transfer_avr_ms[n] = (proc_transfer_avr_ms[n] + delta_ms) / 2;

	if (delta_ms > proc_transfer_max_ms[n])
		proc_transfer_max_ms[n] = delta_ms;

	if (delta_ms < proc_transfer_min_ms[n])
		proc_transfer_min_ms[n] = delta_ms;
}

/** 
 * @brief Reset proc variables
 * 
 * @param n framebuffer number (fb_info->node)
 */
void lcdbus_proc_reset_values(int n)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	/* fps */
	proc_fps_current[n] = 0;
	proc_fps_avr[n] = 0;
	proc_fps_max[n] = 0;
	proc_fps_frames[n] = 0;
	proc_time_fps[n] = tv;

	/* time transfer */
	proc_transfer_current_ms[n] = 0;		
	proc_transfer_min_ms[n] = 0xFFFFFFFF;
	proc_transfer_avr_ms[n] = 0;		
	proc_transfer_max_ms[n] = 0;
	proc_time_before[n] = tv;
	proc_time_after[n] = tv;

}

/** 
 * @brief Copy driver information into the /proc/fb file
 * 
 * @param m 
 * @param unused 
 * 
 * @return 
 */
static int lcdbus_proc_print_show(struct seq_file *m, void *unused)
{
	struct lcdfb_drvdata *drvdata = m->private;
	struct fb_info *info = drvdata->fb_info;
	struct fb_bitfield *r, *g, *b, *a;
	int n = drvdata->fb_info->node;
	u32 x, y, w, h;

	r = &(info->var.red);
	g = &(info->var.green);
	b = &(info->var.blue);
	a = &(info->var.transp);

	/* Memory info */
	seq_printf(m, "[memory]\n");
	seq_printf(m, "  %luk, %d pages of %lu bytes, "
			"logical 0x%X, physical 0x%X\n",
			info->screen_size >> 10, drvdata->num_pages, PAGE_SIZE,
			(unsigned int)drvdata->fb_mem, (unsigned int)info->fix.smem_start);

	/* Framebuffer standard info (fix and var) */
	seq_printf(m, "[standard]\n");

	seq_printf(m, "  [fix_screeninfo]\n"
			"    id:               %s\n"
			"    smem_start:       0x%lX\n"
			"    smem_len:         %d bytes\n"
			"    type:             %d (%s) see fb.h for more info\n"
			"    visual:           %d (%s) see fb.h for more info\n"
			"    x & y panstep:    %d x %d pixels\n"
			"    ywrapstep:        %d\n"
			"    line_length:      %d bytes\n"
			"    mmio_start:       0x%lX\n"
			"    mmio_len:         %d bytes\n"
			"    accel:            %d (0=no accel) see fb.h for more info\n",
			info->fix.id,
			info->fix.smem_start, info->fix.smem_len,
			info->fix.type, fb_fix_type_str[info->fix.type],
			info->fix.visual, fb_fix_visual_str[info->fix.visual],
			info->fix.xpanstep, info->fix.ypanstep, info->fix.ywrapstep,
			info->fix.line_length,
			info->fix.mmio_start, info->fix.mmio_len, 
			info->fix.accel);

	seq_printf(m, "  [var_screeninfo]\n"
			"    x & y res:        %3d x %3d pixels\n"
			"    x & y virtal res: %3d x %3d pixels\n"
			"    x & y offset:     %3d x %3d pixels\n"
			"    bits_per_pixel:   %d bpp\n"
			"    grayscale:        %s\n"
			"    bitfields:        (offset, length, msb right?)\n"
			"      red               (%2d, %2d, %d)\n"
			"      green             (%2d, %2d, %d)\n"
			"      blue              (%2d, %2d, %d)\n"
			"      transp            (%2d, %2d, %d)\n"
			"    nonstd:           %s%s\n"
			"    activate:         %d\n"
			"    height & width:   %d x %d mm\n"
			"    rotate:           %d (%s) see fb.h for more info\n", 	
			info->var.xres, info->var.yres,
			info->var.xres_virtual, info->var.yres_virtual,
			info->var.xoffset, info->var.yoffset,
			info->var.bits_per_pixel,
			(info->var.grayscale)? "yes (graylevels)" : "no (colors)",
			r->offset, r->length, r->msb_right,
			g->offset, g->length, g->msb_right,
			b->offset, b->length, b->msb_right,
			a->offset, a->length, a->msb_right, 
			(info->var.nonstd)? "non" : "", "standard pixel format", 
			info->var.activate, 
			info->var.height, info->var.width,
			info->var.rotate, fb_var_rotate_str[info->var.rotate]);

	/* Framebuffer specific info */
	seq_printf(m, "[specific]\n");

	/* Boot specific configuration */
	seq_printf(m, "  [boot configuration]\n");
	seq_printf(m, "    explicit refresh: %sactivated\n", 
			(drvdata->specific_config->explicit_refresh)? "":"not ");
	seq_printf(m, "    power mode:       %s (%s)\n", 
			fb_power_display_str[drvdata->specific_config->boot_power_mode], 
			fb_power_str[drvdata->specific_config->boot_power_mode]);

	/* Refresh info */
	seq_printf(m, "  [refresh]\n");
	seq_printf(m, "    mode:             ");
	if (drvdata->specific_config->explicit_refresh)
		seq_printf(m, "explicit refresh using panning\n");
	else
		seq_printf(m, "pagefault %ufps (%ums timeout, %d frames)\n",
			//fps, refresh_timeout_ms, frames_before_trying_to_sleep);
			50, 100, 20); /* FIXME : Find a better way to access these vars */

	/* Partial rendering */
	if (info->var.reserved[0] == PNXFB_PARTIAL_UPDATE) {
		/* Extract partial rendering infos from fb reserved fields */
		x = drvdata->x;
		y = drvdata->y;
		w = drvdata->w;
		h = drvdata->h;
		seq_printf(m,"    partial render:   (%u, %u) - (%u, %u) %ux%u\n",
				x, y, x + w - 1, y + h - 1, w, h);
	} else {
		seq_printf(m,"    partial render:   not used\n");
	}

	/* Power info */
	seq_printf(m, "  [power]\n"
			"    power state:      %s (%s)\n",
			fb_power_display_str[drvdata->blank], 
			fb_power_str[drvdata->blank]);

	/* Performance info */
	seq_printf(m, "[perfs]\n");
	seq_printf(m, "  fps:                current %ufps, avr %ufps, max %ufps "
			"(update every %ds only if activities)\n",
			proc_fps_current[n], proc_fps_avr[n], proc_fps_max[n], 
			PROC_FPS_UPDATE_TIME_S);
	seq_printf(m, "  transfers:          last %ums, min %ums, avr %ums, "
			"max %ums\n", 
			proc_transfer_current_ms[n], proc_transfer_min_ms[n], 
			proc_transfer_avr_ms[n], proc_transfer_max_ms[n]);

	return 0;
}


static int lcdbus_proc_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, lcdbus_proc_print_show, PDE(inode)->data);
}

