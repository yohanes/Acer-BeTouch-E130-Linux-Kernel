/*
 * linux/drivers/video/pnx/displays/tvout.c
 *
 * TV Out driver 
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/pm_qos_params.h>

#include <asm/uaccess.h>
#include <mach/vmalloc.h>
#include <mach/clock.h>
#include <mach/pwr.h>
#include <video/pnx/lcdbus.h>
#include <video/pnx/lcdctrl.h>

#include "tvout.h"


/*
 * Defines
 *
 */
#define TVO_IS_NTSC_STANDARD(std)			\
	((std == PNXFB_TVO_STANDARD_NTSC)   ||	\
	 (std == PNXFB_TVO_STANDARD_NTSC_J) ||	\
	 (std == PNXFB_TVO_STANDARD_PAL_M))

/*
 * TODO	implement display_standby, deep_standby ...
 * TODO Manage all ACTIVATE mode in tvo_set_par
 * */

/* ----------------------------------------------------------------------------
 * struct tvo_drvdata - drivers private data structure
 * @pwr         : power structure of the TVO
 * @clk         : clock structure of the TVO
 * @clk_pll     : clock structure of the TVO PLL
 * @write_done  : completion to wait for irqs
 * @irq         : the TVO irq identifier
 * @fb          : LCDFB control structure (used by TVO driver)
 * @out_standard: TV output standard (PAL, PAL-M, PAL-NC, NTSC, NTSC-J)
 * @out_size	: TV output frame size
 * @image       : DVDO image
 * @frames_ready: Number of frames ready to display
 * @odd_field   : Check the frame odd field
 * @started     : Check if the TVO is already started
 * @color_bar   : Activate/Deactivate the TVOUT color bar mode
 * @zoom_mode   : The TVO zoom mode (see pnxfb.h for more details)
 * @power_mode  : Current power mode
 *---------------------------------------------------------------------------*/
struct tvo_drvdata {
	struct pwr *pwr;	
	struct clk *clk;
	struct clk *clk_pll;
	struct completion write_done;
	int irq;

	struct lcdfb_device fb;
	u32 fb_size;

	t_TVOOutputStandard out_standard; /* PAL (M, NSC, BG), NTSC (J, M)*/
	t_TVOFrameSize		out_size;
	t_TVOInputImage		input_image;
	t_TVOInputFormat    input_format;

	u8 frames_ready;
	u8 odd_field;

	u8 started;
	u8 color_bar;	
	u8 power_mode;
	u8 zoom_mode;
};


/* Specific TVO and FB configuration
 * explicit_refresh: 1 to activate the explicit refresh (based on panning calls,
 *                  usefull if the mmi uses always double buffering).
 *
 * boot_power_mode: FB_BLANK_UNBLANK = DISPLAY ON
 *                  FB_BLANK_VSYNC_SUSPEND = DISPLAY STANDBY (OR SLEEP)
 *                  FB_BLANK_HSYNC_SUSPEND = DISPLAY SUSPEND (OR DEEP STANDBY)
 *                  FB_BLANK_POWERDOWN = DISPLAY OFF
 *                  FB_BLANK_NORMAL = not used for the moment
 * */
static struct lcdfb_specific_config tvo_specific_config = {
	.explicit_refresh = 1,
	.boot_power_mode = FB_BLANK_POWERDOWN,
};


/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_TVOUT_KERNEL_SPLASH_SCREEN
#include "tvout_splash.h"
static struct lcdfb_splash_info tvo_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = tvout_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(tvout_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info tvo_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif

/* FB_FIX_SCREENINFO (see fb.h) */
static struct fb_fix_screeninfo tvo_fix = {
	.id          = TVO_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (TVO_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = TVO_SCREEN_WIDTH * (TVO_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo tvo_var = {
	.xres           = TVO_SCREEN_WIDTH,
	.yres           = TVO_SCREEN_HEIGHT,
	.xres_virtual   = TVO_SCREEN_WIDTH,
	.yres_virtual   = TVO_SCREEN_HEIGHT * (TVO_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = TVO_FB_BPP,

	.nonstd			= 0, /* Set to 0/1 to disable/enable the YUV support */

#if (TVO_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = { 8, 8, 0},
	.blue           = { 0, 8, 0},

#elif (TVO_FB_BPP == 16)
	.red            = {11, 5, 0},
	.green          = { 5, 6, 0},
	.blue           = { 0, 5, 0},

#else
	#error "Unsupported color depth (see driver doc)"
#endif

	.vmode          = FB_VMODE_NONINTERLACED,
	.height         = 44,
	.width          = 33,
	.rotate         = FB_ROTATE_UR,
};


/* 
 * /proc/tvout 
 *
 */
static struct tvo_drvdata *g_tvo = NULL;


static int tvo_is_clocked(void)
{
	if (g_tvo->clk != NULL) {
		return clk_get_usecount(g_tvo->clk);
	}

	return 0;
}

static inline void tvo_init_color(struct fb_bitfield *color, 
		__u32 offset, __u32 length, __u32 msb_right)
{
	color->offset     = offset;
	color->length     = length;
	color->msb_right  = msb_right;	
}

/*
 =========================================================================
 =						Forward declarations							 =
 =========================================================================
*/
static int 
tvo_switchon(struct tvo_drvdata *tvo);

static int 
tvo_switchoff(struct tvo_drvdata *tvo);

static int
tvo_display_on(struct device *dev);

static int
tvo_display_off(struct device *dev);

static int
tvo_set_vscreeninfo(struct tvo_drvdata *tvo, struct fb_var_screeninfo *vsi);

static int 
tvo_get_reg_field(unsigned int reg, unsigned int field, unsigned int shift);

static int 
tvo_calculate_output_size(struct tvo_drvdata *tvo, u8 set_filedFormatter);

static int 
tvo_set_out_standard(struct device *dev, unsigned int out_standard);

/*
 =========================================================================
 =                                                                       =
 =          SYSFS  section                                               =
 =                                                                       =
 =========================================================================
*/

/* tvo_show_tv_standard
 *
 */ 
static ssize_t tvo_show_tv_standard(struct device *device, 
									struct device_attribute *attr,
									char *buf)
{
	const char *stds[PNXFB_TVO_STANDARD_MAX] = 
		{"NTSC", "PAL", "NTSC-J", "PAL-M", "PAL-NC"};

	return sprintf(buf, "%s\n", stds[g_tvo->out_standard]);
}

/* tvo_show_tv_standard
 *
 */ 
static ssize_t tvo_store_tv_standard(struct device *device,
									 struct device_attribute *attr,
									 const char *buf,
									 size_t count)
{
	int out_standard = g_tvo->out_standard; 

	if (strncasecmp(buf, "PAL", count-1) == 0)
		out_standard = PNXFB_TVO_STANDARD_PAL;
	else if (strncasecmp(buf, "PAL-M", count-1) == 0)
		out_standard = PNXFB_TVO_STANDARD_PAL_M;
	else if (strncasecmp(buf, "PAL-NC", count-1) == 0)
		out_standard = PNXFB_TVO_STANDARD_PAL_NC;
	else if (strncasecmp(buf, "NTSC", count-1) == 0) 
		out_standard = PNXFB_TVO_STANDARD_NTSC;
	else if (strncasecmp(buf, "NTSC-J", count-1) == 0)
		out_standard = PNXFB_TVO_STANDARD_NTSC_J;
	else
		printk("%s (Invalid TVOUT standard !)\n", __FUNCTION__);

	if (out_standard != g_tvo->out_standard) {
		tvo_set_out_standard(device->parent, out_standard);
	}

	return count;
}

/* tvo_show_zoom_mode
 *
 */ 
static ssize_t tvo_show_zoom_mode(struct device *device, 
								  struct device_attribute *attr,
								  char *buf)
{
	struct tvo_drvdata *tvo = g_tvo;
	const char *modes[3]= {"NONE", "STRETCH", "BEST_FIT"};

	return sprintf(buf, "%s\n", modes[tvo->zoom_mode]);
}

/* tvo_store_zoom_mode
 *
 */ 
static ssize_t tvo_store_zoom_mode(struct device *device,
								   struct device_attribute *attr,
								   const char *buf,
								   size_t count)
{	
	struct tvo_drvdata *tvo = g_tvo;
	int zoom_mode = tvo->zoom_mode; 

	if (strncasecmp(buf, "NONE", count-1) == 0)
		zoom_mode = PNXFB_ZOOM_NONE;
	else if (strncasecmp(buf, "STRETCH", count-1) == 0) 
		zoom_mode = PNXFB_ZOOM_STRETCH;
	else if (strncasecmp(buf, "BEST_FIT", count-1) == 0)
		zoom_mode = PNXFB_ZOOM_BEST_FIT;

	if (zoom_mode != tvo->zoom_mode) {
		/* Save the new zoom mode */
		tvo->zoom_mode = zoom_mode;

		/* Calculates the output size & Set the field formatter */
		tvo_calculate_output_size(tvo, 1);
	}

	return count;
}

/* tvo_show_pixel_format
 *
 */ 
static ssize_t tvo_show_pixel_format(struct device *device, 
									 struct device_attribute *attr,
									 char *buf)
{
	struct tvo_drvdata *tvo = g_tvo;

	if (tvo->input_format == TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY) {
		return sprintf(buf, "YUV\n");
	}
	else {
		return sprintf(buf, "RGB\n");
	}
}

/* tvo_store_pixel_format
 *
 */ 
static ssize_t tvo_store_pixel_format(struct device *device,
									  struct device_attribute *attr,
									  const char *buf,
									  size_t count)
{
	struct tvo_drvdata *tvo = g_tvo;
	unsigned int non_standard, new_params = 0;

	if (strncasecmp(buf, "YUV", count-1) == 0) {
		/* Need to change the current format ? */
		if (tvo->input_format != TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY) {
			new_params = 1;
			non_standard = 1;			
		}
	}
	else if (strncasecmp(buf, "RGB", count-1) == 0) {		
		
		/* Need to change the current format ? */
		if (tvo->input_format == TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY) {
			new_params = 1;
			non_standard = 0;
		}
	}

	if (new_params) {
		struct fb_var_screeninfo vsi = tvo_var;
		int ret;

		/* Change pixel format (Standard <-> Non standard) */
		vsi.nonstd = non_standard;

		/* Set new params */
		ret = tvo_set_vscreeninfo(tvo, &vsi);

		if (ret == 0) {
			struct fb_info *info = dev_get_drvdata(device);
			info->var = vsi;
		}
	}

	return count;
}


/* tvo_show_color_bar
 *
 * This functions should be used for debug purpose only 
 */ 
static ssize_t tvo_show_color_bar(struct device *device, 
								struct device_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%d\n", g_tvo->color_bar);
}

/* tvo_store_color_bar
 *
 * This functions should be used for debug purpose only
 */ 
static ssize_t tvo_store_color_bar(struct device *device,
								  struct device_attribute *attr,
								  const char *buf,
								  size_t count)
{
	struct tvo_drvdata *tvo = g_tvo;
	int color_bar = -1;

	if (strncasecmp(buf, "0", count-1) == 0) {
		color_bar = 0;
	}
	else if (strncasecmp(buf, "1", count-1) == 0) {
		color_bar = 1;
	}

	if ((color_bar != -1) && (color_bar != tvo->color_bar)) {

		/* Set/Unset the color bar mode */
		tvo->color_bar = color_bar;
		
		if (tvo->power_mode != FB_BLANK_POWERDOWN) {
			/* Switch off the TVOUT */
			tvo_switchoff(tvo);

			/* Switch on the TVOUT */
			tvo_switchon(tvo);
		}
	}

	return count;
}

/* tvo_show_explicit_refresh
 *
 */ 
static ssize_t tvo_show_explicit_refresh(struct device *device, 
									  struct device_attribute *attr,
									  char *buf)
{
	return sprintf(buf, "%d\n", tvo_specific_config.explicit_refresh);
}


/* tvo_store_explicit_refresh
 *
 */ 
static ssize_t tvo_store_explicit_refresh(struct device *device,
									   struct device_attribute *attr,
									   const char *buf,
									   size_t count)
{
	if (strncasecmp(buf, "0", count-1) == 0) {
		tvo_specific_config.explicit_refresh = 0;
	}
	else if (strncasecmp(buf, "1", count-1) == 0) {
		tvo_specific_config.explicit_refresh = 1;
	}

	return count;
}


static struct device_attribute tvo_device_attrs[] = {
	__ATTR(tv_standard,   S_IRUGO|S_IWUSR, tvo_show_tv_standard,
			tvo_store_tv_standard),
	__ATTR(zoom_mode,     S_IRUGO|S_IWUSR, tvo_show_zoom_mode,
			tvo_store_zoom_mode),
	__ATTR(pixel_format,  S_IRUGO|S_IWUSR, tvo_show_pixel_format,
			tvo_store_pixel_format),
	__ATTR(color_bar,  S_IRUGO|S_IWUSR, tvo_show_color_bar,
			tvo_store_color_bar),	
	__ATTR(explicit_refresh, S_IRUGO|S_IWUSR, tvo_show_explicit_refresh,
			tvo_store_explicit_refresh)
};

/*
 =========================================================================
 =                                                                       =
 =          /proc/tvout section for debug purpose                        =
 =                                                                       =
 =========================================================================
*/

/*
 * tvo_print_show 
 *
 */
static int tvo_print_show(struct seq_file *m, void *v)
{
	struct tvo_drvdata *tvo = (struct tvo_drvdata *)(m->private);
	int clocked = tvo_is_clocked();

	t_TVOFieldFormatter field_form;
	unsigned int value1, value2;
	const char *zoom_mode = "Unknown";
	const char *out_std = "Unknwon";
	const char *power_mode = "Unknwon";
	const char *input_format = "Unknown";

	/**************************************************/
	/*             TVO variables status               */
	/**************************************************/
	switch(tvo->zoom_mode) {
	case PNXFB_ZOOM_STRETCH:
		zoom_mode = "Stretch";
		break;
	case PNXFB_ZOOM_BEST_FIT:
		zoom_mode = "Best fit";
		break;
	case PNXFB_ZOOM_NONE:
		zoom_mode = "None";
		break;
	}

	switch(tvo->out_standard) {
	case PNXFB_TVO_STANDARD_PAL:
		out_std = "PAL";
		break;
	case PNXFB_TVO_STANDARD_PAL_M:
		out_std = "PAL-M";
		break;
	case PNXFB_TVO_STANDARD_PAL_NC:
		out_std = "PAL-NC";
		break;		
	case PNXFB_TVO_STANDARD_NTSC:
		out_std = "NTSC";
		break;
	case PNXFB_TVO_STANDARD_NTSC_J:
		out_std = "NTSC-J";
		break;		
	}

	switch(tvo->power_mode) {
	case FB_BLANK_UNBLANK:
		power_mode = "Power ON";
		break;
	case FB_BLANK_POWERDOWN:
		power_mode = "Power OFF";
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		power_mode = "VSYNC Suspend";
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		power_mode = "HSYNC Suspend";
		break;
	}

	switch (tvo->input_format) {
	case TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY:
		input_format = "YUV422";
		break;
	case TVO_FORMAT_RGB565_HALFWORD_ALIGNED:
		input_format = "RGB565";
		break;
	case TVO_FORMAT_BGR888_PACKED_ON_24BIT:
		input_format = "RGB888";
		break;
	}

	seq_printf(m, "=> TVOUT general information:\n");
	seq_printf(m, "  - Supported out. standards: PAL, PAL-M, PAL-NC, NTSC, NTSC-J\n");

	seq_printf(m, "=> TVOUT driver parametres:\n");
	seq_printf(m, "  - Power mode      : %s.\n", power_mode);
	seq_printf(m, "  - Zoom mode       : %s.\n", zoom_mode);
	seq_printf(m, "  - Input format    : %s\n", input_format);
	seq_printf(m, "  - Input size      : %dx%d.\n", tvo_var.xres, tvo_var.yres);
	seq_printf(m, "  - Output size     : %dx%d.\n", tvo->out_size.width, tvo->out_size.height);
	seq_printf(m, "  - Output standard : %s.\n", out_std);

	
	/**************************************************/
	/*             TVO registers status               */
	/**************************************************/
	seq_printf(m, "=> TVOUT registers values:\n");
	
	if (! clocked) {	
		seq_printf(m, "  - Power mode      :\n");
		seq_printf(m, "  - Input format    :\n");
		seq_printf(m, "  - Input size      :\n");
		seq_printf(m, "  - Output size     :\n");
		seq_printf(m, "  - Output standard :\n");
		
		return 0;
	}

	/* */
	value1 = tvo_get_reg_field(TVO_GENERAL_CTRL_REG, TVO_POWER_DOWN_FIELD, TVO_POWER_DOWN_SHIFT);
	switch(value1) {
	case 0:
		power_mode = "Power OFF";
		break;
	case 1:
		power_mode = "Power ON";
		break;
	default:
		power_mode = "Unknown";
		break;
	}

	seq_printf(m, "  - Power mode      : %s.\n", power_mode);

	/* */
	value1 = ioread32(TVO_INPUT_FORMAT_A_REG);
	switch (value1) {
	case TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY:
		input_format = "YUV422";
		break;
	case TVO_FORMAT_RGB565_HALFWORD_ALIGNED:
		input_format = "RGB565";
		break;
	case TVO_FORMAT_BGR888_PACKED_ON_24BIT:
		input_format = "RGB888";
		break;
	}
	
	seq_printf(m, "  - Input format    : %s\n", input_format);

	/* */
	value1 = tvo_get_reg_field(TVO_INPUT_SIZE_A_REG, TVO_WIDTH_FIELD,  TVO_WIDTH_SHIFT);
	value2 = tvo_get_reg_field(TVO_INPUT_SIZE_A_REG, TVO_HEIGHT_FIELD, TVO_HEIGHT_SHIFT);
	seq_printf(m, "  - Input size      : %dx%d.\n", value1, value2);

	/* */
	value1 = tvo_get_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_WIDTH_FIELD,  TVO_WIDTH_SHIFT);
	value2 = tvo_get_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_HEIGHT_FIELD, TVO_HEIGHT_SHIFT);
	seq_printf(m, "  - Output size     : %dx%d.\n", value1, value2);

	/* */
	value1 = tvo_get_reg_field(TVO_GENERAL_CTRL_REG, TVO_OUTPUT_MODE_FIELD,	 TVO_OUTPUT_MODE_SHIFT);
	switch(value1) {
	case PNXFB_TVO_STANDARD_PAL:
		switch(tvo->out_standard) {
		case PNXFB_TVO_STANDARD_PAL:
			out_std = "PAL";
			break;
		case PNXFB_TVO_STANDARD_PAL_NC:
			out_std = "PAL-NC";
			break;
		default:
			out_std = "Unknown PAL standard";
		}
		break;

	case PNXFB_TVO_STANDARD_NTSC:
		switch(tvo->out_standard) {
		case PNXFB_TVO_STANDARD_NTSC:
			out_std = "NTSC";
			break;
		case PNXFB_TVO_STANDARD_NTSC_J:
			out_std = "NTSC-J";
			break;
		case PNXFB_TVO_STANDARD_PAL_M:
			out_std = "PAL-M";
			break;
		default:
			out_std = "Unknown NTSC standard";
		}
		break;
	}

	seq_printf(m, "  - Output standard : %s.\n", out_std);

	/* */
	field_form.oddBeg.horzPos = tvo_get_reg_field(TVO_FIELD_ODD_BEG_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT);
	field_form.oddBeg.vertPos = tvo_get_reg_field(TVO_FIELD_ODD_BEG_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT);
	field_form.oddEnd.horzPos = tvo_get_reg_field(TVO_FIELD_ODD_END_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT);
	field_form.oddEnd.vertPos = tvo_get_reg_field(TVO_FIELD_ODD_END_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT);

	field_form.evenBeg.horzPos = tvo_get_reg_field(TVO_FIELD_EVEN_BEG_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT);
	field_form.evenBeg.vertPos = tvo_get_reg_field(TVO_FIELD_EVEN_BEG_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT);
	field_form.evenEnd.horzPos = tvo_get_reg_field(TVO_FIELD_EVEN_END_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT);
	field_form.evenEnd.vertPos = tvo_get_reg_field(TVO_FIELD_EVEN_END_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT);

	seq_printf(m, "  - Field formatter : ODD  [%04x%04x] [%04x%04x].\n",
		   	field_form.oddBeg.horzPos, field_form.oddBeg.vertPos,
			field_form.oddEnd.horzPos, field_form.oddEnd.vertPos);

	seq_printf(m, "  - Field formatter : EVEN [%04x%04x] [%04x%04x].\n",
		   	field_form.evenBeg.horzPos, field_form.evenBeg.vertPos,
			field_form.evenEnd.horzPos, field_form.evenEnd.vertPos);

	return 0;
}

static int tvo_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, tvo_print_show, g_tvo);
}

static struct file_operations tvo_procOps = {
	.open     = tvo_seq_open,
	.read     = seq_read,
	.llseek   = seq_lseek,
	.release  = seq_release		
};

/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/
static void 
tvo_set_reg_field(unsigned int reg,
		unsigned int field, 
		unsigned int field_shift, 
		unsigned int value)
{
	unsigned int regValue;

	regValue = ioread32(reg);
	regValue = (regValue & field) | (value << field_shift);
	iowrite32(regValue, reg);
}

static int 
tvo_get_reg_field(unsigned int reg,
				unsigned int field, 
				unsigned int field_shift)
{
	unsigned int regValue;

	regValue = ioread32(reg);
	regValue &= ~ field;
	regValue = regValue >> field_shift;

	return regValue;
}

/*
 * tvo_sw_reset
 *
 */
void tvo_sw_reset(void)
{
    volatile u32 loop = 0;

	/* Soft reset device & wait for reset */
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_SW_RESET_FIELD, 0, TVO_SW_RESET_1);
	for (loop=255; loop > 1; loop--); // FIXME

	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_SW_RESET_FIELD, 0, TVO_SW_RESET_0);
	for (loop=255; loop > 1; loop--); // FIXME
}


/*
 * tvo_clearInterrupt
 *
 */
void tvo_clearInterrupt(void)
{
	unsigned int itMask;

	itMask = TVO_LOAD_ID_DAC_1 | TVO_FIFO_UNDERRUN_1 |
			 TVO_SOF_OUT_1	   | TVO_EOF_IN_1		 | TVO_SOF_IN_1;

	/* Clear all interrupt statuses. */
	iowrite32(itMask, TVO_IRQ_CLEAR_REG);
	iowrite32(0,	  TVO_IRQ_CLEAR_REG);
}


/*
 * tvo_disableInterrupt
 *
 */
void tvo_disableInterrupt(void)
{
	unsigned int itMask, regValue;

	itMask = TVO_LOAD_ID_DAC_1 | TVO_FIFO_UNDERRUN_1 |
			 TVO_SOF_OUT_1	   | TVO_EOF_IN_1		 | TVO_SOF_IN_1;

    /* Disable all interrupts. */
	regValue = ioread32(TVO_IRQ_MASK_REG);
    regValue &= ~ itMask;
	iowrite32(regValue, TVO_IRQ_MASK_REG);
}


/*
 * tvo_enableInterrupt
 *
 */
void tvo_enableInterrupt(void)
{
	unsigned int itMask, regValue;

	itMask = TVO_LOAD_ID_DAC_1 | TVO_FIFO_UNDERRUN_1 |
			 TVO_SOF_OUT_1	   | TVO_EOF_IN_1		 | TVO_SOF_IN_1;

    /* Disable all interrupts. */
	regValue = ioread32(TVO_IRQ_MASK_REG);
    regValue |= itMask;
	iowrite32(regValue, TVO_IRQ_MASK_REG);
}


/*
 *  tvo_setDVDOBanks
 *
 * Configure the DVDO
 */
void tvo_setDVDOBanks(t_TVOInputImage *DVDOConfig, struct tvo_drvdata *tvo)
{
    if ( DVDOConfig->imageBank == TVO_SELECT_BANK_A) {
        iowrite32(DVDOConfig->inputAddressPlane[0],	TVO_INPUT_START_ADDRESS_P1_A_REG);
        iowrite32(DVDOConfig->inputAddressPlane[1],	TVO_INPUT_START_ADDRESS_P2_A_REG);
        iowrite32(DVDOConfig->inputAddressPlane[2],	TVO_INPUT_START_ADDRESS_P3_A_REG);
        iowrite32(DVDOConfig->inputStridePlane[0],	TVO_INPUT_STRIDE_P1_A_REG);
        iowrite32(DVDOConfig->inputStridePlane[1],	TVO_INPUT_STRIDE_P2_A_REG);
        iowrite32(DVDOConfig->inputStridePlane[2],	TVO_INPUT_STRIDE_P3_A_REG);

        tvo_set_reg_field(TVO_INPUT_SIZE_A_REG, TVO_WIDTH_FIELD,  TVO_WIDTH_SHIFT,  tvo_var.xres);
        tvo_set_reg_field(TVO_INPUT_SIZE_A_REG, TVO_HEIGHT_FIELD, TVO_HEIGHT_SHIFT, tvo_var.yres);

        tvo_set_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_WIDTH_FIELD, TVO_WIDTH_SHIFT,  tvo->out_size.width);
        tvo_set_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_HEIGHT_FIELD,TVO_HEIGHT_SHIFT, tvo->out_size.height);

        iowrite32(DVDOConfig->pixelBlanking,	TVO_PIXEL_BLANKING_A_REG);
        iowrite32(DVDOConfig->lineBlanking,		TVO_LINE_BLANKING_A_REG);
        iowrite32(DVDOConfig->verticalBlanking,	TVO_VERTICAL_BLANKING_A_REG);
    }
    else if ( DVDOConfig->imageBank == TVO_SELECT_BANK_B ) {
        iowrite32(DVDOConfig->inputAddressPlane[0],	TVO_INPUT_START_ADDRESS_P1_B_REG);
        iowrite32(DVDOConfig->inputAddressPlane[1],	TVO_INPUT_START_ADDRESS_P2_B_REG);
        iowrite32(DVDOConfig->inputAddressPlane[2],	TVO_INPUT_START_ADDRESS_P3_B_REG);
        iowrite32(DVDOConfig->inputStridePlane[0],	TVO_INPUT_STRIDE_P1_B_REG);
        iowrite32(DVDOConfig->inputStridePlane[1],	TVO_INPUT_STRIDE_P2_B_REG);
        iowrite32(DVDOConfig->inputStridePlane[2],	TVO_INPUT_STRIDE_P3_B_REG);

		tvo_set_reg_field(TVO_INPUT_SIZE_B_REG, TVO_WIDTH_FIELD, TVO_WIDTH_SHIFT,  tvo_var.xres);
        tvo_set_reg_field(TVO_INPUT_SIZE_B_REG, TVO_HEIGHT_FIELD,TVO_HEIGHT_SHIFT, tvo_var.yres);

        tvo_set_reg_field(TVO_OUTPUT_SIZE_B_REG, TVO_WIDTH_FIELD, TVO_WIDTH_SHIFT,  tvo->out_size.width);
        tvo_set_reg_field(TVO_OUTPUT_SIZE_B_REG, TVO_HEIGHT_FIELD,TVO_HEIGHT_SHIFT, tvo->out_size.height);

		iowrite32(DVDOConfig->pixelBlanking,	TVO_PIXEL_BLANKING_B_REG);
        iowrite32(DVDOConfig->lineBlanking,		TVO_LINE_BLANKING_B_REG);
        iowrite32(DVDOConfig->verticalBlanking,	TVO_VERTICAL_BLANKING_B_REG);
    }

	iowrite32(DVDOConfig->stripeSize, TVO_STRIPE_SIZE_REG);

	tvo_set_reg_field(TVO_DTL2DVDO_CONTROL_REG,  TVO_BS_FIELD, TVO_BS_SHIFT, DVDOConfig->imageBank);
}


/*
 *  tvo_setDENCRegisters
 *
 */
void tvo_setDENCRegisters(t_TVOOutputStandard outputStandard, u8 color_bar)
{
	t_TVODENCRegisters DENCRegs;

	memset(&DENCRegs, 0, sizeof(t_TVODENCRegisters));

	switch(outputStandard) {
	case  PNXFB_TVO_STANDARD_NTSC: /* NTSC-M  */
		DENCRegs.WssCtrlLsb = 0x00000008;
		DENCRegs.WssCtrlMsb = 0x00000000;
		DENCRegs.BurstStart = 0x00000019;
		DENCRegs.BurstEnd = 0x0000001D;
		DENCRegs.OutPortCtl = 0x00000000;
		DENCRegs.DacPd = 0x00000000;
		DENCRegs.GainR = 0x00000080;
		DENCRegs.GainG = 0x00000080;
		DENCRegs.GainB = 0x00000080;
		DENCRegs.InPortCtl = 0x00000023;
		DENCRegs.DacsCtl0 = 0x00000002;
		DENCRegs.DacsCtl3 = 0x0000003F;
		DENCRegs.VpsEnable = 0x0000000C;
		DENCRegs.VpsByte5 = 0x00000000;
		DENCRegs.VpsByte11 = 0x00000000;
		DENCRegs.VpsByte12 = 0x00000000;
		DENCRegs.VpsByte13 = 0x00000000;
		DENCRegs.VpsByte14 = 0x00000000;
		DENCRegs.ChromaPhase = 0x000000;
		DENCRegs.GainULsb = 0x0000007A;
		DENCRegs.GainVLsb = 0x000000AC;
		DENCRegs.GainUBlackLev = 0x0000003A;
		DENCRegs.GainVBlackLev = 0x0000002E;
		DENCRegs.CcrVertBlankLev = 0x0000002E;
		DENCRegs.StdCtl = 0x00000011; 
		DENCRegs.BurstAmpl = 0x00000043;
		DENCRegs.Fsc0 = 0x0000001F;
		DENCRegs.Fsc1 = 0x0000007C;
		DENCRegs.Fsc2 = 0x000000F0;
		DENCRegs.Fsc3 = 0x00000021;
		DENCRegs.HTrig = 0x00000003;
		DENCRegs.VTrig = 0x00000000;
		DENCRegs.MultiCtl = 0x00000090;
		DENCRegs.FirstActiveLnLsb = 0x00000011;
		DENCRegs.LastActLnLsb = 0x00000040;
		break;

	case  PNXFB_TVO_STANDARD_NTSC_J: /* NTSC-J (Japan) */
		DENCRegs.WssCtrlLsb = 0x00000008;
		DENCRegs.WssCtrlMsb = 0x00000000;
		DENCRegs.BurstStart = 0x00000019;
		DENCRegs.BurstEnd = 0x0000001D;
		DENCRegs.OutPortCtl = 0x00000000;
		DENCRegs.DacPd = 0x00000000;
		DENCRegs.GainR = 0x00000080;
		DENCRegs.GainG = 0x00000080;
		DENCRegs.GainB = 0x00000080;
		DENCRegs.InPortCtl = 0x00000023;
		DENCRegs.DacsCtl0 = 0x00000001;
		DENCRegs.DacsCtl3 = 0x00000035;
		DENCRegs.VpsEnable = 0x0000000C;
		DENCRegs.VpsByte5 = 0x00000000;
		DENCRegs.VpsByte11 = 0x00000000;
		DENCRegs.VpsByte12 = 0x00000000;
		DENCRegs.VpsByte13 = 0x00000000;
		DENCRegs.VpsByte14 = 0x00000000;
		DENCRegs.ChromaPhase = 0x00000058;
		DENCRegs.GainULsb = 0x00000088;
		DENCRegs.GainVLsb = 0x000000C0;
		DENCRegs.GainUBlackLev = 0x0000003F;
		DENCRegs.GainVBlackLev = 0x00000038;
		DENCRegs.CcrVertBlankLev = 0x00000038;
		DENCRegs.StdCtl = 0x00000005;
		DENCRegs.BurstAmpl = 0x00000049;
		DENCRegs.Fsc0 = 0x0000001F;
		DENCRegs.Fsc1 = 0x0000007C;
		DENCRegs.Fsc2 = 0x000000F0;
		DENCRegs.Fsc3 = 0x00000021;
		DENCRegs.HTrig = 0x00000003; 
		DENCRegs.VTrig = 0x00000000;
		DENCRegs.MultiCtl = 0x00000090;
		DENCRegs.FirstActiveLnLsb = 0x00000017;
		DENCRegs.LastActLnLsb = 0x00000035;
		break;

	case  PNXFB_TVO_STANDARD_PAL: /* PAL-B/G */
		DENCRegs.WssCtrlLsb = 0x00000008;
		DENCRegs.WssCtrlMsb = 0x00000000;
		DENCRegs.BurstStart = 0x00000021;
		DENCRegs.BurstEnd = 0x0000001D;
		DENCRegs.OutPortCtl = 0x00000010;
		DENCRegs.DacPd = 0x00000000;
		DENCRegs.GainR = 0x00000080;
		DENCRegs.GainG = 0x00000080;
		DENCRegs.GainB = 0x00000080;
		DENCRegs.InPortCtl = 0x00000023;
		DENCRegs.DacsCtl0 = 0x00000002;
		DENCRegs.DacsCtl3 = 0x0000003F;
		DENCRegs.VpsEnable = 0x0000000C;
		DENCRegs.VpsByte5 = 0x00000000;
		DENCRegs.VpsByte11 = 0x00000000;
		DENCRegs.VpsByte12 = 0x00000000;
		DENCRegs.VpsByte13 = 0x00000000;
		DENCRegs.VpsByte14 = 0x00000000;
		DENCRegs.ChromaPhase = 0x00000010;
		DENCRegs.GainULsb = 0x00000085;
		DENCRegs.GainVLsb = 0x000000B8;
		DENCRegs.GainUBlackLev = 0x00000033;
		DENCRegs.GainVBlackLev = 0x00000035;
		DENCRegs.CcrVertBlankLev = 0x00000035;
		DENCRegs.StdCtl = 0x00000002;
		DENCRegs.BurstAmpl = 0x00000034;
		DENCRegs.Fsc0 = 0x000000CB;
		DENCRegs.Fsc1 = 0x0000008A;
		DENCRegs.Fsc2 = 0x00000009;
		DENCRegs.Fsc3 = 0x0000002A;
		DENCRegs.HTrig = 0x00000003;
		DENCRegs.VTrig = 0x00000000;
		DENCRegs.MultiCtl = 0x000000A0;
		DENCRegs.FirstActiveLnLsb = 0x00000017;
		DENCRegs.LastActLnLsb = 0x00000035;
		break;

	case  PNXFB_TVO_STANDARD_PAL_M:	/* PAL-M  (Brazil 525 lines) */
		DENCRegs.WssCtrlLsb = 0x00000008;
		DENCRegs.WssCtrlMsb = 0x00000000;
		DENCRegs.BurstStart = 0x00000025;
		DENCRegs.BurstEnd = 0x00000029;
		DENCRegs.OutPortCtl = 0x00000000;
		DENCRegs.DacPd = 0x00000000;
		DENCRegs.GainR = 0x00000080;
		DENCRegs.GainG = 0x00000080;
		DENCRegs.GainB = 0x00000080;
		DENCRegs.InPortCtl = 0x00000023;
		DENCRegs.DacsCtl0 = 0x00000001;
		DENCRegs.DacsCtl3 = 0x00000035;
		DENCRegs.VpsEnable = 0x0000000C;
		DENCRegs.VpsByte5 = 0x00000000;
		DENCRegs.VpsByte11 = 0x00000000;
		DENCRegs.VpsByte12 = 0x00000000;
		DENCRegs.VpsByte13 = 0x00000000;
		DENCRegs.VpsByte14 = 0x00000000;
		DENCRegs.ChromaPhase = 0x00000058;
		DENCRegs.GainULsb = 0x00000090;
		DENCRegs.GainVLsb = 0x000000CF; 
		DENCRegs.GainUBlackLev = 0x0000003A;
		DENCRegs.GainVBlackLev = 0x0000002E;
		DENCRegs.CcrVertBlankLev = 0x0000002E;
		DENCRegs.StdCtl = 0x00000017;
		DENCRegs.BurstAmpl = 0x0000002E;
		DENCRegs.Fsc0 = 0x000000E4;
		DENCRegs.Fsc1 = 0x000000EF;
		DENCRegs.Fsc2 = 0x000000E6;
		DENCRegs.Fsc3 = 0x00000021;
		DENCRegs.HTrig = 0x00000003; 
		DENCRegs.VTrig = 0x00000000;
		DENCRegs.MultiCtl = 0x000000A0;
		DENCRegs.FirstActiveLnLsb = 0x00000017;
		DENCRegs.LastActLnLsb = 0x00000035;
		break;

	case  PNXFB_TVO_STANDARD_PAL_NC: /* PAL-NC (Argentina 625 lines) */
		DENCRegs.WssCtrlLsb = 0x00000008;
		DENCRegs.WssCtrlMsb = 0x00000000;
		DENCRegs.BurstStart = 0x00000021;
		DENCRegs.BurstEnd = 0x00000025;
		DENCRegs.OutPortCtl = 0x00000000;
		DENCRegs.DacPd = 0x00000000;
		DENCRegs.GainR = 0x00000080;
		DENCRegs.GainG = 0x00000080;
		DENCRegs.GainB = 0x00000080;
		DENCRegs.InPortCtl = 0x00000023;
		DENCRegs.DacsCtl0 = 0x00000001;
		DENCRegs.DacsCtl3 = 0x00000035;
		DENCRegs.VpsEnable = 0x0000000C;
		DENCRegs.VpsByte5 = 0x00000000;
		DENCRegs.VpsByte11 = 0x00000000;
		DENCRegs.VpsByte12 = 0x00000000;
		DENCRegs.VpsByte13 = 0x00000000;
		DENCRegs.VpsByte14 = 0x00000000;
		DENCRegs.ChromaPhase = 0x00000058;
		DENCRegs.GainULsb = 0x00000090;
		DENCRegs.GainVLsb = 0x000000CF; 
		DENCRegs.GainUBlackLev = 0x0000003A;
		DENCRegs.GainVBlackLev = 0x0000003F;
		DENCRegs.CcrVertBlankLev = 0x0000003F;
		DENCRegs.StdCtl = 0x00000006;
		DENCRegs.BurstAmpl = 0x00000039;
		DENCRegs.Fsc0 = 0x00000046;
		DENCRegs.Fsc1 = 0x00000094;
		DENCRegs.Fsc2 = 0x000000F6;
		DENCRegs.Fsc3 = 0x00000021;
		DENCRegs.HTrig = 0x00000003; 
		DENCRegs.VTrig = 0x00000000;
		DENCRegs.MultiCtl = 0x000000A0;
		DENCRegs.FirstActiveLnLsb = 0x00000017;
		DENCRegs.LastActLnLsb = 0x00000035;
		break;
	}

	if (color_bar)
		DENCRegs.InPortCtl = 0x000000A3;

	/* Write value to registers */
	iowrite32(DENCRegs.WssCtrlLsb,	TVO_WSS_CTRL_LSB_REG);
	iowrite32(DENCRegs.WssCtrlMsb,	TVO_WSS_CTRL_MSB_REG);
	iowrite32(DENCRegs.BurstStart,	TVO_BURST_START_REG);
	iowrite32(DENCRegs.BurstEnd,	TVO_BURST_END_REG);
	iowrite32(DENCRegs.OutPortCtl,	TVO_OUTPUT_PORT_CONTROL_REG);
	iowrite32(DENCRegs.DacPd,		TVO_DAC_POWERDOWN_REG);
	iowrite32(DENCRegs.GainR,		TVO_GAIN_R_REG);
	iowrite32(DENCRegs.GainG,		TVO_GAIN_G_REG);
	iowrite32(DENCRegs.GainB,		TVO_GAIN_B_REG);
	iowrite32(DENCRegs.InPortCtl,	TVO_INPUT_PORT_CONTROL_REG);
	iowrite32(DENCRegs.DacsCtl0,	TVO_DACS_CONTROL0_REG);
	iowrite32(DENCRegs.DacsCtl3,	TVO_DACS_CONTROL3_REG);
	iowrite32(DENCRegs.VpsEnable,	TVO_VPS_ENABLE_REG);
	iowrite32(DENCRegs.VpsByte5,	TVO_VPS_BYTE_5_REG);
	iowrite32(DENCRegs.VpsByte11,	TVO_VPS_BYTE_11_REG);
	iowrite32(DENCRegs.VpsByte12,	TVO_VPS_BYTE_12_REG);
	iowrite32(DENCRegs.VpsByte13,	TVO_VPS_BYTE_13_REG);
	iowrite32(DENCRegs.VpsByte14,	TVO_VPS_BYTE_14_REG);
	iowrite32(DENCRegs.ChromaPhase, TVO_CHROMA_PHASE_REG);
	iowrite32(DENCRegs.GainULsb,	TVO_GAIN_U_LSB_REG);
	iowrite32(DENCRegs.GainVLsb,	TVO_GAIN_V_LSB_REG);
	iowrite32(DENCRegs.GainUBlackLev,TVO_GAIN_U_BLACKLEV_REG);
	iowrite32(DENCRegs.GainVBlackLev,TVO_GAIN_V_BLANKLEV_REG);
	iowrite32(DENCRegs.CcrVertBlankLev,TVO_CCR_VERTBLANKLEV_REG);
	iowrite32(DENCRegs.StdCtl,		TVO_STANDARD_CONTROL_REG);
	iowrite32(DENCRegs.BurstAmpl,	TVO_BURST_AMPLITUDE_REG);
	iowrite32(DENCRegs.Fsc0,		TVO_FSC0_REG);
	iowrite32(DENCRegs.Fsc1,		TVO_FSC1_REG);
	iowrite32(DENCRegs.Fsc2,		TVO_FSC2_REG);
	iowrite32(DENCRegs.Fsc3,		TVO_FSC3_REG);
	iowrite32(DENCRegs.HTrig,		TVO_HTRIG_REG);
	iowrite32(DENCRegs.VTrig,		TVO_VTRIG_REG);
	iowrite32(DENCRegs.MultiCtl,	TVO_MULTI_CONTROL_REG);
	iowrite32(DENCRegs.FirstActiveLnLsb,TVO_FIRST_ACTIVE_LINE_LSB_REG);
	iowrite32(DENCRegs.LastActLnLsb,	TVO_LAST_ACTIVE_LINE_LSB_REG);
}

/*
 *  tvo_setGenConfig
 *
 */
void tvo_setGenConfig(t_TVOGenConfig *genConfig)
{
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_STREAM_MODE_FIELD,	 TVO_STREAM_MODE_SHIFT,	 genConfig->streamMode);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_OUTPUT_MODE_FIELD,	 TVO_OUTPUT_MODE_SHIFT,	 genConfig->outputStandard);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_COLOR_MAT_FIELD,	 TVO_COLOR_MAT_SHIFT,	 genConfig->colorMatrixEn);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_DEBUG_PROBE_A_FIELD, TVO_DEBUG_PROBE_A_SHIFT,genConfig->debugProbeA);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_DEBUG_PROBE_B_FIELD, TVO_DEBUG_PROBE_B_SHIFT,genConfig->debugProbeB);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_DEBUG_PROBE_C_FIELD, TVO_DEBUG_PROBE_C_FIELD,genConfig->debugProbeC);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_DAC_MAP_FIELD,		TVO_DAC_MAP_SHIFT,		genConfig->DACMap);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_DAC_EDGE_FIELD,		TVO_DAC_EDGE_SHIFT,		genConfig->DACEdge);
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_LOAD_ID_CAL_FIELD,	TVO_LOAD_ID_CAL_SHIFT,	genConfig->loadIdCalEn);
}


/*
 *  tvo_setColorMatrix
 *
 */
void tvo_setColorMatrix(t_TVOColorMatrixCoef *matrixCoef)
{
	tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_2_1_REG, TVO_COEFH_FIELD, TVO_COEFH_SHIFT, matrixCoef->coef21_H);
	tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_2_1_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef21_L);

	tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_4_3_REG, TVO_COEFM_FIELD, TVO_COEFM_SHIFT, matrixCoef->coef43_M);
	tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_4_3_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef43_L);

    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_6_5_REG, TVO_COEFH_FIELD, TVO_COEFH_SHIFT, matrixCoef->coef65_H);
    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_6_5_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef65_L);

    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_8_7_REG, TVO_COEFM_FIELD, TVO_COEFM_SHIFT, matrixCoef->coef87_M);
    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_8_7_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef87_L);

    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_10_9_REG, TVO_COEFH_FIELD, TVO_COEFH_SHIFT, matrixCoef->coef109_H);
    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_10_9_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef109_L);

    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_12_11_REG, TVO_COEFM_FIELD, TVO_COEFM_SHIFT, matrixCoef->coef1211_M);
    tvo_set_reg_field(TVO_COLOR_MATRIX_COEF_12_11_REG, TVO_COEFL_FIELD, TVO_COEFL_SHIFT, matrixCoef->coef1211_L);

	iowrite32(matrixCoef->ucYColorClip, TVO_COLOR_CLIP_REG);
}


/*
 *  tvo_setAntiFlickerFilter
 *
 */
void tvo_setAntiFlickerFilter(t_TVOAntiFlicker *antiFlicker)
{
	/* Set Anti-Flicker mode. */
	iowrite32(antiFlicker->mode, TVO_ANTIFLICKER_MODE_REG);

	/* Program the anti-flicker filters. */
    tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_H_Y_FIELD,		TVO_H_Y_SHIFT,			antiFlicker->filter.HY);
	tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_H_CBCR_FIELD,		TVO_H_CBCR_SHIFT,		antiFlicker->filter.HCbCr);
	tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_V_ODD_Y_FIELD,	TVO_V_ODD_Y_SHIFT,		antiFlicker->filter.VOddY);
	tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_V_ODD_CBCR_FIELD,	TVO_V_ODD_CBCR_SHIFT,	antiFlicker->filter.VOddCbCr);
	tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_V_EVEN_Y_FIELD,	TVO_V_EVEN_Y_SHIFT,		antiFlicker->filter.VEvenY);
	tvo_set_reg_field(TVO_ANTIFLICKER_FILTER_REG, TVO_V_EVEN_CBCR_FIELD,TVO_V_EVEN_CBCR_SHIFT,	antiFlicker->filter.VEvenCbCr);
}


/*
 * tvo_setFieldFormatter
 *
 * Output window position calculated to be centred on screen
 */
void tvo_setFieldFormatter(struct tvo_drvdata *tvo)
{
	t_TVOFieldFormatter fieldFormatter;

	memset(&fieldFormatter, 0, sizeof(t_TVOFieldFormatter));

	/* Field formatter config according to TV standard */
	if (TVO_IS_NTSC_STANDARD(tvo->out_standard)) {
		/* Output window position calculated to be centred on screen */
		/* Upper left coordinates in Odd frame */
		fieldFormatter.oddBeg.horzPos = TVO_NTSC_ACTIVE_COLUMN_START +
			(((TVO_NTSC_ACTIVE_COLUMN_STOP - TVO_NTSC_ACTIVE_COLUMN_START) - tvo->out_size.width)/2);

		fieldFormatter.oddBeg.vertPos = TVO_NTSC_ACTIVE_ODD_LINE_START +
			(((TVO_NTSC_ACTIVE_ODD_LINE_STOP - TVO_NTSC_ACTIVE_ODD_LINE_START) - (tvo->out_size.height/2))/2);

		/* Lower right coordinates in Odd frame */
		fieldFormatter.oddEnd.horzPos = fieldFormatter.oddBeg.horzPos + tvo->out_size.width;
		fieldFormatter.oddEnd.vertPos = fieldFormatter.oddBeg.vertPos + (tvo->out_size.height/2);

		/* Outside the field? */
		if (fieldFormatter.oddEnd.horzPos > TVO_NTSC_LAST_COLUMN)
			fieldFormatter.oddEnd.horzPos = TVO_NTSC_FIRST_COLUMN;

		/* Upper left coordinates in Even frame */
		fieldFormatter.evenBeg.horzPos = fieldFormatter.oddBeg.horzPos;

		fieldFormatter.evenBeg.vertPos  = TVO_NTSC_ACTIVE_EVEN_LINE_START +
			(((TVO_NTSC_ACTIVE_EVEN_LINE_STOP - TVO_NTSC_ACTIVE_EVEN_LINE_START) - (tvo->out_size.height/2))/2);

		/* Lower right coordinates in Even frame */
		fieldFormatter.evenEnd.horzPos = fieldFormatter.oddEnd.horzPos;
		fieldFormatter.evenEnd.vertPos = fieldFormatter.evenBeg.vertPos + (tvo->out_size.height/2);

		/* Outside the field? */
		if (fieldFormatter.evenEnd.vertPos > TVO_NTSC_LAST_LINE)
			fieldFormatter.evenEnd.vertPos = TVO_NTSC_FIRST_LINE;
	}
	else  {
		/* Upper left coordinates in Odd frame */
		fieldFormatter.oddBeg.horzPos = TVO_PAL_ACTIVE_COLUMN_START +
			(((TVO_PAL_ACTIVE_COLUMN_STOP - TVO_PAL_ACTIVE_COLUMN_START) - tvo->out_size.width)/2);

		fieldFormatter.oddBeg.vertPos = TVO_PAL_ACTIVE_ODD_LINE_START +
			(((TVO_PAL_ACTIVE_ODD_LINE_STOP - TVO_PAL_ACTIVE_ODD_LINE_START) - (tvo->out_size.height/2))/2);

		/* Lower right coordinates in Odd frame */
		fieldFormatter.oddEnd.vertPos = fieldFormatter.oddBeg.vertPos + tvo->out_size.height/2;
		fieldFormatter.oddEnd.horzPos = fieldFormatter.oddBeg.horzPos + tvo->out_size.width;

		/* Outside the field? */
		if (fieldFormatter.oddEnd.horzPos > TVO_PAL_LAST_COLUMN)
			fieldFormatter.oddEnd.horzPos = TVO_PAL_FIRST_COLUMN;

		/* Upper left coordinates in Even frame */
		fieldFormatter.evenBeg.horzPos = fieldFormatter.oddBeg.horzPos;

		fieldFormatter.evenBeg.vertPos  = TVO_PAL_ACTIVE_EVEN_LINE_START +
			(((TVO_PAL_ACTIVE_EVEN_LINE_STOP - TVO_PAL_ACTIVE_EVEN_LINE_START) - (tvo->out_size.height/2))/2);

		/* Lower right coordinates in Even frame */
		fieldFormatter.evenEnd.horzPos = fieldFormatter.oddEnd.horzPos;

		fieldFormatter.evenEnd.vertPos = fieldFormatter.evenBeg.vertPos + tvo->out_size.height/2;

		/* Outside the field? */
		if (fieldFormatter.evenEnd.vertPos > TVO_PAL_LAST_LINE)
			fieldFormatter.evenEnd.vertPos = TVO_PAL_FIRST_LINE;
	}

	/*Then Set the Field formatter atthe register level*/
    fieldFormatter.fieldPixColor.YComponent  = 0x10;
    fieldFormatter.fieldPixColor.CbComponent = 0x80;
    fieldFormatter.fieldPixColor.CrComponent = 0x80;

	/* Set the windows of interest. */
	tvo_set_reg_field(TVO_FIELD_ODD_BEG_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT, fieldFormatter.oddBeg.horzPos);
	tvo_set_reg_field(TVO_FIELD_ODD_BEG_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT, fieldFormatter.oddBeg.vertPos);

	tvo_set_reg_field(TVO_FIELD_ODD_END_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT, fieldFormatter.oddEnd.horzPos);
	tvo_set_reg_field(TVO_FIELD_ODD_END_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT, fieldFormatter.oddEnd.vertPos);

	tvo_set_reg_field(TVO_FIELD_EVEN_BEG_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT, fieldFormatter.evenBeg.horzPos);
	tvo_set_reg_field(TVO_FIELD_EVEN_BEG_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT, fieldFormatter.evenBeg.vertPos);

	tvo_set_reg_field(TVO_FIELD_EVEN_END_REG, TVO_HPOS_FIELD, TVO_HPOS_SHIFT, fieldFormatter.evenEnd.horzPos);
	tvo_set_reg_field(TVO_FIELD_EVEN_END_REG, TVO_VPOS_FIELD, TVO_VPOS_SHIFT, fieldFormatter.evenEnd.vertPos);

	tvo_set_reg_field(TVO_LOAD_ID_H_REG, TVO_H_START_FIELD, TVO_H_START_SHIFT, fieldFormatter.loadId.horzStart);
	tvo_set_reg_field(TVO_LOAD_ID_H_REG, TVO_V_STOP_FIELD,  TVO_V_STOP_SHIFT,  fieldFormatter.loadId.horzStop);

	tvo_set_reg_field(TVO_LOAD_ID_V_REG,	TVO_H_START_FIELD, TVO_H_START_SHIFT, fieldFormatter.loadId.vertStart);
	tvo_set_reg_field(TVO_LOAD_ID_V_REG,	TVO_V_STOP_FIELD, TVO_V_STOP_SHIFT,   fieldFormatter.loadId.vertStop);

    /* Configure the colour conversion parameters. */
	tvo_set_reg_field(TVO_FIELD_PIX_COLOR_REG, TVO_Y_FIELD,  TVO_Y_SHIFT,  fieldFormatter.fieldPixColor.YComponent);
	tvo_set_reg_field(TVO_FIELD_PIX_COLOR_REG, TVO_CB_FIELD, TVO_CB_SHIFT, fieldFormatter.fieldPixColor.CbComponent);
	tvo_set_reg_field(TVO_FIELD_PIX_COLOR_REG, TVO_CR_FIELD, TVO_CR_SHIFT, fieldFormatter.fieldPixColor.CrComponent);
}


/*
 *  tvo_setSyncGenerator
 *
 */
void  tvo_setSyncGenerator(t_TVOOutputStandard outputStandard)
{
	t_TVOPosition	syncPosV1;
    t_TVOPosition	syncPosV2;
    unsigned short	HsyncPos;

	HsyncPos          = 33;
	syncPosV1.horzPos = 1;
	syncPosV1.vertPos = 1;

	/*Test the current video standard */
	if (TVO_IS_NTSC_STANDARD(outputStandard)) {
		/* NTSC  configuration */
		syncPosV2.horzPos = 853;
		syncPosV2.vertPos = 263;
	}
	else  {
		/* PAL configuration */
		syncPosV2.horzPos = 864;
		syncPosV2.vertPos = 313;
	}

	/*then set the synchro generator*/
	iowrite32(HsyncPos, TVO_SYNCHRO_H_REG);

	tvo_set_reg_field(TVO_SYNCHRO_V1_REG, TVO_SYNC_HPOS_FIELD, TVO_SYNC_HPOS_SHIFT, syncPosV1.horzPos);
	tvo_set_reg_field(TVO_SYNCHRO_V1_REG, TVO_SYNC_VPOS_FIELD, TVO_SYNC_VPOS_SHIFT, syncPosV1.vertPos);

	tvo_set_reg_field(TVO_SYNCHRO_V2_REG, TVO_SYNC_HPOS_FIELD, TVO_SYNC_HPOS_SHIFT, syncPosV2.horzPos);
	tvo_set_reg_field(TVO_SYNCHRO_V2_REG, TVO_SYNC_VPOS_FIELD, TVO_SYNC_VPOS_SHIFT, syncPosV2.vertPos);
}

/*
 * tvo_FIFORecovery
   ------------------
   An exact sequence of operations must be followed in order to recover from
   a FIFO Underrun or Overrun, otherwise overrun or underrun interrupts will
   be generated upon attempting to start-up the system again.
   First the software has to ensure that the streaming mode (loop) has been
   turned off. Then a stop command has to be issued followed by the resetting
   of the deivce. Each operation requires a 255 clock cycle settling time before
   the next operation is called.
*/
void tvo_FIFORecovery(struct tvo_drvdata *tvo)
{
    volatile u32 counter = 0;

    /* first of all disable interrupts etc. ----------------------------------*/
	tvo_clearInterrupt();

	tvo_disableInterrupt();

	/* First turn streaming mode off. ----------------------------------------*/
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_STREAM_MODE_FIELD, 0, TVO_STREAM_MODE_SINGLE_PICTURE);
    for(counter = 0; counter <= 255; counter++);

    /* Issue a stop command. -------------------------------------------------*/
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_STOP_FIELD, 0, TVO_STOP_1);
    for(counter = 0; counter <= 255; counter++);

	/* Switch off the TVOUT */
	tvo_switchoff(tvo);

	/* Switch on the TVOUT */
	tvo_switchon(tvo);
}


/*
 * tvo_set_out_standard
 *
 */
static int 
tvo_set_out_standard(struct device *dev, unsigned int out_standard)
{
	int ret = 0;
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);	

	if (out_standard >= PNXFB_TVO_STANDARD_MAX) {
		ret = -EINVAL;
	}
	else {
		/* Save the new output standard */
		tvo->out_standard = out_standard;

		/* Calculates the output size */
		tvo_calculate_output_size(tvo, 0);

		if (tvo->power_mode != FB_BLANK_POWERDOWN) {
			/* Switch off the TVO HW */
			tvo_display_off(dev);

			/* Switch on the TVO HW */
			tvo_display_on(dev);
		}
	}

	return ret;
}

/*
 * tvo_calculate_output_size
 *
 */
static int 
tvo_calculate_output_size(struct tvo_drvdata *tvo, u8 set_filedFormatter)
{
	int max_w, max_h; 
	int clocked = tvo_is_clocked();

	/* Calcaulates the TVO output size */
	switch (tvo->zoom_mode) {
	case PNXFB_ZOOM_NONE:
		tvo->out_size.width  = tvo_var.xres;
		tvo->out_size.height  = tvo_var.yres;
		break;

	case PNXFB_ZOOM_BEST_FIT:
		if (TVO_IS_NTSC_STANDARD(tvo->out_standard)) {
			max_w = TVO_NTSC_MAX_WIDTH;
			max_h = TVO_NTSC_MAX_HEIGHT;
		}
		else {
			max_w = TVO_PAL_MAX_WIDTH;
			max_h = TVO_PAL_MAX_HEIGHT;
		}

		if (((tvo_var.xres * 1000 / max_w)) >  /* Width ratio  */
			((tvo_var.yres * 1000 / max_h))) { /* Height ratio */
			tvo->out_size.width  = ((tvo_var.xres * 1000) / ((tvo_var.xres * 1000) / max_w));
			tvo->out_size.height = ((tvo_var.yres * 1000) / ((tvo_var.xres * 1000) / max_w));
		}
		else {
			tvo->out_size.width  = (tvo_var.xres  * 1000)/ ((tvo_var.yres * 1000) / max_h);
			tvo->out_size.height = (tvo_var.yres  * 1000)/ ((tvo_var.yres * 1000) / max_h);
		}
		break;

	case PNXFB_ZOOM_STRETCH:
		if (TVO_IS_NTSC_STANDARD(tvo->out_standard)) {
			tvo->out_size.width  = TVO_NTSC_MAX_WIDTH;
			tvo->out_size.height = TVO_NTSC_MAX_HEIGHT;
		}
		else {
			tvo->out_size.width  = TVO_PAL_MAX_WIDTH;
			tvo->out_size.height = TVO_PAL_MAX_HEIGHT;
		}
		break;
	}

	
	/* Update output size ? */
	if (clocked) {
		tvo_set_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_WIDTH_FIELD, TVO_WIDTH_SHIFT,  tvo->out_size.width);
		tvo_set_reg_field(TVO_OUTPUT_SIZE_A_REG, TVO_HEIGHT_FIELD,TVO_HEIGHT_SHIFT, tvo->out_size.height);
		
		if (set_filedFormatter) {
			/* Set the field formatter */
			tvo_setFieldFormatter(tvo);
		}

		/* Change parameters for the next frame */
		tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_TV_UPDATE_FIELD, 0, TVO_TV_UPDATE_1);
	}

	return 0;
}

/*
 * tvo_set_default_params
 *
 */
static int 
tvo_set_default_params(struct tvo_drvdata *tvo)
{
	tvo->out_standard = TVO_OUTPUT_STANDARD;
	tvo->zoom_mode    = TVO_ZOOM_MODE;
	tvo->power_mode   = tvo_specific_config.boot_power_mode;

	/* If the TVOUT is configured with a NON standard format use */
	/* YUV422 format (good quality) otherwise use RGB format */
	if (tvo_var.nonstd)
		tvo->input_format = TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY;
	else if (tvo_var.bits_per_pixel == 16)
		tvo->input_format = TVO_FORMAT_RGB565_HALFWORD_ALIGNED;
	else
		tvo->input_format = TVO_FORMAT_BGR888_PACKED_ON_24BIT;

	/* Calculates the output size */
	tvo_calculate_output_size(tvo, 0);

	return 0;
}


/**
 * tvo_set_vscreeninfo
 * @tvo:	device which has been used to call this function
 * @vsi:	new var screeninfo structure to be set
 *
 * Set the variable screen information.
 */
static int
tvo_set_vscreeninfo(struct tvo_drvdata *tvo,
					struct fb_var_screeninfo *vsi)

{
	int ret = 0;
	int clocked = tvo_is_clocked();
	t_TVOInputFormat old_format = tvo->input_format;
	unsigned int size, bytes_per_pixel;
	

	/* Check pixelformat (RGB, YUV)*/
	if (tvo_var.nonstd != vsi->nonstd) {
		tvo_var.nonstd = vsi->nonstd;

		/* When changing to YUV, color depth (BPP) should be 16 */
		if (tvo_var.nonstd == 1) {
			vsi->bits_per_pixel = 16;
		}
	}

	/* Check bpp */
	if (tvo_var.bits_per_pixel != vsi->bits_per_pixel) {
		tvo_var.bits_per_pixel = vsi->bits_per_pixel;

		switch(tvo_var.bits_per_pixel) {
		case 16:
			tvo_init_color(&tvo_var.red,  11, 5, 0);
			tvo_init_color(&tvo_var.green, 5, 6, 0);
			tvo_init_color(&tvo_var.blue,  0, 5, 0);
			break;

		case 24:
			tvo_init_color(&tvo_var.red,   16, 8, 0);
			tvo_init_color(&tvo_var.green,  8, 8, 0);
			tvo_init_color(&tvo_var.blue,   0, 8, 0);
			break;
		}
	}

	bytes_per_pixel = tvo_var.bits_per_pixel/8;

	/* Check resolution */
	if ((tvo_var.xres != vsi->xres) ||
			(tvo_var.yres != vsi->yres)) {

		size = tvo_var.xres_virtual * tvo_var.yres_virtual * bytes_per_pixel;

		/* Set the new resoltion & virtual resolution */
		tvo_var.xres = vsi->xres;
		tvo_var.yres = vsi->yres;

		tvo_var.xres_virtual = tvo_var.xres;
		tvo_var.yres_virtual = size / (tvo_var.xres_virtual * bytes_per_pixel);

		/* Check the y panning */
		if ((tvo_var.yres_virtual > tvo_var.yres)) {
			tvo_fix.ypanstep = 1;
		}
		else {
			tvo_fix.ypanstep = 0;
		}

		/* Calculates the output size & Set the field formatter */
		tvo_calculate_output_size(tvo, 1);
	}

	/* Set the new line length (BPP or resolution changed) */
	tvo_fix.line_length = tvo_var.xres_virtual * bytes_per_pixel;

	/* If the TVOUT is configured with a NON standard format use */
	/* YUV422 format (good quality) otherwise use RGB format */
	if (tvo_var.nonstd)
		tvo->input_format = TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY;
	else if (tvo_var.bits_per_pixel == 16)
		tvo->input_format = TVO_FORMAT_RGB565_HALFWORD_ALIGNED;
	else
		tvo->input_format = TVO_FORMAT_BGR888_PACKED_ON_24BIT;

	if ((old_format != tvo->input_format) && (clocked)) {
		iowrite32(tvo->input_format, TVO_INPUT_FORMAT_A_REG);

		/* Change parameters for the next frame */
		tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_TV_UPDATE_FIELD, 0, TVO_TV_UPDATE_1);
	}

	return ret;
}


/**
 * tvo_write_data
 * @dev: device which has been used to call this function
 * @transfer: the lcdfb_transfer to be converted
 *
 * This function writes data of the given transfer.
 */
static int
tvo_write_data(struct tvo_drvdata *tvo, struct lcdfb_transfer *transfer)
{
	pr_debug("%s()\n", __FUNCTION__);

		/* Set image parameters */
	tvo->input_image.inputAddressPlane[0] = transfer->addr_phys;

		/* Set the input buffer */
	tvo_setDVDOBanks(&tvo->input_image, tvo);

	if (!tvo->started) {
		/* START ...........  */
		tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_START_FIELD, 0, TVO_START_1);
		
		/* Set flags */
		tvo->started = 1;
		tvo->odd_field = 0;
	}
	else {
		/* Change parameters for the next frame */
		tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_TV_UPDATE_FIELD, 0, TVO_TV_UPDATE_1);
	}

	/* Increase the number of available frames */
	tvo->frames_ready++;

	/* Wait for the buffer to be written to the TVO hw */
	wait_for_completion(&tvo->write_done);

	return 0;
}

/*
 * Reset & Initialize the TVO block
 *
 */
void tvo_hw_init(struct tvo_drvdata *tvo)
{
	t_TVOGenConfig       genConfig;
	t_TVOAntiFlicker     antiFlicker;
	t_TVOColorMatrixCoef matrixCoef;

    /* First, switch on the device */
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_POWER_DOWN_FIELD, 0, TVO_POWER_DOWN_TVO_POWERED_UP);

	/* Disable interrupt */
	tvo_disableInterrupt();

	/* Clear interrupt */
	tvo_clearInterrupt();

	/* SW Reset TVOUT */
	tvo_sw_reset();

	/* Initialize DENC registers */
	tvo_setDENCRegisters(tvo->out_standard, tvo->color_bar);

	/* General control configuration */
	if (TVO_IS_NTSC_STANDARD(tvo->out_standard))
		genConfig.outputStandard = PNXFB_TVO_STANDARD_NTSC;
	else
		genConfig.outputStandard = PNXFB_TVO_STANDARD_PAL;

	genConfig.streamMode    = TVO_LOOP_MODE;
	genConfig.debugProbeA	= TVO_PROBE_A_ANTI_FLICKER_OUTPUT;
	genConfig.debugProbeB	= TVO_DEBUG_SIGNAL_Y_R;
	genConfig.debugProbeC   = TVO_PROBE_C_DENC_OUTPUT;
	genConfig.DACMap        = TVO_DAC_TO_DENC_CVBS;
	genConfig.DACEdge       = TVO_DAC_RISING_EDGE;
	genConfig.loadIdCalEn   = 0;
	if (TVO_FORMAT_IS_RGB(tvo->input_format))
		genConfig.colorMatrixEn = TVO_COLOR_MATRIX_ENABLED;
	else
		genConfig.colorMatrixEn = TVO_COLOR_MATRIX_BYPASSED;

	tvo_setGenConfig(&genConfig);

	/* Set input format register */
    iowrite32(tvo->input_format, TVO_INPUT_FORMAT_A_REG);

	/* Color matrix configuration */
	matrixCoef.coef21_H   = TVO_COLOR_MATRIX_COEF02;
	matrixCoef.coef21_L   = TVO_COLOR_MATRIX_COEF01;
	matrixCoef.coef43_L   = TVO_COLOR_MATRIX_COEF03;
	matrixCoef.coef43_M   = TVO_COLOR_MATRIX_COEF04;
	matrixCoef.coef65_H   = TVO_COLOR_MATRIX_COEF06;
	matrixCoef.coef65_L   = TVO_COLOR_MATRIX_COEF05;
	matrixCoef.coef87_L   = TVO_COLOR_MATRIX_COEF07;
	matrixCoef.coef87_M   = TVO_COLOR_MATRIX_COEF08;
	matrixCoef.coef109_H  = TVO_COLOR_MATRIX_COEF10;
	matrixCoef.coef109_L  = TVO_COLOR_MATRIX_COEF09;
	matrixCoef.coef1211_L = TVO_COLOR_MATRIX_COEF11;
	matrixCoef.coef1211_M = TVO_COLOR_MATRIX_COEF12;
	matrixCoef.ucYColorClip = 0xFF;

	tvo_setColorMatrix(&matrixCoef);

	/* Anti-flicker configuration */
	antiFlicker.mode = TVO_ANTIFLICKER_MODE_PROGRESSIVE;
	antiFlicker.filter.VEvenCbCr = 0x03;
	antiFlicker.filter.VEvenY    = 0x03;
	antiFlicker.filter.VOddCbCr  = 0x01;
	antiFlicker.filter.VOddY     = 0x01;
	antiFlicker.filter.HCbCr     = 0x03;
	antiFlicker.filter.HY        = 0x03;

	tvo_setAntiFlickerFilter(&antiFlicker);

	/* Configure the Field formatter */
	tvo_setFieldFormatter(tvo);

	/* Configure the synchro generator*/
	tvo_setSyncGenerator(tvo->out_standard);

	/* Calculates the output size & Set the field formatter */
	tvo_calculate_output_size(tvo, 1);

	/* Enable TVO interrupt */
	tvo_enableInterrupt();
}

/**
 * tvo_switchon
 * @tvo: device which has been used to call this function
 *
 * This function is called by the probe function to switches the display on.
 */
static int tvo_switchon(struct tvo_drvdata *tvo)
{
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	/* --> Set DDR constraint (320 ---> 200 Mhz) */
	ret = pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT, 
									TVO_NAME, 320);
	if (ret < 0) {
		printk("%s (pm_qos_update_requirement (DDR) failed ! (%d))\n", 
			__FUNCTION__, ret);
	}
	else {	
		/* enable power */
		pwr_enable(tvo->pwr);

		/* enable PLL clock */
		clk_enable(tvo->clk_pll);

		/* enable clock */
		clk_enable(tvo->clk);

		/* Switch on & initialize the TVO HW block */
		tvo_hw_init(tvo);

#if 0 /* Done when writing to TVO for the fisrt time */

		/* START ...........  */
		tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_START_FIELD, 0, TVO_START_1);

		tvo->started = 1;
#endif

		/* Set the power mode flag */
		tvo->power_mode = FB_BLANK_UNBLANK;
	}

	return ret;
}

/**
 * tvo_switchoff
 * @tvo: device which has been used to call this function
 *
 * This function is called by the probe function to switches the display off.
 */
static int tvo_switchoff(struct tvo_drvdata *tvo)
{
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	/* STOP ...........  */
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_STOP_FIELD, 0, TVO_STOP_1);

	/* Power down the device */
	tvo_set_reg_field(TVO_GENERAL_CTRL_REG, TVO_POWER_DOWN_FIELD, 0, TVO_POWER_DOWN_TVO_IN_POWER_DOWN);

	/* Reset flags */
	tvo->started = 0;
	tvo->odd_field = 0;

	/* Disable interrupt */
	tvo_disableInterrupt();

	/* disable clock */
	clk_disable(tvo->clk);

	/* disable PLL clock */
	clk_disable(tvo->clk_pll);

	/* disable power */
	pwr_disable(tvo->pwr);

	/* Set DDR constraint (default value) */
	ret = pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT, 
									TVO_NAME, PM_QOS_DEFAULT_VALUE);
	if (ret < 0) {
	    printk("%s (pm_qos_update_requirement (DDR) failed ! (%d))\n", 
		    __FUNCTION__, ret);
	}

	/* Set the power mode flag */
	tvo->power_mode = FB_BLANK_POWERDOWN;

	return ret;
}


/*
 =========================================================================
 =                                                                       =
 =              lcdfb_ops implementations                                =
 =                                                                       =
 =========================================================================
*/

/**
 * tvo_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function sends the list of lcdfb_transfer to the display controller 
 * using the underlying bus driver.
 */
static int
tvo_write(const struct device *dev,
			 const struct list_head *transfers)
{
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);
	struct lcdfb_transfer *transfer;
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	if (tvo->power_mode != FB_BLANK_UNBLANK) {
		pr_debug("%s():NOT allowed (refresh while power off)\n", 
				__FUNCTION__);
		return 0;
	}

	if (list_empty(transfers)) {
		dev_warn((struct device *)dev,
				"%s(): Got an empty transfer list\n", __FUNCTION__);
		return 0;
	}

	/* Write data to the TVOUT IP */
	list_for_each_entry(transfer, transfers, link) {
		ret |= tvo_write_data(tvo, transfer);
	}

	return ret;
}

/**
 * tvo_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev:	device which has been used to call this function
 * @fsi:	structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
tvo_get_fscreeninfo(const struct device *dev,
						struct fb_fix_screeninfo *fsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!fsi) {
		return -EINVAL;
	}

	*fsi = tvo_fix;
	return 0;
}

/**
 * tvo_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev:	device which has been used to call this function
 * @vsi:	structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
tvo_get_vscreeninfo(const struct device *dev,
						struct fb_var_screeninfo *vsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	*vsi = tvo_var;
	return 0;
}

/**
 * tvo_get_splash_info - copies the splash screen info into si
 * @dev:	device which has been used to call this function
 * @si:		structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
tvo_get_splash_info(struct device *dev, struct lcdfb_splash_info *si)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!si) {
		return -EINVAL;
	}

	*si = tvo_splash_info;
	return 0;
}


/**
 * tvo_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
tvo_get_specific_config(struct device *dev, struct lcdfb_specific_config **sc)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!sc) {
		return -EINVAL;
	}

	(*sc) = &tvo_specific_config;
	return 0;
}

/**
 * tvo_get_device_attrs
 * @dev: device which has been used to call this function
 * @device_attrs: device attributes to be returned
 *
 * Returns the device attributes to be added to the SYSFS
 * entries (/sys/class/graphics/fbX/....
 * */
static int 
tvo_get_device_attrs(struct device *dev, 
					 struct device_attribute **device_attrs,
					 unsigned int *attrs_number)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!device_attrs) {
		return -EINVAL;
	}

	(*device_attrs) = tvo_device_attrs;

	(*attrs_number) = sizeof(tvo_device_attrs) / sizeof(tvo_device_attrs[0]);

	return 0;
}

/**
 * tvo_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
tvo_display_on(struct device *dev)
{
	int ret = 0;
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch on the display if needed */
	if (tvo->power_mode != FB_BLANK_UNBLANK) {

		if (tvo->power_mode == FB_BLANK_POWERDOWN) {
			tvo_switchon(tvo);
		}
		else {
			/* Just set the power mode flag
			 * TVOUT was in standby or deep standbay mode */
			tvo->power_mode = FB_BLANK_UNBLANK;			
		}
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_UNBLANK mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}

/**
 * tvo_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
tvo_display_off(struct device *dev)
{
	int ret = 0;
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (tvo->power_mode != FB_BLANK_POWERDOWN) {
		tvo_switchoff(tvo);
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_POWERDOWN mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}

/**
 * tvo_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
tvo_display_standby(struct device *dev)
{
	int ret = 0;
	struct tvo_drvdata *tvo = dev_get_drvdata((struct device *)dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (tvo->power_mode != FB_BLANK_VSYNC_SUSPEND) {
		tvo->power_mode = FB_BLANK_VSYNC_SUSPEND;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_VSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}

/**
 * tvo_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
tvo_display_deep_standby(struct device *dev)
{
	int ret = 0;
	struct tvo_drvdata *tvo = dev_get_drvdata((struct device *)dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (tvo->power_mode != FB_BLANK_HSYNC_SUSPEND) {
		tvo->power_mode = FB_BLANK_HSYNC_SUSPEND;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_HSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}

/*
 * tvo_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * */
static int tvo_check_var(const struct device *dev,
						 struct fb_var_screeninfo *vsi)
{
	int ret = -EINVAL;
	u32 max_x, max_y, is_ntsc;
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	/* check x & y resolution */
	is_ntsc = TVO_IS_NTSC_STANDARD(tvo->out_standard);
	max_x = is_ntsc ? TVO_NTSC_MAX_WIDTH  : TVO_PAL_MAX_WIDTH;
	max_y = is_ntsc ? TVO_NTSC_MAX_HEIGHT : TVO_PAL_MAX_HEIGHT;

	if ((vsi->xres > max_x) || (vsi->yres > max_y))
		goto ko;

	/* check xyres virtual */
	if ((vsi->xres_virtual != tvo_var.xres_virtual) ||
		(vsi->yres_virtual != tvo_var.yres_virtual))
		goto ko;

	/* check xoffset */
	if (vsi->xoffset != tvo_var.xoffset)
		goto ko;

	/* check bpp */
	if ((vsi->bits_per_pixel != 16) &&
		(vsi->bits_per_pixel != 24))
		goto ko;

	/* check rotation */
	if (vsi->rotate != FB_ROTATE_UR) {
		/* Rotation not supported by the TVOUT IP */
		ret = -EPERM;
		goto ko;
	}

	/* Everything is ok */
	return 0;

ko:
	return ret;
}

/*
 * tvo_set_par
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to set
 *
 * */
static int tvo_set_par(const struct device *dev,
					   struct fb_info *info)
{
	int ret = -EINVAL;
	int set_params = 0;
	struct fb_var_screeninfo *vsi = &info->var;
	struct tvo_drvdata *tvo = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	/* Check the activation mode (see fb.h) */
	switch(vsi->activate)
	{
	case FB_ACTIVATE_NOW:		/* set values immediately (or vbl)*/
		set_params = 1;
		break;

	case FB_ACTIVATE_NXTOPEN:	/* activate on next open	*/
		break;

	case FB_ACTIVATE_TEST:		/* don't set, round up impossible */
		break;

	case FB_ACTIVATE_MASK: 		/* values			*/
		break;

	case FB_ACTIVATE_VBL:		/* activate values on next vbl  */
		break;

	case FB_CHANGE_CMAP_VBL:	/* change colormap on vbl	*/
		break;

	case FB_ACTIVATE_ALL:		/* change all VCs on this fb	*/
		set_params = 1;
		break;

	case FB_ACTIVATE_FORCE:		/* force apply even when no change*/
		set_params = 1;
		break;

	case FB_ACTIVATE_INV_MODE:	/* invalidate videomode */
		break;
	}	

	/* Check if params are different */
	if (set_params) {
		if (! memcmp(vsi, &tvo_var, sizeof(struct fb_var_screeninfo))) {
			ret = 0;
			goto quit;
		}

		/* Set new params */	
		ret = tvo_set_vscreeninfo(tvo, vsi);
	}

quit:
	return ret;
}


/**
 * tvo_ioctl
 * @dev: device which has been used to call this function
 * @cmd: requested command
 * @arg: command argument
 * */
static int
tvo_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	struct tvo_drvdata *tvo = dev_get_drvdata((struct device *)dev->parent);
	int ret = 0;
	unsigned int value;

	pr_debug("%s()\n", __FUNCTION__);

	switch (cmd) {

	/* */
	case PNXFB_GET_ZOOM_MODE:
		if (put_user(tvo->zoom_mode, (int __user *)arg)) {
			ret = -EFAULT;
		}
		break;

	/* */		
	case PNXFB_SET_ZOOM_MODE:
		if (get_user(value, (int __user *)arg)) {
			ret = -EFAULT;
		}
		else {
			if ((value >= PNXFB_ZOOM_MAX) || (value < PNXFB_ZOOM_NONE)) {
				ret = -EINVAL;
			}
			else {
				/* Save the new zoom mode */
				tvo->zoom_mode = value;

				/* Calculates the output size & Set the field formatter */
				tvo_calculate_output_size(tvo, 1);
			}
		}
		break;

	/* */
	case PNXFB_TVO_GET_OUT_STANDARD:
		if (put_user(tvo->out_standard, (int __user *)arg)) {
			ret = -EFAULT;
		}
		break;

	/* */
	case PNXFB_TVO_SET_OUT_STANDARD:
		if (get_user(value, (int __user *)arg)) {
			ret = -EFAULT;
		}
		else {
			ret = tvo_set_out_standard(dev, value);
		}
		break;

	/* */
	default:
		dev_err(dev, "Unknwon IOCTL command (%d)\n", cmd);
		ret = -ENOIOCTLCMD;
	}

	return ret;
}


/*
 =========================================================================
 =                                                                       =
 =              LCDFB operations supported by this driver                =
 =                                                                       =
 =========================================================================
*/
struct lcdfb_ops tvo_ops = {
	.write					= tvo_write,
	.get_fscreeninfo		= tvo_get_fscreeninfo,
	.get_vscreeninfo		= tvo_get_vscreeninfo,
	.get_splash_info		= tvo_get_splash_info,
	.get_specific_config	= tvo_get_specific_config,
	.get_device_attrs       = tvo_get_device_attrs,	
	.display_on				= tvo_display_on,
	.display_off			= tvo_display_off,
	.display_standby		= tvo_display_standby,
	.display_deep_standby	= tvo_display_deep_standby,	
	.check_var				= tvo_check_var,
	.set_par				= tvo_set_par,	
	.ioctl					= tvo_ioctl,
};


/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/

/*
 * Check which events have occured and peform the appropriate actions
 *
 */
irqreturn_t tvo_irq_cb(int irq, void *arg)
{
	u32 intStat, regValue;
	irqreturn_t ret = IRQ_NONE;
	struct tvo_drvdata *tvo = (struct tvo_drvdata *)arg;

	/* Read the current irq register value */
	regValue = ioread32(TVO_IRQ_STATUS_REG);

	/********************************************************************/
	/* This interrupt is handled as an event in streaming mode. It      */
	/* indicates that the settings for the next frame can be programmed */
	/* into the double-buffered registers of DTL2DVDO.                  */
	/* In non-streaming mode, this event is ignored.                    */
	/********************************************************************/
	intStat = tvo_get_reg_field(TVO_IRQ_STATUS_REG, TVO_SOF_IN_FIELD, TVO_SOF_IN_SHIFT);
	if (intStat) {
		ret = IRQ_HANDLED;
	}

	/*******************************************************************/
	/* This interrupt signals that the data buffer holding the picture */
	/* frame data in system RAM has been completely read. This buffer  */
	/* can therefore be re-used.                                       */
	/*******************************************************************/
	intStat = tvo_get_reg_field(TVO_IRQ_STATUS_REG, TVO_EOF_IN_FIELD, TVO_EOF_IN_SHIFT);
	if (intStat) {

		if (tvo->odd_field && tvo->frames_ready)
		{
			/* Notify for frame completion */
			complete(&tvo->write_done);

			tvo->frames_ready--;
		}

		tvo->odd_field = !tvo->odd_field;

		ret = IRQ_HANDLED;
	}

	/******************************************************************/
	/* These interrupts indicate an error condition which can only be */
	/* recovered from when the TVO device is reset.                   */
	/******************************************************************/
	intStat = tvo_get_reg_field(TVO_IRQ_STATUS_REG, TVO_FIFO_UNDERRUN_FIELD, TVO_FIFO_UNDERRUN_SHIFT);
	if (intStat) {

		printk("%s (TVOUT FIFO UNDERRUN)\n", __FUNCTION__);
		ret = IRQ_HANDLED;
		tvo_FIFORecovery(tvo);
	}

	/**************************************************************/
	/* This interrupt indicates that the TV has been connected or */
	/* disconnected to the TV output of the TVO device.           */
	/**************************************************************/
	intStat = tvo_get_reg_field(TVO_IRQ_STATUS_REG, TVO_LOAD_ID_DAC_FIELD, TVO_LOAD_ID_DAC_SHIFT);
	if (intStat) {

		ret = IRQ_HANDLED;
	}

	/*******************************************************************/
	/* This interrupt signals that the picutre frame which was held in */
	/* system RAM is being output to the TV.                           */
	/*******************************************************************/
	intStat = tvo_get_reg_field(TVO_IRQ_STATUS_REG, TVO_SOF_OUT_FIELD, TVO_SOF_OUT_SHIFT);
	if (intStat) {

		ret = IRQ_HANDLED;
	}

	/* Clear IRQ register */
	iowrite32(regValue, TVO_IRQ_CLEAR_REG);
	iowrite32(0,		TVO_IRQ_CLEAR_REG);

	return ret;
}


static int __devinit
tvo_probe(struct platform_device *pdev)
{
	struct tvo_drvdata *tvo;
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	tvo = kzalloc(sizeof(*tvo), GFP_KERNEL);
	if (!tvo) {
		printk("%s Failed ! (No more memory (tvo))\n", __FUNCTION__);
		ret = -ENOMEM;		
	}
	else {
		dev_set_drvdata(&pdev->dev, tvo);

		/* Save the global TVO instance */
		g_tvo = tvo;

		/* Set default TVO params */
		tvo_set_default_params(tvo);

		tvo->fb_size = tvo_var.xres * tvo_var.yres * tvo_var.bits_per_pixel;
		tvo->fb.ops = &tvo_ops;
		tvo->fb.dev.parent = &pdev->dev;
		snprintf(tvo->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", pdev->dev.bus_id);

		/* grab TVO clock */
		tvo->clk = clk_get(&pdev->dev, "TVO");
		if (IS_ERR(tvo->clk)) {
			printk("%s Failed ! (Could not get the clock of TVO)\n", __FUNCTION__);
			ret = -ENXIO;
			goto err_kfree;
		}

		/* grab TVO PLL clock */
		tvo->clk_pll = clk_get(&pdev->dev, "TVOPLL");
		if (IS_ERR(tvo->clk_pll)) {
			printk("%s Failed ! (Could not get the PLL clock of TVO)\n", __FUNCTION__);
			ret = -ENXIO;
			goto err_kfree;
		}

		/* grab TVO power */
		tvo->pwr = pwr_get(&pdev->dev, "TVO");
		if (IS_ERR(tvo->pwr)) {
			printk("%s Failed ! (Could not get the power of TVO)\n", __FUNCTION__);
			ret = -ENXIO;
			goto err_kfree;
		}

		init_completion(&tvo->write_done);

		/* Get & Register irq */
		tvo->irq = platform_get_irq(pdev, 0);
		if (tvo->irq == 0) {
			printk("%s Failed ! (Could not get irq for TVO)\n", __FUNCTION__);
			ret = -ENXIO;
			goto err_kfree;
		}

		ret = request_irq(tvo->irq, tvo_irq_cb, IRQF_DISABLED, "tvo", tvo);
		if (ret) {
			printk("%s Failed ! (Could not register irq function for TVO)\n", __FUNCTION__);
			ret = -ENXIO;
			goto err_kfree;
		}

		/* Initialize the TVO if the initial state is ON (FB_BLANK_UNBLANK) */
		if (tvo->power_mode == FB_BLANK_UNBLANK) {
			ret = tvo_switchon(tvo);
			if (ret != 0) {
				printk("%s Failed ! (Could not bootstrap the TVOut)\n", __FUNCTION__);
				ret = -EBUSY;
				goto err_kfree;
			}
		}

		/* Rgister the FB device */
		ret = lcdfb_device_register(&tvo->fb);
		if (ret < 0) {
			printk("%s Failed ! (Unable to register lcdfb device)\n", __FUNCTION__);
			goto err_kfree;
		}
	}

	return ret;

err_kfree:
	kfree(tvo);
	return ret;
}

static int __devexit
tvo_remove(struct platform_device *pdev)
{
	struct tvo_drvdata *tvo = dev_get_drvdata(&pdev->dev);

	pr_debug("%s()\n", __FUNCTION__);

	/* Unregister device */
	lcdfb_device_unregister(&tvo->fb);

	/* Switch off the TVO HW block */
	tvo_display_off(&pdev->dev);

	/* release clock */
	clk_put(tvo->clk);

	/* release clock */
	pwr_put(tvo->pwr);

	/* free the irq */
	free_irq(tvo->irq, tvo);

	/* Free allocated data */
	kfree(tvo);

	return 0;
}

static struct platform_driver tvo_driver = {
	.driver.name = TVO_NAME,
	.driver.owner = THIS_MODULE,
	.probe = tvo_probe,
	.remove = tvo_remove,
};

static int __init
tvo_init(void)
{
	int ret;
	struct proc_dir_entry *entry;

	pr_debug("%s()\n", __FUNCTION__);

	/* Create /proc/tvout */
	entry = create_proc_entry("tvout" /*TVO_NAME */, S_IRUGO, NULL);
	if (! entry) {
		printk("%s (Unable to create /proc/tvout entry)\n", __FUNCTION__);
	}
	else {
		entry->proc_fops = &tvo_procOps;
	}
	
    ret = pm_qos_add_requirement (PM_QOS_DDR_THROUGHPUT, 
								TVO_NAME, PM_QOS_DEFAULT_VALUE);
    if (ret < 0) {
		printk("%s (pm_qos_add_requirement (DDR) failed ! (%d))\n", __FUNCTION__, ret);
    }

	/* Register the plaform driver */
	ret = platform_driver_register(&tvo_driver);

	return ret; 
}

static void __exit
tvo_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	/* Remove /proc/tvout entry */
	remove_proc_entry("tvout" /*TVO_NAME */, NULL);

    /* Remove QOS requirement */
    pm_qos_remove_requirement(PM_QOS_DDR_THROUGHPUT, TVO_NAME);

	/* Unregister the plaform driver */	
	platform_driver_unregister(&tvo_driver);
}

module_init(tvo_init);
module_exit(tvo_exit);

MODULE_AUTHOR("Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("TV Out IP driver");
MODULE_LICENSE("GPL");
