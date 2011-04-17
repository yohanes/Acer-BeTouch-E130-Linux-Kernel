/*
 * linux/drivers/video/pnx/displays/lc822973.c
 *
 * lc822973 LCD driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <mach/gpio.h>
#include <video/pnx/lcdbus.h>
#include <video/pnx/lcdctrl.h>

#include "lc822973.h"

/*
 * TODO improve rotation management
 * TODO	check different screen resolutions (for instance, 640x480...)
 * TODO	implement display_on, off, sleep...
 * TODO	the probe does not initialized the TV out IC because during the
 *	    probe (boot), the pmu is not ready. So, during the first writes
 *	    the TV out IC will be initialized. See function write();
 *
 * TODO Manage all ACTIVATE mode in jbt6k71_set_par
 *
 * */

#include "../lcdbus_pagefaultfb.h"

/* Specific TVO and FB configuration
 * explicit_refresh:   1 to activate the explicit blit (based on panning calls,
 *                  usefull if the mmi uses always double buffering).
 * boot_power_mode: FB_BLANK_UNBLANK = DISPLAY ON
 *                  FB_BLANK_VSYNC_SUSPEND = DISPLAY STANDBY (OR SLEEP)
 *                  FB_BLANK_HSYNC_SUSPEND = DISPLAY SUSPEND (OR DEEP STANDBY)
 *                  FB_BLANK_POWERDOWN = DISPLAY OFF
 *                  FB_BLANK_NORMAL = not used for the moment
 * */
static struct lcdfb_specific_config lc822973_specific_config = {
	.explicit_refresh   = 0,
	.boot_power_mode = FB_BLANK_POWERDOWN,
};


/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_LC822973_KERNEL_SPLASH_SCREEN
#include "lc822973_splash.h"
static struct lcdfb_splash_info lc822973_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = lc822973_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(lc822973_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info lc822973_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif


/* VDE color format vs. TVO color format (usefull for hw color conversion */
static struct lcdbus_conf lc822973_busconf = {

// Case 1: 16BPP
#if (LC822973_FB_BPP == 16)
	.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB565,
	.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB565,

	.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_RGB565,
	.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_RGB565,

// Case 2: 24PP
#elif (LC822973_FB_BPP == 24)
	.data_ifmt = LCDBUS_INPUT_DATAFMT_TRANSP,
	.data_ofmt = LCDBUS_OUTPUT_DATAFMT_TRANSP,

	.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP,
	.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP,

// Case 3: 32BPP
#elif (LC822973_FB_BPP == 32)
	.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB888,
	.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB565,

	.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_RGB888,
	.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_RGB565,

#else
	#error "Unsupported color depth (see driver doc)"
#endif

};


/* FB_FIX_SCREENINFO (see fb.h) */
static const struct fb_fix_screeninfo lc822973_fix = {
	.id          = LC822973_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (LC822973_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = LC822973_SCREEN_WIDTH * (LC822973_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo lc822973_var = {
	.xres           = LC822973_SCREEN_WIDTH,
	.yres           = LC822973_SCREEN_HEIGHT,
	.xres_virtual   = LC822973_SCREEN_WIDTH,
	.yres_virtual   = LC822973_SCREEN_HEIGHT * (LC822973_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = LC822973_FB_BPP,

#if (LC822973_FB_BPP == 32)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (LC822973_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#else
	.red            = {11, 5, 0},
	.green          = {5, 6, 0},
	.blue           = {0, 5, 0},

#endif

	.vmode          = FB_VMODE_NONINTERLACED,
	.height         = 44,
	.width          = 33,
	.rotate         = FB_ROTATE_UR,
};

/* Hw LCD TIMINGS (adapted from GIDISCFG / gidiscfg.hec  */
static struct lcdbus_timing lc822973_timing = {
	/* bus */
#if (LC822973_FB_BPP == 24)
	.mode = 1, /* VDE_CONF_MODE_8_BIT_PARALLEL_INTERFACE */
#else
	.mode = 0, /* VDE_CONF_MODE_16_BIT_PARALLEL_INTERFACE */
#endif

	.ser  = 0, /* serial mode is not used*/
	.hold = 0, /* VDE_CONF_HOLD_A0_FOR_COMMAND */
	/* read */
	.rh = 5,
	.rc = 5,
	.rs = 0,
	/* write */
	.wh = 4,
	.wc = 4,
	.ws = 1,
	/* misc */
	.ch = 0,
	.cs = 0,
};

/* ----------------------------------------------------------------------- */


/* LC822973 driver data */
static const char lc822973_name[] = LC822973_NAME;

/* Cached register values of the LC822973 display */
static u16 lc822973_register_values[LC822973_REGISTERMAP_SIZE];

struct lc822973_drvdata {
	struct mutex lock;
	struct lcdfb_device fb;
	struct list_head commands;
	struct fb_var_screeninfo vsi;
	u32 power_mode;
	u32 line_length;
	u32 xres;
	u32 yoffset;
	u32 byte_shift;
	u32 angle;
};


/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/

/**
 * lc822973_free_cmd_list - removes and free's all lcdbus_cmd's from commands
 * @commands: a LLI list of lcdbus_cmd's
 *
 * This function frees the given lcdbus command list.
 */
static void
lc822973_free_cmd_list(struct list_head *commands)
{
	struct lcdbus_cmd *cmd, *tmp;

	pr_debug("%s()\n", __FUNCTION__);

	list_for_each_entry_safe(cmd, tmp, commands, link) {
		list_del(&cmd->link);
		kfree(cmd);
	}
}

/**
 * lc822973_add_cmd_to_cmd_list - add a request to the internal command chain
 * @dev: device which has been used to call this function
 * @reg: the register that is addressed
 * @value: the value that should be copied into the register
 *
 * This function adds the given command to the internal command chain
 * of the LCD driver.
 */
static inline int
lc822973_add_cmd_to_cmd_list(struct list_head *commands, const u16 reg,
		u32 value)
{
	struct lcdbus_cmd *cmd;

	pr_debug("%s()\n", __FUNCTION__);

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL | GFP_DMA);
	if (cmd == NULL) {
		return -ENOMEM;
	}

	list_add_tail(&cmd->link, commands);
	cmd->cmd = LOBYTE(reg);
	cmd->data_phys = cmd->data_int_phys;
#if (LC822973_FB_BPP == 24)
	cmd->data_int[0] = HIBYTE(value);
	cmd->data_int[1] = LOBYTE(value);
#else
	cmd->data_int[0] = LOBYTE(value);
	cmd->data_int[1] = HIBYTE(value);
#endif
	cmd->len = 2;
	cmd->type = LCDBUS_CMDTYPE_CMD;

	return 0;
}

/**
 * lc822973_add_data_to_cmd_list - add a request to the internal command chain
 * @dev: device which has been used to call this function
 * @data: the data to send
 * @len: the length of the data to send
 *
 * This function adds the given command and its assigned data to the internal
 * command chain of the LCD driver. Note that this function can only be used
 * for write commands and that the data buffer must retain intact until
 * transfer is finished.
 */
static inline int
lc822973_add_data_to_cmd_list(struct list_head *commands, u8 *data, u32 len)
{
	struct lcdbus_cmd *cmd;

	pr_debug("%s()\n", __FUNCTION__);

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL | GFP_DMA);
	if (cmd == NULL) {
		return -ENOMEM;
	}

	list_add_tail(&cmd->link, commands);
	cmd->cmd = LOBYTE(LC822973_REG(imgwrite));
	cmd->data_phys = data;
	cmd->len = len;
	cmd->type = LCDBUS_CMDTYPE_DATA;

	return 0;
}

/**
 * lc822973_add_transfer_to_cmd_list - adds lcdbus_cmd's from a transfer to list
 * @dev: device which has been used to call this function
 * @transfer: the lcdfb_transfer to be converted
 * @commands: a LLI list of lcdbus_cmd's
 *
 * This function creates a command list for a given transfer. Therefore it
 * selects the x- and y-start position in the display ram, issues the write-ram
 * command and sends the data.
 */
static int
lc822973_add_transfer_to_cmd_list(struct lc822973_drvdata *drvdata,
	                              struct list_head *commands,
	                              struct lcdfb_transfer *transfer)
{
	int area_x, area_y, area_w, area_h, zoom, scandir, scale, tmp;
	int startx, starty;
	static int imgbank = 0;
	int imgbankyofs;

	pr_debug("%s()\n", __FUNCTION__);

	area_w = lc822973_var.xres;
	area_h = lc822973_var.yres;
	/* 2nd bank (buffer) y offset */
	imgbankyofs = 512;


	/* Configure the zoom and the rotation */
/*
 * FB_ROTATE_UR      0 - normal orientation (0 degree)
 * FB_ROTATE_CW      1 - clockwise orientation (90 degrees)
 * FB_ROTATE_UD      2 - upside down orientation (180 degrees)
 * FB_ROTATE_CCW     3 - counterclockwise orientation (270 degrees
 *
 * */

	switch(lc822973_var.rotate)
	{
	case FB_ROTATE_UR:
		scale = 0xA5; // Zoom = 1.3
		zoom = (256 * scale * 2) / 0xFF;
		area_x = 0x00C0;
		area_y = 0x0010;
		startx = 0;
		starty = 0;
		scandir = 0;
		break;

	case FB_ROTATE_CW:
		tmp = area_w; area_w = area_h; area_h = tmp; // permute w and h
		scale = 0xFF; // Zoom =~ 2
		zoom = (256 * scale * 2) / 0xFF;
		area_x = 0x0020;
		area_y = 0x0000;
		startx = 0;
		starty = area_h;
		scandir = 3;
		break;

	case FB_ROTATE_UD:
		scale = 0xA5; // Zoom = 1.3
		zoom = (256 * scale * 2) / 0xFF;
		area_x = 0x00C0;
		area_y = 0x0010;
		startx = 0;
		starty = area_h;
		scandir = 4;
		break;

	case FB_ROTATE_CCW:
		tmp = area_w; area_w = area_h; area_h = tmp; // permute w and h
		scale = 0xFF; // Zoom =~ 2
		zoom = (256 * scale * 2) / 0xFF;
		area_x = 0x0020;
		area_y = 0x0000;
		startx = 0;
		starty = 0;
		scandir = 1;
		break;

	default: // No rotation
		printk("Warning %s(): invalid parameter\n", __FUNCTION__);
		scale = 0xA5; // Zoom = 1.3
		zoom = (256 * scale * 2) / 0xFF;
		area_x = 0x00C0;
		area_y = 0x0010;
		startx = 0;
		starty = 0;
		scandir = 0;
	}

	// Reset Int
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(Int), 0x07FF);

	// TODO comment me!
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(encbst1), 0x0020);

	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(encmode), 0x0000);


	if (imgbank == 0) {
		// Set the real-time image reading area
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(rfbhofst), 0x0000);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(rfbvofst), 0x0000);

	 	/* Set the Tvout SDRAM area */
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhofst), 0x0000);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvofst),
				imgbankyofs);

		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhlen), area_w);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvlen), area_h);

		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhstart),
				0x0000 + startx);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvstart),
				imgbankyofs + starty);
	} else {
		// Set the real-time image reading area
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(rfbhofst), 0x0000);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(rfbvofst),
				imgbankyofs);

	 	/* Set the Tvout SDRAM area */
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhofst), 0x0000);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvofst), 0x0000);

		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhlen), area_w);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvlen), area_h);

		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbhstart),
				0x0000 + startx);
		lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(wfbvstart),
				0x0000 + starty);
	}

	imgbank = 1 - imgbank; // change image bank

	// set vscale and hscale
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(scale),
				scale | (scale << 8));

	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(sysctl1),
			0x27 | (scandir << 6));

	// Set the TV display area
	area_w = ((area_w * zoom) / 256) & 0xFFFF; // parity?
	area_h = ((area_h * zoom) / 256) & 0xFFFF; // parity?

	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(dsphofst), area_x);
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(dspvofst), area_y);

	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(dsphlen), area_w) ;
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(dspvlen), area_h);


	// Set data
	lc822973_add_data_to_cmd_list(commands,  transfer->addr, transfer->count);
	lc822973_add_cmd_to_cmd_list(commands, LC822973_REG(imgabort), 0x0000);

	return 0;
}


/**
 * lc822973_execute_cmd_list - execute the internal command chain
 * @dev: device which has been used to call this function
 * @issue: the issue to handle
 *
 * This function executes the internal command chain by sending it
 * to the lcdbus layer. Afterwards it cleans the internal command
 * chain.
 */
static int
lc822973_execute_cmd_list(struct device *dev,
		const struct lc822973_issue *issue)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct lc822973_drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

	ret = bus->write(dev, &drvdata->commands);
	lc822973_free_cmd_list(&drvdata->commands);

	return ret;
}

/**
 * lc822973_wait_issue - waits the given amount of ms
 * @dev: device which has been used to call this function
 * @issue: the issue to handle
 *
 * This function waits the given amount of msecs.
 */
static int
lc822973_wait_issue(struct device *dev, const struct lc822973_issue *issue)
{
	pr_debug("%s()\n", __FUNCTION__);

	mdelay(issue->data);
	return 0;
}

/**
 * lc822973_execute_issue - execute an issue
 * @dev: device which has been used to call this function
 * @issue: the issue to handle
 *
 * This function executes issues according to a given command
 * list.
 */
static inline int
lc822973_execute_issue(struct device *dev, const struct lc822973_issue *issue,
	                  const struct lc822973_cmd *cmds)
{
	int ret = 0;

	if ((cmds + issue->cmd) != NULL) {
		ret = (cmds + issue->cmd)->cmd(dev, issue);
	}

	return ret;
}

static int
lc822973_execute_list(struct device * dev, const struct lc822973_issue * issue,
	   const struct lc822973_cmd * commands)
{
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	for (; !ret && issue->cmd != LC822973_END; issue++) {
		ret |= lc822973_execute_issue(dev, issue, commands);
	}

	return ret;
}

/**
 * lc822973_write_issue - write a value to the given register
 * @dev: device which has been used to call this function
 * @issue: the issue to handle
 *
 * This function writes the given register value
 */
static int __devinit
lc822973_write_issue(struct device *dev, const struct lc822973_issue *issue)
{
	struct lc822973_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	/* for bootstrapping we don't need modify, just overwrite */
	lc822973_register_values[issue->reg] = (u16)issue->data;

	/* now write new value to hardware */
	lc822973_add_cmd_to_cmd_list(&drvdata->commands, issue->reg, issue->data);

	return 0;
}

/*
 * LC822973 command implementation
 */
static const __devinitdata struct lc822973_cmd lc822973_cmds[] = {
	[LC822973_WRITE] = {.cmd = lc822973_write_issue},
	[LC822973_EXEC]  = {.cmd = lc822973_execute_cmd_list},
	[LC822973_WAIT]  = {.cmd = lc822973_wait_issue},
};


/*
 =========================================================================
 =                                                                       =
 =              device detection and bootstrapping                       =
 =                                                                       =
 =========================================================================
*/

/**
 * lc822973_device_supported - perform hardware detection check
 * @dev:	pointer to the device which should be checked for support
 *
 * XXX: maybe there is a workaround possible to fake detection.
 */
static int __devinit
lc822973_device_supported(struct device *dev)
{
	/*
	 * Hardware detection of the display does not seem to be supported
	 * by the hardware, thus we assume the display is there!
	 */

#ifdef CONFIG_FB_LCDBUS_VDE
	if (!strcmp(dev->bus_id, "pnx-vde-lcd1"))
#else
	if (!strcmp(dev->bus_id, "devvde0-lcd1"))
#endif
		return 0;
	else
		/*
		 * pnx-vde-lcd1 not detected (pb during VDE init)
		 * */
		return -ENODEV;
}

/**
 * lc822973_bootstrap - initialize the display
 * @dev:	the device which should be initialized
 *
 * Bootstrap the display
 */
static int __devinit
lc822973_bootstrap(struct device *dev)
{
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

    /* Hw reset with GPIOE10 */
	// FIXME usefull? maybe done during the boot
	pnx_set_gpio_mode(GPIO_E10, GPIO_MODE_MUX1);
	// FIXME usefull? maybe done during the boot
	pnx_set_gpio_direction(GPIO_E10, GPIO_DIR_OUTPUT);
	pnx_write_gpio_pin(GPIO_E10, 1);
	mdelay(10); // FIXME usefull?
	pnx_write_gpio_pin(GPIO_E10, 0);
	mdelay(10); // FIXME usefull?
	pnx_write_gpio_pin(GPIO_E10, 1);
	mdelay(10); // FIXME usefull?

	/* Execute bootsrap init sequence */
	ret = lc822973_execute_list(dev, lc822973_bootstrap_values, lc822973_cmds);

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
 * lc822973_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function converts the list of lcdfb_transfer into a list of resulting
 * lcdbus_cmd's which then gets sent to the display controller using the
 * underlying bus driver.
 */
static int
lc822973_write(const struct device *dev, const struct list_head *transfers)
{
	struct lc822973_drvdata *drvdata =
		dev_get_drvdata((struct device *)dev->parent);
	struct lcdfb_device *fb = to_lcdfb_device(dev);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;
	struct list_head commands;
	struct lcdfb_transfer *transfer;
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		dev_warn((struct device *)dev,
				"%s(): NOT allowed (refresh while power off)\n", __FUNCTION__);
		return 0;
	}

	if (list_empty(transfers)) {
		dev_warn((struct device *)dev,
				"%s(): Got an empty transfer list\n", __FUNCTION__);
		return 0;
	}

	/* now get on with the real stuff */
	INIT_LIST_HEAD(&commands);

	/* moved out of the loop for performance improvements */
	fb->ops->get_vscreeninfo(dev, &drvdata->vsi);
	drvdata->xres = drvdata->vsi.xres - 1;
	drvdata->yoffset = drvdata->vsi.yoffset;
	drvdata->byte_shift = (drvdata->vsi.bits_per_pixel >> 3) - 1;
	drvdata->line_length = lc822973_fix.line_length;

	list_for_each_entry(transfer, transfers, link) {
		ret |= lc822973_add_transfer_to_cmd_list(drvdata, &commands, transfer);
	}

	if (ret >= 0) {
		/* execute the cmd list we build */
		ret = bus->write(dev->parent, &commands);
	}

	/* Now buffers may be freed by the application */
	lc822973_free_cmd_list(&commands);

	return ret;
}


/**
 * lc822973_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev:	device which has been used to call this function
 * @fsi:	structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
lc822973_get_fscreeninfo(const struct device *dev,
		struct fb_fix_screeninfo *fsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!fsi) {
		return -EINVAL;
	}

	*fsi = lc822973_fix;
	return 0;
}

/**
 * lc822973_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev:	device which has been used to call this function
 * @fsi:	structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
lc822973_get_vscreeninfo(const struct device *dev,
		struct fb_var_screeninfo *vsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	*vsi = lc822973_var;
	return 0;
}

/*
 * lc822973_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int lc822973_check_var(const struct device *dev,
		struct fb_var_screeninfo *vsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	/* check xyres */
	if ((vsi->xres != lc822973_var.xres) ||
		(vsi->yres != lc822973_var.yres))
		goto ko;

	/* check xyres virtual */
	if ((vsi->xres_virtual != lc822973_var.xres_virtual) ||
		(vsi->yres_virtual != lc822973_var.yres_virtual))
		goto ko;

	/* check xoffset */
	if (vsi->xoffset != lc822973_var.xoffset)
		goto ko;

	/* check bpp */
	if (vsi->bits_per_pixel != lc822973_var.bits_per_pixel)
		goto ko;

	/* check rotation */
/*
 * FB_ROTATE_UR      0 - normal orientation (0 degree)
 * FB_ROTATE_CW      1 - clockwise orientation (90 degrees)
 * FB_ROTATE_UD      2 - upside down orientation (180 degrees)
 * FB_ROTATE_CCW     3 - counterclockwise orientation (270 degrees
 *
 * */

	if (vsi->rotate > 3)
		goto ko;

	/* Everything is ok */
	return 0;

ko:
	return -EINVAL;
}

/*
 * lc822973_set_par
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to set
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int lc822973_set_par(const struct device *dev,
	 						 struct fb_info *info)
{
	int ret = -EINVAL;
	int set_params = 0;
	struct fb_var_screeninfo *vsi = &info->var;

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

	if (set_params) {
		memcpy(&lc822973_var, vsi, sizeof(struct fb_var_screeninfo));

		ret = 0;
	}

	return ret;
}


/**
 * lc822973_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
lc822973_display_on(struct device *dev)
{
	int ret = 0;
	struct lc822973_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch on the display if needed */
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		// FIXME : be sure the PMU LDO driving the TV out is ON
		ret = lc822973_bootstrap(dev->parent);
		if (ret != 0) {
			dev_err(dev, "%s(): Could not bootstrap display %s\n",
					__FUNCTION__, dev->bus_id);
			return 0; /* TODO we should probably return something !=0 ...*/
		}

		ret = lc822973_execute_list(dev->parent,
									lc822973_display_on_seq,
									lc822973_cmds);

		drvdata->power_mode = FB_BLANK_UNBLANK;
	}

	return ret;
}

/**
 * lc822973_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
lc822973_display_off(struct device *dev)
{
	int ret = 0;
	struct lc822973_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (drvdata->power_mode != FB_BLANK_POWERDOWN) {
		ret = lc822973_execute_list(dev->parent,
									lc822973_display_off_seq,
									lc822973_cmds);

		drvdata->power_mode = FB_BLANK_POWERDOWN;
	}

	return ret;
}


/**
 * lc822973_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
lc822973_display_standby(struct device *dev)
{
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	ret = lc822973_execute_list(dev,
								lc822973_display_standby_seq,
								lc822973_cmds);

	return  ret;
}

/**
 * lc822973_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
lc822973_display_deep_standby(struct device *dev)
{
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	ret = lc822973_execute_list(dev,
								lc822973_display_deep_standby_seq,
								lc822973_cmds);

	return  ret;
}

/**
 * lc822973_get_splash_info - copies the splash screen info into si
 * @dev:	device which has been used to call this function
 * @si:		structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
lc822973_get_splash_info(struct device *dev,
		struct lcdfb_splash_info *si)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!si) {
		return -EINVAL;
	}

	*si = lc822973_splash_info;
	return 0;
}


/**
 * lc822973_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
lc822973_get_specific_config(struct device *dev,
		struct lcdfb_specific_config **sc)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!sc) {
		return -EINVAL;
	}

	(*sc) = &lc822973_specific_config;
	return 0;
}

/*
 * TVO operations supported by this driver
 */
struct lcdfb_ops lc822973_ops = {
	.write					= lc822973_write,
	.get_fscreeninfo		= lc822973_get_fscreeninfo,
	.get_vscreeninfo		= lc822973_get_vscreeninfo,
	.check_var				= lc822973_check_var,
	.set_par				= lc822973_set_par,
	.display_on				= lc822973_display_on,
	.display_off			= lc822973_display_off,
	.display_standby		= lc822973_display_standby,
	.display_deep_standby	= lc822973_display_deep_standby,
	.get_splash_info		= lc822973_get_splash_info,
	.get_specific_config	= lc822973_get_specific_config,
};


/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/

static int __devinit
lc822973_probe(struct device *dev)
{
	struct lc822973_drvdata *drvdata;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	int ret;

	dev_dbg(dev, "%s()\n", __FUNCTION__);

	if (!bus || !bus->read || !bus->write || !bus->get_conf ||
	       !bus->set_conf || !bus->set_timing) {
		return -EINVAL;
	}

	/* --------------------------------------------------------------------
	 * Set display timings before anything else happens
	 * ---- */
	bus->set_timing(dev, &lc822973_timing);

	/* --------------------------------------------------------------------
	 * Hardware detection
	 * ---- */
	ret = lc822973_device_supported(dev);
	if (ret < 0)
		goto out;

	dev_info(dev, "detected as lc822973 display\n");

	ret = -ENOMEM;
	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		dev_err(dev, "%s(): No more memory (drvdata)\n", __FUNCTION__);
		goto out;
	}

	dev_set_drvdata(dev, drvdata);

	drvdata->power_mode = lc822973_specific_config.boot_power_mode;
	drvdata->fb.ops = &lc822973_ops;
	drvdata->fb.dev.parent = dev;
	snprintf(drvdata->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", dev->bus_id);

	INIT_LIST_HEAD(&(drvdata->commands));

	/* --------------------------------------------------------------------
	 * Bootstrap the display if initial state is ON (FB_BLANK_UNBLANK)
	 * ---- */
	if (drvdata->power_mode == FB_BLANK_UNBLANK) {
		ret = lc822973_bootstrap(dev);
		if (ret != 0) {
			dev_err(dev, "%s(): Could not bootstrap display %s\n",
				__FUNCTION__, dev->bus_id);
			goto out;
		}
	}

	/* --------------------------------------------------------------------
	 * Set the bus configuration for the display
	 * ---- */
	bus->set_conf(dev, &lc822973_busconf);

	ret = lcdfb_device_register(&drvdata->fb);

out:
	return ret;
}

static int __devexit
lc822973_remove(struct device *dev)
{
	struct lc822973_drvdata *drvdata;

	dev_dbg(dev, "%s()\n", __FUNCTION__);

	drvdata = dev_get_drvdata(dev);
	lcdfb_device_unregister(&drvdata->fb);

	kfree(drvdata);

	return 0;
}

static struct device_driver lc822973_driver = {
	.owner = THIS_MODULE,
	.name = lc822973_name,
	.bus = &lcdctrl_bustype,
	.probe = lc822973_probe,
	.remove = lc822973_remove,
};

static int __init
lc822973_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	return driver_register(&lc822973_driver);
}

static void __exit
lc822973_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	driver_unregister(&lc822973_driver);
}

module_init(lc822973_init);
module_exit(lc822973_exit);

MODULE_AUTHOR("ST-Ericsson");
MODULE_DESCRIPTION("LCDBUS driver for the lc822973 TV out IC");
MODULE_LICENSE("GPL");
