/*
 * linux/drivers/video/pnx/displays/ili9481bb.c
 *
 * ili9481bb HVGA LCD driver
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
#include <video/pnx/lcdbus.h>
#include <video/pnx/lcdctrl.h>
#include <mach/pnxfb.h>
#include <mach/gpio.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>

#include "ili9481bb.h"


/*
 * NOTE The address_mode register settings are TRICKY (dose not really
 *   what is indicated in the datasheet !!!!
 *
 * FIXME Manage correctly the Tearing effect !
 *       (get more info about how TE works for this LCD)
 *
 * TODO Test the 262K color mode
 * TODO Tunning for sleep/deep sleep mode
 * TODO x panning has not been tested (but problably less important
 *      than y panning)
 *
 * TODO Manage all ACTIVATE mode in ili9481bb_set_par
 *
 * */

/* Specific LCD and FB configuration
 * explicit_refresh: 1 to activate the explicit refresh (based on panning calls,
 *                  usefull if the mmi uses always double buffering).
 *
 * boot_power_mode: FB_BLANK_UNBLANK = DISPLAY ON
 *                  FB_BLANK_VSYNC_SUSPEND = DISPLAY STANDBY (OR SLEEP)
 *                  FB_BLANK_HSYNC_SUSPEND = DISPLAY SUSPEND (OR DEEP STANDBY)
 *                  FB_BLANK_POWERDOWN = DISPLAY OFF
 *                  FB_BLANK_NORMAL = not used for the moment
 * */
static struct lcdfb_specific_config ili9481bb_specific_config = {
	.explicit_refresh = 1,
	.boot_power_mode = FB_BLANK_UNBLANK,
};


/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_ILI9481BB_KERNEL_SPLASH_SCREEN
#include "ili9481bb_splash.h"
static struct lcdfb_splash_info ili9481bb_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = ili9481bb_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(ili9481bb_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info ili9481bb_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif

/* FB_FIX_SCREENINFO (see fb.h) */
static struct fb_fix_screeninfo ili9481bb_fix = {
	.id          = ILI9481BB_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (ILI9481BB_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = ILI9481BB_SCREEN_WIDTH * (ILI9481BB_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo ili9481bb_var = {
	.xres           = ILI9481BB_SCREEN_WIDTH,
	.yres           = ILI9481BB_SCREEN_HEIGHT,
	.xres_virtual   = ILI9481BB_SCREEN_WIDTH,
	.yres_virtual   = ILI9481BB_SCREEN_HEIGHT * (ILI9481BB_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = ILI9481BB_FB_BPP,

#if (ILI9481BB_FB_BPP == 32)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (ILI9481BB_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (ILI9481BB_FB_BPP == 16)
	.red            = {11, 5, 0},
	.green          = {5, 6, 0},
	.blue           = {0, 5, 0},

#else
	#error "Unsupported color depth (see driver doc)"
#endif

	.vmode          = FB_VMODE_NONINTERLACED,
	.height         = 69,
	.width          = 41,
	.rotate         = FB_ROTATE_UR,
};


/* Hw LCD timings */
static struct lcdbus_timing ili9481bb_timing = {
	/* bus */
	.mode = 0, /* 16 bits // interface */
	.ser  = 0, /* serial mode is not used */
	.hold = 0, /* VDE_CONF_HOLD_A0_FOR_COMMAND */
	/* read */
	.rh = 7,
	.rc = 4,
	.rs = 0,
	/* write */
	.wh = 2,
	.wc = 4,
	.ws = 1,
	/* misc */
	.ch = 0,
	.cs = 0,
};

/* ----------------------------------------------------------------------- */

/* ILI9481BB driver data */
/*
 * @dev :
 * @fb  :
 * @bus :
 * @cmds_list: commands list virtual address
 * @curr_cmd : current command pointer
 * @last_cmd : last command pointer
 * @lock :
 * @osd0 :
 * @osd1 :
 * @power_mode :
 * @zoom_mode  :
 * @use_262k_colors :
 */
struct ili9481bb_drvdata {

	struct lcdfb_device fb;

	struct lcdbus_cmd *cmds_list;
	u32    cmds_list_phys;
	u32    cmds_list_max_size;
	struct lcdbus_cmd *curr_cmd;
	struct lcdbus_cmd *last_cmd;

	u16 power_mode;
	u16 zoom_mode;
	u16 use_262k_colors;
};

/*
 =========================================================================
 =                                                                       =
 =          SYSFS  section                                               =
 =                                                                       =
 =========================================================================
*/
/* ili9481bb_show_explicit_refresh
 *
 */
static ssize_t
ili9481bb_show_explicit_refresh(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", ili9481bb_specific_config.explicit_refresh);
}


/* ili9481bb_store_explicit_refresh
 *
 */
static ssize_t
ili9481bb_store_explicit_refresh(struct device *device,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	if (strncasecmp(buf, "0", count - 1) == 0) {
		ili9481bb_specific_config.explicit_refresh = 0;
	}
	else if (strncasecmp(buf, "1", count - 1) == 0) {
		ili9481bb_specific_config.explicit_refresh = 1;
	}

	return count;
}


static struct device_attribute ili9481bb_device_attrs[] = {
	__ATTR(explicit_refresh, S_IRUGO|S_IWUSR, ili9481bb_show_explicit_refresh,
			ili9481bb_store_explicit_refresh)
};

/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/

static void ili9481bb_set_cols_rows(struct device *dev,
					struct list_head *cmds,
					u16 x_start, u16 x_end,
					u16 y_start, u16 y_end);

/**
 * ili9481bb_set_bus_config - Sets the bus (VDE) config
 *
 * Configure the VDE colors format according to the LCD
 * colors formats (conversion)
 */
static int
ili9481bb_set_bus_config(struct device *dev)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);
	struct lcdbus_conf busconf;
	int ret = 0;

	/* Tearing management */
	busconf.eofi_del  = 0;
	busconf.eofi_skip = 0;
	busconf.eofi_pol  = 0;
	busconf.eofi_use_vsync = 0; // TE pin is not connected !

	/* CSKIP & BSWAP params */
	busconf.cskip = 0;
	busconf.bswap = 0;

	/* Align paremeter */
	busconf.align = 0;

	/* Data & cmd params */
	busconf.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP;
	busconf.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS;

	switch (ili9481bb_var.bits_per_pixel) {

	// Case 1: 16BPP
	case 16:
		busconf.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB565;

		if (drvdata->use_262k_colors)
			busconf.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB666;
		else
			busconf.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB565;
		break;

	// Case 2: 24BPP
	case 24:
		busconf.data_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP;
		busconf.data_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS;
		break;

	// Case 3: 32BPP
	case 32:
		busconf.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB888;

		if (drvdata->use_262k_colors)
			busconf.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB666;
		else
			busconf.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB565;
		break;

	default:
		dev_err(dev, "(Invalid color depth value %d "
					"(Supported color depth => 16, 24 or 23))\n",
					ili9481bb_var.bits_per_pixel);
		ret = -EINVAL;
		break;
	}

	/* Set the bus config */
	if (ret == 0) {
		struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
		struct lcdbus_ops *bus = ldev->ops;

		ret = bus->set_conf(dev, &busconf);
	}

	return ret;
}

/**
 * ili9481bb_set_gpio_config - Sets the gpio config
 *
 */
static int
ili9481bb_set_gpio_config(struct device *dev)
{
	int ret=0;

	/* EOFI pin is NOT connected !!! */

	/* Return the error code */
	return ret;
}


/**
 * ili9481bb_check_cmd_list_overflow
 *
 */
#define ili9481bb_check_cmd_list_overflow()                                      \
    if (drvdata->curr_cmd > drvdata->last_cmd) {                               \
        printk(KERN_ERR "%s(***********************************************\n",\
                __FUNCTION__);                                                 \
        printk(KERN_ERR "%s(* Too many cmds, please try to:      \n",          \
                __FUNCTION__);                                                 \
        printk(KERN_ERR "%s(* 1- increase LCDBUS_CMDS_LIST_LENGTH (%d) \n",    \
                __FUNCTION__, LCDBUS_CMDS_LIST_LENGTH);                        \
        printk(KERN_ERR "%s(* 2- Split the commands list\n",                   \
                __FUNCTION__);                                                 \
        printk(KERN_ERR "%s(***********************************************\n",\
                __FUNCTION__);                                                 \
        /* Return the error code */                                            \
        return -ENOMEM;                                                        \
    }


/**
 * ili9481bb_free_cmd_list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 *
 * This function removes and free's all lcdbus_cmd's from cmds.
 */
static void
ili9481bb_free_cmd_list(struct device *dev, struct list_head *cmds)
{
	struct lcdbus_cmd *cmd, *tmp;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	list_for_each_entry_safe(cmd, tmp, cmds, link) {
		list_del(&cmd->link);
	}

	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;
}

/**
 * ili9481bb_execute_cmds - execute the internal command chain
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 * @delay: wait duration is msecs
 *
 * This function executes the given commands list by sending it
 * to the lcdbus layer. Afterwards it cleans the given command list.
 * This function waits the given amount of msecs after sending the
 * commands list
 */
static int
ili9481bb_execute_cmds(struct device *dev, struct list_head *cmds, u16 delay)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

	/* Write data to the bus */
	ret = bus->write(dev, cmds);
	ili9481bb_free_cmd_list(dev, cmds);

	/* Need to wait ? */
	if (delay != 0)
		mdelay(delay);

	/* Return the error code */
	return ret;
}

/**
 * ili9481bb_add_ctrl_cmd
 * @dev: device which has been used to call this function
 * @cmds: commandes list
 * @reg: the register that is addressed
 * @reg_size: the register data size
 * @reg_data: the register data value
 *
 * This function adds the given command to the internal command chain
 * of the LCD driver.
 */
static int
ili9481bb_add_ctrl_cmd(struct device *dev,
		struct list_head *cmds,
		u16 reg,
		u16 reg_size,
		u8 *reg_data)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	ili9481bb_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->cmd  = reg;
	drvdata->curr_cmd->len  = reg_size;
	drvdata->curr_cmd->data_phys = drvdata->curr_cmd->data_int_phys;

	if ((reg_data) && (reg_size))
		memcpy(&drvdata->curr_cmd->data_int, reg_data, reg_size);

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* Return the error code */
	return 0;
}

/**
 * ili9481bb_add_data_cmd - add a request to the internal command chain
 * @dev: device which has been used to call this function
 * @cmds: commandes list
 * @data: the data to send
 * @len: the length of the data to send
 *
 * This function adds the given command and its assigned data to the internal
 * command chain of the LCD driver. Note that this function can only be used
 * for write cmds and that the data buffer must retain intact until
 * transfer is finished.
 */
static inline int
ili9481bb_add_data_cmd(struct device *dev,
		struct list_head *cmds,
		struct lcdfb_transfer *transfer)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	ili9481bb_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_DATA;
	drvdata->curr_cmd->cmd  = ILI9481BB_WRITE_MEMORY_START_REG;
	drvdata->curr_cmd->w = transfer->w;
	drvdata->curr_cmd->h = transfer->h;
	drvdata->curr_cmd->bytes_per_pixel = ili9481bb_var.bits_per_pixel >> 3;
	drvdata->curr_cmd->stride = ili9481bb_fix.line_length;
	drvdata->curr_cmd->data_phys = transfer->addr_phys +
			transfer->x * drvdata->curr_cmd->bytes_per_pixel +
			transfer->y * drvdata->curr_cmd->stride;

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* Return the error code */
	return 0;
}

/**
 * ili9481bb_add_transfer_cmd - adds lcdbus_cmd's from a transfer to list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 * @transfer: the lcdfb_transfer to be converted
 *
 * This function creates a command list for a given transfer. Therefore it
 * selects the x- and y-start position in the display ram, issues the write-ram
 * command and sends the data.
 */
static int
ili9481bb_add_transfer_cmd(struct device *dev,
	                     struct list_head *cmds,
	                     struct lcdfb_transfer *transfer)
{
	int ret = 0;
	u16 x_start, x_end, y_start, y_end;

	pr_debug("%s()\n", __FUNCTION__);

	x_start = transfer->x;
	x_end = x_start + transfer->w - 1;

	y_start = transfer->y;
	y_end = y_start + transfer->h - 1;

	/* Configure display window (columns/rows) */
	ili9481bb_set_cols_rows(dev, cmds, x_start, x_end, y_start, y_end);

	/* Prepare Data */
	ret = ili9481bb_add_data_cmd(dev, cmds, transfer);

	/* Return the error code */
	return ret;
}


/**
 * @brief
 *
 * @param dev
 * @param cmds
 * @param x_start
 * @param x_end
 * @param y_start
 * @param y_end
 */
/*
 * FB_ROTATE_UR   0 - normal orientation (0 degree)
 * FB_ROTATE_CW   1 - clockwise orientation (90 degrees)
 * FB_ROTATE_UD   2 - upside down orientation (180 degrees)
 * FB_ROTATE_CCW  3 - counterclockwise orientation (270 degrees)
 *
 **/
static void
ili9481bb_set_cols_rows(struct device *dev,
					struct list_head *cmds,
					u16 x_start, u16 x_end,
					u16 y_start, u16 y_end)
{
	u8 vl_CmdParam[ILI9481BB_REG_SIZE_MAX];

    /* Colomn start and end */
	vl_CmdParam[0] = ILI9481BB_HIBYTE(x_start);
	vl_CmdParam[1] = ILI9481BB_LOBYTE(x_start);
	vl_CmdParam[2] = ILI9481BB_HIBYTE(x_end);
	vl_CmdParam[3] = ILI9481BB_LOBYTE(x_end);

	switch(ili9481bb_var.rotate) {
	case FB_ROTATE_CW:   /* 90  */
	case FB_ROTATE_CCW:  /* 270 */
		ili9481bb_add_ctrl_cmd(dev, cmds,
			ILI9481BB_SET_PAGE_ADDRESS_REG,
			ILI9481BB_SET_PAGE_ADDRESS_SIZE,
			vl_CmdParam);
		break;

	default: /* 0 or 180 */
		ili9481bb_add_ctrl_cmd(dev, cmds,
			ILI9481BB_SET_COLUMN_ADDRESS_REG,
			ILI9481BB_SET_COLUMN_ADDRESS_SIZE,
			vl_CmdParam);
		break;
	}

	/* Row start and end */
	vl_CmdParam[0] = ILI9481BB_HIBYTE(y_start);
	vl_CmdParam[1] = ILI9481BB_LOBYTE(y_start);
	vl_CmdParam[2] = ILI9481BB_HIBYTE(y_end);
	vl_CmdParam[3] = ILI9481BB_LOBYTE(y_end);

	switch(ili9481bb_var.rotate) {
	case FB_ROTATE_CW:   /* 90  */
	case FB_ROTATE_CCW:  /* 270 */
		ili9481bb_add_ctrl_cmd(dev, cmds,
			ILI9481BB_SET_COLUMN_ADDRESS_REG,
			ILI9481BB_SET_COLUMN_ADDRESS_SIZE,
			vl_CmdParam);
		break;

	default: /* 0 or 180 */
		ili9481bb_add_ctrl_cmd(dev, cmds,
			ILI9481BB_SET_PAGE_ADDRESS_REG,
			ILI9481BB_SET_PAGE_ADDRESS_SIZE,
			vl_CmdParam);
		break;
	}
}


/*
 * ili9481bb_bootstrap - execute the bootstrap commands list
 * @dev: device which has been used to call this function
 *
 */
static int
ili9481bb_bootstrap(struct device *dev)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);
	int ret = 0;
	struct list_head cmds;
	u16 x_start, x_end, y_start, y_end;
	u8 vl_CmdParam[ILI9481BB_REG_SIZE_MAX];

	pr_debug("%s()\n", __FUNCTION__);

	/* ------------------------------------------------------------------------
	 * Initialize the LCD HW
	 * --------------------------------------------------------------------- */
	INIT_LIST_HEAD(&cmds);

	/* Soft Reset */
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_SOFT_RESET_REG,
			ILI9481BB_SOFT_RESET_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 20);

	/* Exit Sleep Mode */
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_EXIT_SLEEP_MODE_REG,
			ILI9481BB_EXIT_SLEEP_MODE_SIZE,	vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 250);

	/* Set Address Mode */
	vl_CmdParam[0] = 0x09;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_SET_ADDRESS_MODE_REG,
			ILI9481BB_SET_ADDRESS_MODE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Power Settings */
	vl_CmdParam[0] = 0x07;
	vl_CmdParam[1] = 0x42;
	vl_CmdParam[2] = 0x18;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_POWER_SETTING_REG,
			ILI9481BB_POWER_SETTING_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* VCOM Control */
	vl_CmdParam[0] = 0x00;
	vl_CmdParam[1] = 0x22;
	vl_CmdParam[2] = 0x10;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_VCOM_CONTROL_REG,
			ILI9481BB_VCOM_CONTROL_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Power Setting Normal Mode */
	vl_CmdParam[0] = 0x02;
	vl_CmdParam[1] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_POWER_SETTING_NORMAL_MODE_REG,
			ILI9481BB_POWER_SETTING_NORMAL_MODE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Power Setting Partial Mode */
	vl_CmdParam[0] = 0x01;
	vl_CmdParam[1] = 0x22;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_POWER_SETTING_PARTIAL_MODE_REG,
			ILI9481BB_POWER_SETTING_PARTIAL_MODE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Power Setting Idle Mode */
	vl_CmdParam[0] = 0x01;
	vl_CmdParam[1] = 0x22;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_POWER_SETTING_IDLE_MODE_REG,
			ILI9481BB_POWER_SETTING_IDLE_MODE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Panel Driving Setting */
	vl_CmdParam[0] = 0x10;
	vl_CmdParam[1] = 0x3B;
	vl_CmdParam[2] = 0x00;
	vl_CmdParam[3] = 0x00;
	vl_CmdParam[4] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_PANEL_DRIVING_SETTING_REG,
			ILI9481BB_PANEL_DRIVING_SETTING_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Display Timing Setting Normal Mode */
	vl_CmdParam[0] = 0x10;
	vl_CmdParam[1] = 0x10;
	vl_CmdParam[2] = 0x22;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_DISPLAY_TIMING_NORMAL_MODE_REG,
			ILI9481BB_DISPLAY_TIMING_NORMAL_MODE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Frame Rate & Inversion Control */
	vl_CmdParam[0] = 0x02;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_FRAME_RATE_INVERSION_CONTROL_REG,
			ILI9481BB_FRAME_RATE_INVERSION_CONTROL_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Interface Control */
	vl_CmdParam[0] = 0x02;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_INTERFACE_CONTROL_REG,
			ILI9481BB_INTERFACE_CONTROL_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Gamme Settings */
	vl_CmdParam[0]  = 0x22;
	vl_CmdParam[1]  = 0x27;
	vl_CmdParam[2]  = 0x40;
	vl_CmdParam[3]  = 0x07;
	vl_CmdParam[4]  = 0x00;
	vl_CmdParam[5]  = 0x07;
	vl_CmdParam[6]  = 0x63;
	vl_CmdParam[7]  = 0x06;
	vl_CmdParam[8]  = 0x55;
	vl_CmdParam[9]  = 0x70;
	vl_CmdParam[10] = 0x06;
	vl_CmdParam[11] = 0x02;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_GAMMA_SETTING_REG,
			ILI9481BB_GAMMA_SETTING_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* NV Memory Write */
	vl_CmdParam[0] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_NV_MEMORY_WRITE_REG,
			ILI9481BB_NV_MEMORY_WRITE_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* NV Memory Control */
	vl_CmdParam[0] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_NV_MEMORY_CONTROL_REG,
			ILI9481BB_NV_MEMORY_CONTROL_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* NV Memory Protect */
	vl_CmdParam[0] = 0x00;
	vl_CmdParam[1] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_NV_MEMORY_PROTECT_REG,
			ILI9481BB_NV_MEMORY_PROTECT_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Command Access Protect */
	vl_CmdParam[0] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_COMMAND_ACCESS_PROTECT_REG,
			ILI9481BB_COMMAND_ACCESS_PROTECT_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Frame Memory Access & Interface Settings */
	vl_CmdParam[0] = 0x02;
	vl_CmdParam[1] = 0x00;
	vl_CmdParam[2] = 0x00;
	vl_CmdParam[3] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_FRAME_MEMORY_ACCESS_REG,
			ILI9481BB_FRAME_MEMORY_ACCESS_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Display Mode & Frame Memory Write Mode */
	vl_CmdParam[0] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_DISPLAY_MODE_FRAME_MEMORY_REG,
			ILI9481BB_DISPLAY_MODE_FRAME_MEMORY_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Set Pixel Format */
	vl_CmdParam[0] = 0x55;
	if (drvdata->use_262k_colors) {
		ili9481bb_add_ctrl_cmd(dev, &cmds,
				ILI9481BB_SET_PIXEL_FORMAT_REG,
				ILI9481BB_SET_PIXEL_FORMAT_SIZE, vl_CmdParam);
		ili9481bb_execute_cmds(dev, &cmds, 0);
	}
	else {
		ili9481bb_add_ctrl_cmd(dev, &cmds,
				ILI9481BB_SET_PIXEL_FORMAT_REG,
				ILI9481BB_SET_PIXEL_FORMAT_SIZE, vl_CmdParam);
		ili9481bb_execute_cmds(dev, &cmds, 0);
	}

	/* Tearing ON */
/* FIXME : Needed ?
	vl_CmdParam[0] = 0x00;
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_SET_TEAR_ON_REG,
			ILI9481BB_SET_TEAR_ON_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);
*/

	/* Configure display window (columns/rows) */
	x_start = 0;
	x_end = ili9481bb_var.xres - 1;
	y_start = 0;
	y_end = ili9481bb_var.yres - 1;

	ili9481bb_set_cols_rows(dev, &cmds, x_start, x_end, y_start, y_end);

	/* Display ON */
	ili9481bb_add_ctrl_cmd(dev, &cmds,
			ILI9481BB_SET_DISPLAY_ON_REG,
			ILI9481BB_SET_DISPLAY_ON_SIZE, vl_CmdParam);
	ili9481bb_execute_cmds(dev, &cmds, 0);

	/* Set power mode */
	drvdata->power_mode = FB_BLANK_UNBLANK;

	/* Return the error code */
	return ret;
}

/*
 =========================================================================
 =                                                                       =
 =              device detection and bootstrapping                       =
 =                                                                       =
 =========================================================================
*/

/**
 * ili9481bb_device_supported - perform hardware detection check
 * @dev:	pointer to the device which should be checked for support
 *
 */
static int __devinit
ili9481bb_device_supported(struct device *dev)
{
	int ret = 0;

	/*
	 * Hardware detection of the display does not seem to be supported
	 * by the hardware, thus we assume the display is there!
	 */
	if (strcmp(dev->bus_id, "pnx-vde-lcd0"))
		ret = -ENODEV; // (pnx-vde-lcd0 not detected (pb during VDE init)

	/* Return the error code */
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
 * ili9481bb_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function converts the list of lcdfb_transfer into a list of resulting
 * lcdbus_cmd's which then gets sent to the display controller using the
 * underlying bus driver.
 */
static int
ili9481bb_write(const struct device *dev, const struct list_head *transfers)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;
	struct list_head cmds;
	struct lcdfb_transfer *transfer;
	int ret = 0;

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		dev_warn((struct device *)dev,
				"NOT allowed (refresh while power off)\n");
		return 0;
	}

	if (list_empty(transfers)) {
		dev_warn((struct device *)dev,
				"Got an empty transfer list\n");
		return 0;
	}

	/* now get on with the real stuff */
	INIT_LIST_HEAD(&cmds);

	list_for_each_entry(transfer, transfers, link) {
		ret |= ili9481bb_add_transfer_cmd(dev->parent, &cmds, transfer);
	}

	if (ret >= 0) {
		/* execute the cmd list we build */
		ret = bus->write(dev->parent, &cmds);
	}

	/* Now buffers may be freed */
	ili9481bb_free_cmd_list(dev->parent, &cmds);

	/* Return the error code */
	return ret;
}

/**
 * ili9481bb_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev: device which has been used to call this function
 * @fsi: structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
ili9481bb_get_fscreeninfo(const struct device *dev,
		struct fb_fix_screeninfo *fsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!fsi) {
		return -EINVAL;
	}

	*fsi = ili9481bb_fix;

	/* Return the error code */
	return 0;
}

/**
 * ili9481bb_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev: device which has been used to call this function
 * @vsi: structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
ili9481bb_get_vscreeninfo(const struct device *dev,
		struct fb_var_screeninfo *vsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}
	
	*vsi = ili9481bb_var;

	/* Return the error code */
	return 0;
}


/**
 * ili9481bb_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
ili9481bb_display_on(struct device *dev)
{
	int ret = 0;
	struct device *devp = dev->parent;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(devp);

	/* Switch on the display if needed */
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		struct list_head cmds;

		INIT_LIST_HEAD(&cmds);

		/* Exit Sleep Mode */
		ili9481bb_add_ctrl_cmd(devp, &cmds,
				ILI9481BB_EXIT_SLEEP_MODE_REG,
				ILI9481BB_EXIT_SLEEP_MODE_SIZE, NULL);
		ili9481bb_execute_cmds(devp, &cmds, 250);

		/* Display ON */
		ili9481bb_add_ctrl_cmd(devp, &cmds,
				ILI9481BB_SET_DISPLAY_ON_REG,
				ILI9481BB_SET_DISPLAY_ON_SIZE, NULL);
		ili9481bb_execute_cmds(devp, &cmds, 0);

		/* Set power mode */
		drvdata->power_mode = FB_BLANK_UNBLANK;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_UNBLANK mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	/* Return the error code */
	return ret;
}

/**
 * ili9481bb_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
ili9481bb_display_off(struct device *dev)
{
	int ret = 0;
	struct device *devp = dev->parent;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(devp);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (drvdata->power_mode != FB_BLANK_POWERDOWN) {
		struct list_head cmds;

		INIT_LIST_HEAD(&cmds);

		/* Display OFF */
		ili9481bb_add_ctrl_cmd(devp, &cmds,
				ILI9481BB_SET_DISPLAY_OFF_REG,
				ILI9481BB_SET_DISPLAY_OFF_SIZE, NULL);
		ili9481bb_execute_cmds(devp, &cmds, 45);

		/* Enter Sleep Mode */
		ili9481bb_add_ctrl_cmd(devp, &cmds,
				ILI9481BB_ENTER_SLEEP_MODE_REG,
				ILI9481BB_ENTER_SLEEP_MODE_SIZE, NULL);
		ili9481bb_execute_cmds(devp, &cmds, 250);

		/* Set power mode */
		drvdata->power_mode = FB_BLANK_POWERDOWN;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_POWERDOWN mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	/* Return the error code */
	return ret;
}


/**
 * ili9481bb_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
ili9481bb_display_standby(struct device *dev)
{
	int ret = 0;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_VSYNC_SUSPEND) {

		/* Set power mode */
		drvdata->power_mode = FB_BLANK_VSYNC_SUSPEND;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_VSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	/* Return the error code */
	return  ret;
}


/**
 * ili9481bb_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
ili9481bb_display_deep_standby(struct device *dev)
{
	int ret = 0;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_VSYNC_SUSPEND) {

		/* Set power mode */
		drvdata->power_mode = FB_BLANK_VSYNC_SUSPEND;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_HSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	/* Return the error code */
	return  ret;
}

/**
 * ili9481bb_get_splash_info - copies the splash screen info into si
 * @dev: device which has been used to call this function
 * @si:  structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
ili9481bb_get_splash_info(struct device *dev,
		struct lcdfb_splash_info *si)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!si) {
		return -EINVAL;
	}

	*si = ili9481bb_splash_info;

	/* Return the error code */
	return 0;
}


/**
 * ili9481bb_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
ili9481bb_get_specific_config(struct device *dev,
		struct lcdfb_specific_config **sc)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!sc) {
		return -EINVAL;
	}

	(*sc) = &ili9481bb_specific_config;

	/* Return the error code */
	return 0;
}


/**
 * ili9481bb_get_device_attrs
 * @dev: device which has been used to call this function
 * @device_attrs: device attributes to be returned
 * @attrs_nbr: the number of device attributes
 *
 *
 * Returns the device attributes to be added to the SYSFS
 * entries (/sys/class/graphics/fbX/....
 * */
static int
ili9481bb_get_device_attrs(struct device *dev,
		struct device_attribute **device_attrs,
		unsigned int *attrs_nbr)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!device_attrs) {
		return -EINVAL;
	}

	(*device_attrs) = ili9481bb_device_attrs;

	(*attrs_nbr) = sizeof(ili9481bb_device_attrs)/sizeof(ili9481bb_device_attrs[0]);

	/* Return the error code */
	return 0;
}


/*
 * ili9481bb_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int ili9481bb_check_var(const struct device *dev,
		 struct fb_var_screeninfo *vsi)
{
	int ret = -EINVAL;

	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	/* check xyres */
	if ((vsi->xres != ili9481bb_var.xres) ||
		(vsi->yres != ili9481bb_var.yres)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xyres virtual */
	if ((vsi->xres_virtual != ili9481bb_var.xres_virtual) ||
		(vsi->yres_virtual != ili9481bb_var.yres_virtual)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xoffset */
	if (vsi->xoffset != ili9481bb_var.xoffset) {
		ret = -EPERM;
		goto ko;
	}

	/* check bpp */
	if ((vsi->bits_per_pixel != 16) &&
		(vsi->bits_per_pixel != 24) &&
		(vsi->bits_per_pixel != 32))
		goto ko;

	/* Check pixel format */
	if (vsi->nonstd) {
		/* Non-standard pixel format not supported by LCD HW */
		ret = -EPERM;
		goto ko;
	}

	/* check rotation */
/*
 * FB_ROTATE_UR      0 - normal orientation (0 degree)
 * FB_ROTATE_CW      1 - clockwise orientation (90 degrees)
 * FB_ROTATE_UD      2 - upside down orientation (180 degrees)
 * FB_ROTATE_CCW     3 - counterclockwise orientation (270 degrees)
 *
 * */
	if (vsi->rotate > FB_ROTATE_CCW)
		goto ko;

	/* Everything is ok */
	return 0;

ko:
	/* Return the error code */
	return ret;
}

/*
 * ili9481bb_set_par
 * @dev: device which has been used to call this function
 * @vsi: structure  var screeninfo to set
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static inline void ili9481bb_init_color(struct fb_bitfield *color,
		__u32 offset, __u32 length, __u32 msb_right)
{
	color->offset     = offset;
	color->length     = length;
	color->msb_right  = msb_right;
}

static int ili9481bb_set_par(const struct device *dev,
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

	case FB_ACTIVATE_NXTOPEN:	/* activate on next open */
		break;

	case FB_ACTIVATE_TEST:		/* don't set, round up impossible */
		break;

	case FB_ACTIVATE_MASK: 		/* values */
		break;

	case FB_ACTIVATE_VBL:		/* activate values on next vbl  */
		break;

	case FB_CHANGE_CMAP_VBL:	/* change colormap on vbl */
		break;

	case FB_ACTIVATE_ALL:		/* change all VCs on this fb */
		set_params = 1;
		break;

	case FB_ACTIVATE_FORCE:		/* force apply even when no change*/
		set_params = 1;
		break;

	case FB_ACTIVATE_INV_MODE:	/* invalidate videomode */
		break;
	}

	if (set_params) {
		if (! memcmp(vsi, &ili9481bb_var, sizeof(struct fb_var_screeninfo))) {
			ret = 0;
			goto quit;
		}

		/* Check rotation */
		if (ili9481bb_var.rotate != vsi->rotate) {
			ili9481bb_var.rotate = vsi->rotate;
		}

		/* Check bpp */
		if (ili9481bb_var.bits_per_pixel != vsi->bits_per_pixel) {

			u32 bytes_per_pixel;

			ili9481bb_var.bits_per_pixel = vsi->bits_per_pixel;

			switch(ili9481bb_var.bits_per_pixel) {
			case 16:
				ili9481bb_init_color(&ili9481bb_var.red,  11, 5, 0);
				ili9481bb_init_color(&ili9481bb_var.green, 5, 6, 0);
				ili9481bb_init_color(&ili9481bb_var.blue,  0, 5, 0);
				break;

			case 24:
			case 32:
				ili9481bb_init_color(&ili9481bb_var.red,   16, 8, 0);
				ili9481bb_init_color(&ili9481bb_var.green,  8, 8, 0);
				ili9481bb_init_color(&ili9481bb_var.blue,   0, 8, 0);
				break;
			}

			bytes_per_pixel = ili9481bb_var.bits_per_pixel/8;

			/* Set the new line length (BPP or resolution changed) */
			ili9481bb_fix.line_length = ili9481bb_var.xres_virtual*bytes_per_pixel;

			/* Switch off the LCD HW */
			ili9481bb_display_off((struct device *)dev);

			/* Switch on the LCD HW */
			ret = ili9481bb_display_on((struct device *)dev);
		}

		/* TODO: Check other parameters */

		ret = 0;
	}

quit:
	/* Return the error code */
	return ret;
}


/**
 * ili9481bb_ioctl
 * @dev: device which has been used to call this function
 * @cmd: requested command
 * @arg: command argument
 * */
static int
ili9481bb_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int value;
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	switch (cmd) {

	/* */
	case PNXFB_GET_ZOOM_MODE:
		if (put_user(drvdata->zoom_mode, (int __user *)arg)) {
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
				/* NOT allowed to change zoom mode */
				ret = -EPERM;
			}
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
 * LCD operations supported by this driver
 */
struct lcdfb_ops ili9481bb_ops = {
	.write           = ili9481bb_write,
	.get_fscreeninfo = ili9481bb_get_fscreeninfo,
	.get_vscreeninfo = ili9481bb_get_vscreeninfo,
	.get_splash_info = ili9481bb_get_splash_info,
	.get_specific_config = ili9481bb_get_specific_config,
	.get_device_attrs= ili9481bb_get_device_attrs,
	.display_on      = ili9481bb_display_on,
	.display_off     = ili9481bb_display_off,
	.display_standby = ili9481bb_display_standby,
	.display_deep_standby = ili9481bb_display_deep_standby,
	.check_var       = ili9481bb_check_var,
	.set_par         = ili9481bb_set_par,
	.ioctl           = ili9481bb_ioctl,
};


/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/
static int __devinit
ili9481bb_probe(struct device *dev)
{
	struct ili9481bb_drvdata *drvdata;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	u32 cmds_list_phys;
	struct lcdbus_cmd *cmd;
	int ret, i;

	pr_debug("%s()\n", __FUNCTION__);

	if (!bus || !bus->read || !bus->write || !bus->get_conf ||
	       !bus->set_conf || !bus->set_timing) {
		return -EINVAL;
	}

	/* ------------------------------------------------------------------------
	 * Hardware detection
	 * ---- */
	ret = ili9481bb_device_supported(dev);
	if (ret < 0) {
		goto out;
	}

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		dev_err(dev, "No more memory (drvdata)\n");
		ret = -ENOMEM;
		goto out;
	}

	/* ------------------------------------------------------------------------
	 *  Prepare the lcd cmds list
	 * ---- */
	drvdata->cmds_list_max_size = LCDBUS_CMDS_LIST_LENGTH *
			sizeof(struct lcdbus_cmd);

	drvdata->cmds_list = (struct lcdbus_cmd *)dma_alloc_coherent(NULL,
			drvdata->cmds_list_max_size, &(drvdata->cmds_list_phys),
			GFP_KERNEL | GFP_DMA);

	if ((!drvdata->cmds_list) || (!drvdata->cmds_list_phys)) {
		printk(KERN_ERR "%s Failed ! (No more memory (drvdata->cmds_list))\n",
				__FUNCTION__);
		ret = -ENOMEM;
		goto err_free_drvdata;
	}

	/* Calculates the physical address */
	cmd = drvdata->cmds_list;
	cmds_list_phys = drvdata->cmds_list_phys;
	i = 0;
	do {
		cmd->data_int_phys = cmds_list_phys;
		i++;
		cmd++;
		cmds_list_phys += sizeof(struct lcdbus_cmd);
	} while (i < LCDBUS_CMDS_LIST_LENGTH);

	/* Set the first & last command pointer */
	drvdata->curr_cmd = drvdata->cmds_list;
	drvdata->last_cmd = cmd--;

	pr_debug("%s (cmd %p, curr %p, last %p)\n",
			__FUNCTION__, drvdata->cmds_list, drvdata->curr_cmd,
			drvdata->last_cmd);

	/* Set drvdata */
	dev_set_drvdata(dev, drvdata);

	drvdata->power_mode = ili9481bb_specific_config.boot_power_mode;
	drvdata->fb.ops = &ili9481bb_ops;
	drvdata->fb.dev.parent = dev;
	snprintf(drvdata->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", dev->bus_id);

	/* ------------------------------------------------------------------------
	 * GPIO configuration is specific for each LCD
	 * (Chech the HW datasheet (VDE_EOFI, VDE_EOFI_copy)
	 * ---- */
	ret = ili9481bb_set_gpio_config(dev);
	if (ret < 0) {
		printk(KERN_ERR "%s Failed ! (Could not set gpio config)\n",
				__FUNCTION__);
		ret = -EBUSY;
		goto err_free_cmds_list;
	}

	/* ------------------------------------------------------------------------
	 * Set the bus timings for this display device
	 * ---- */
	bus->set_timing(dev, &ili9481bb_timing);

	/* ------------------------------------------------------------------------
	 * Set the bus configuration for this display device
	 * ---- */
	ret = ili9481bb_set_bus_config(dev);
	if (ret < 0) {
		printk(KERN_ERR "%s Failed ! (Could not set bus config)\n",
				__FUNCTION__);
		ret = -EBUSY;
		goto err_free_cmds_list;
	}

	/* -------------------------------------------------------------------------
	 * Initialize the LCD if the initial state is ON (FB_BLANK_UNBLANK)
	 * ---- */
	if (drvdata->power_mode == FB_BLANK_UNBLANK) {
		ret = ili9481bb_bootstrap(dev);
		if (ret < 0) {
			ret = -EBUSY;
			goto err_free_cmds_list;
		}
	}

	/* ------------------------------------------------------------------------
	 * Register the lcdfb device
	 * ---- */
	ret = lcdfb_device_register(&drvdata->fb);
	if (ret < 0) {
		goto err_free_cmds_list;
	}

	/* Everything is OK, so return */
	goto out;

err_free_cmds_list:

	dma_free_coherent(NULL, drvdata->cmds_list_max_size, drvdata->cmds_list,
			(dma_addr_t)drvdata->cmds_list_phys);

err_free_drvdata:
	kfree(drvdata);

out:
	return ret;
}

static int __devexit
ili9481bb_remove(struct device *dev)
{
	struct ili9481bb_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	lcdfb_device_unregister(&drvdata->fb);

	if (drvdata->cmds_list) {
		dma_free_coherent(NULL,
				drvdata->cmds_list_max_size,
				drvdata->cmds_list,
				(dma_addr_t)drvdata->cmds_list_phys);
	}

	kfree(drvdata);

	return 0;
}

static struct device_driver ili9481bb_driver = {
	.owner  = THIS_MODULE,
	.name   = ILI9481BB_NAME,
	.bus    = &lcdctrl_bustype,
	.probe  = ili9481bb_probe,
	.remove = ili9481bb_remove,
};

static int __init
ili9481bb_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	return driver_register(&ili9481bb_driver);
}

static void __exit
ili9481bb_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	driver_unregister(&ili9481bb_driver);
}

module_init(ili9481bb_init);
module_exit(ili9481bb_exit);

MODULE_AUTHOR("Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("LCDBUS driver for the ili9481bb LCD");
MODULE_LICENSE("GPL");
