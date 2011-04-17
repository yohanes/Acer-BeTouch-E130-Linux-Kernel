/*
 * linux/drivers/video/pnx/displays/jbt6k71-as-a.c
 *
 * jbt6k71-as-a QVGA LCD driver
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

#include "jbt6k71-as-a.h"

/*
 * TODO update rotation for -90 and +90 degrees
 * TODO x panning has not been tested (but problably less important
 *      than y panning)
 *
 * TODO Manage all ACTIVATE mode in jbt6k71_set_par
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
static struct lcdfb_specific_config jbt6k71_specific_config = {
	.explicit_refresh = 1,
	.boot_power_mode = FB_BLANK_UNBLANK,
};


/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_JBT6K71_KERNEL_SPLASH_SCREEN
#include "jbt6k71_splash.h"
static struct lcdfb_splash_info jbt6k71_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = jbt6k71_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(jbt6k71_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info jbt6k71_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif

/* FB_FIX_SCREENINFO (see fb.h) */
static struct fb_fix_screeninfo jbt6k71_fix = {
	.id          = JBT6K71_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (JBT6K71_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = JBT6K71_SCREEN_WIDTH * (JBT6K71_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo jbt6k71_var = {
	.xres           = JBT6K71_SCREEN_WIDTH,
	.yres           = JBT6K71_SCREEN_HEIGHT,
	.xres_virtual   = JBT6K71_SCREEN_WIDTH,
	.yres_virtual   = JBT6K71_SCREEN_HEIGHT * (JBT6K71_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = JBT6K71_FB_BPP,

#if (JBT6K71_FB_BPP == 32)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (JBT6K71_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (JBT6K71_FB_BPP == 16)
	.red            = {11, 5, 0},
	.green          = {5, 6, 0},
	.blue           = {0, 5, 0},

#else
	#error "Unsupported color depth (see driver doc)"
#endif

	.vmode          = FB_VMODE_NONINTERLACED,
	.height         = 44,
	.width          = 33,
	.rotate         = FB_ROTATE_UR,
};


/* Hw LCD timings */
static struct lcdbus_timing jbt6k71_timing = {
	/* bus */
	.mode = 1, /* 8 bits // interface */
	.ser  = 0, /* serial mode is not used */
	.hold = 0, /* VDE_CONF_HOLD_A0_FOR_COMMAND */
	/* read */
	.rh = 7,
	.rc = 4,
	.rs = 0,
	/* write */
	.wh = 1,
	.wc = 2,
	.ws = 1,
	/* misc */
	.ch = 0,
	.cs = 0,
};

/* ----------------------------------------------------------------------- */

/* cached register values of the JBT6K71 display */
static u16 jbt6k71_register_values[JBT6K71_REGISTERMAP_SIZE];

/* JBT6K71 driver data */
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
struct jbt6k71_drvdata {

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
/* jbt6k71_show_explicit_refresh
 *
 */
static ssize_t
jbt6k71_show_explicit_refresh(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", jbt6k71_specific_config.explicit_refresh);
}


/* jbt6k71_store_explicit_refresh
 *
 */
static ssize_t
jbt6k71_store_explicit_refresh(struct device *device,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	if (strncasecmp(buf, "0", count - 1) == 0) {
		jbt6k71_specific_config.explicit_refresh = 0;
	}
	else if (strncasecmp(buf, "1", count - 1) == 0) {
		jbt6k71_specific_config.explicit_refresh = 1;
	}

	return count;
}


static struct device_attribute jbt6k71_device_attrs[] = {
	__ATTR(explicit_refresh, S_IRUGO|S_IWUSR, jbt6k71_show_explicit_refresh,
			jbt6k71_store_explicit_refresh)
};

/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/


/**
 * jbt6k71_set_bus_config - Sets the bus (VDE) config
 *
 * Configure the VDE colors format according to the LCD
 * colors formats (conversion)
 */
static int
jbt6k71_set_bus_config(struct device *dev)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);
	struct lcdbus_conf busconf;
	int ret = 0;

	/* Tearing management */
	busconf.eofi_del  = 0;
	busconf.eofi_skip = 0;
	busconf.eofi_pol  = 0;
	busconf.eofi_use_vsync = 1;

	/* CSKIP & BSWAP params */
	busconf.cskip = 0;
	busconf.bswap = 0;

	/* Align paremeter */
	busconf.align = 0;

	/* Data & cmd params */
	busconf.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP;
	busconf.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS;

	switch (jbt6k71_var.bits_per_pixel) {

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
					jbt6k71_var.bits_per_pixel);
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
 * jbt6k71_set_gpio_config - Sets the gpio config
 *
 */
static int
jbt6k71_set_gpio_config(struct device *dev)
{
	int ret=0;

	/* EOFI pin is connected to the GPIOB7 */
	gpio_request(GPIO_B7, (char*)dev->init_name);
	pnx_gpio_set_mode(GPIO_B7, GPIO_MODE_MUX0);
	gpio_direction_input(GPIO_B7);
	// TODO: Check the Pull Up/Down mode

	/* Return the error code */
	return ret;
}


/**
 * jbt6k71_check_cmd_list_overflow
 *
 */
#define jbt6k71_check_cmd_list_overflow()                                      \
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
 * jbt6k71_free_cmd_list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 *
 * This function removes and free's all lcdbus_cmd's from cmds.
 */
static void
jbt6k71_free_cmd_list(struct device *dev, struct list_head *cmds)
{
	struct lcdbus_cmd *cmd, *tmp;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	list_for_each_entry_safe(cmd, tmp, cmds, link) {
		list_del(&cmd->link);
	}

	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;
}

/**
 * jbt6k71_execute_cmds - execute the internal command chain
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
jbt6k71_execute_cmds(struct device *dev, struct list_head *cmds, u16 delay)
{
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	int ret;

	pr_debug("%s()\n", __FUNCTION__);

	/* Write data to the bus */
	ret = bus->write(dev, cmds);
	jbt6k71_free_cmd_list(dev, cmds);

	/* Need to wait ? */
	if (delay != 0)
		mdelay(delay);

	/* Return the error code */
	return ret;
}

/**
 * jbt6k71_add_ctrl_cmd
 * @dev: device which has been used to call this function
 * @cmds: commandes list
 * @reg: the register that is addressed
 * @value: the value that should be copied into the register
 *
 * This function adds the given command to the internal command chain
 * of the LCD driver.
 */
static int
jbt6k71_add_ctrl_cmd(struct device *dev,
		struct list_head *cmds,
		const u16 reg, u16 value)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	/* upper 8 bits of command */
	jbt6k71_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->cmd  = HIBYTE(reg);
	drvdata->curr_cmd->data_phys   = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->data_int[0] = 0;
	drvdata->curr_cmd->data_int[1] = 0;
	drvdata->curr_cmd->len  = 0;

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* lower 8 bits of command */
	jbt6k71_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->cmd  = LOBYTE(reg);
	drvdata->curr_cmd->data_phys   = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->data_int[0] = HIBYTE(value);
	drvdata->curr_cmd->data_int[1] = LOBYTE(value);
	/* FIXME HACK as reg 0x0 seems to have no args when using it for sync */
	drvdata->curr_cmd->len = ((reg == 0x0000) && (value == 0x0000) ? 0 : 2);

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* Save register value */
	jbt6k71_register_values[reg] = value;

	/* Return the error code */
	return 0;
}

/**
 * jbt6k71_add_data_cmd - add a request to the internal command chain
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
jbt6k71_add_data_cmd(struct device *dev,
		struct list_head *cmds,
		struct lcdfb_transfer *transfer)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	/* upper 8 bits of command */
	jbt6k71_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->cmd  = HIBYTE(JBT6K71_REG(DATA, RAMWR));
	drvdata->curr_cmd->data_phys   = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->data_int[0] = 0;
	drvdata->curr_cmd->data_int[1] = 0;
	drvdata->curr_cmd->len  = 0;

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* Prepare the lower 8 bits of command */
	jbt6k71_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_DATA;
	drvdata->curr_cmd->cmd  = LOBYTE(JBT6K71_REG(DATA, RAMWR));
	drvdata->curr_cmd->w = transfer->w;
	drvdata->curr_cmd->h = transfer->h;
	drvdata->curr_cmd->bytes_per_pixel = jbt6k71_var.bits_per_pixel >> 3;
	drvdata->curr_cmd->stride = jbt6k71_fix.line_length;
	drvdata->curr_cmd->data_phys = transfer->addr_phys +
			transfer->x * drvdata->curr_cmd->bytes_per_pixel +
			transfer->y * drvdata->curr_cmd->stride;

	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */

	/* Return the error code */
	return 0;
}

/**
 * jbt6k71_add_transfer_cmd - adds lcdbus_cmd's from a transfer to list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 * @transfer: the lcdfb_transfer to be converted
 *
 * This function creates a command list for a given transfer. Therefore it
 * selects the x- and y-start position in the display ram, issues the write-ram
 * command and sends the data.
 */
static int
jbt6k71_add_transfer_cmd(struct device *dev,
	                     struct list_head *cmds,
	                     struct lcdfb_transfer *transfer)
{
	int ret = 0;
	const u16 *cached_reg = &jbt6k71_register_values[JBT6K71_REG(DISPLAY,
			ENTRY_MODE)];
	u16 rotation, x_start, x_end, y_start, y_end;

	pr_debug("%s()\n", __FUNCTION__);

	x_start = transfer->x;
	x_end = x_start + transfer->w - 1;

	y_start = transfer->y;
	y_end = y_start + transfer->h - 1;

	/* LCD (0, 0) is top right so adjust x_start and x_end */
	x_start = jbt6k71_var.xres - 1 - x_start;
	x_end = jbt6k71_var.xres - 1 - x_end;

	/* set window x start & end */
	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(WINDOW, H_RAM_ADDR_LOC_1), x_end);
 	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(WINDOW, H_RAM_ADDR_LOC_2), x_start);

	/* set window y start & end */
	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(WINDOW, V_RAM_ADDR_LOC_1), y_start);
	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(WINDOW, V_RAM_ADDR_LOC_2), y_end);


	/* set data x & y ram addr */
	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(DATA, RAM_ADDR_SET_1), x_start);
	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(DATA, RAM_ADDR_SET_2), y_start);

	/* enable highspeed ram-write and rotation */
/*
 * FB_ROTATE_UR      0 - normal orientation (0 degree)
 * FB_ROTATE_CW      1 - clockwise orientation (90 degrees)
 * FB_ROTATE_UD      2 - upside down orientation (180 degrees)
 * FB_ROTATE_CCW     3 - counterclockwise orientation (270 degrees)
 *
 * */
	/* ID1/ID0/AM are important in entry mode reg. See lcd datasheet
	 * for more information */
	switch(jbt6k71_var.rotate) {
	case FB_ROTATE_UR:  rotation = 0x0020; break;
	case FB_ROTATE_CW:  rotation = 0x0000; break;
	case FB_ROTATE_UD:  rotation = 0x0010; break;
	case FB_ROTATE_CCW: rotation = 0x0030; break;
	default: rotation = 0x0020;
	}

	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(DISPLAY, ENTRY_MODE),
			(*cached_reg & ~0x0038) | 0x0100 | rotation);

	/* disable highspeed ram-write */
/*	jbt6k71_add_ctrl_cmd(dev, cmds, JBT6K71_REG(DISPLAY, ENTRY_MODE),
			*cached_reg & ~0x0100);*/

	/* Prepare Data */
	jbt6k71_add_data_cmd(dev, cmds, transfer);

	/* Return the error code */
	return ret;
}

/*
 * jbt6k71_bootstrap - execute the bootstrap commands list
 * @dev: device which has been used to call this function
 *
 */
static int
jbt6k71_bootstrap(struct device *dev)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);
	int ret = 0;
	struct list_head cmds;

	pr_debug("%s()\n", __FUNCTION__);

	/* ------------------------------------------------------------------------
	 * Initialize the LCD HW
	 * --------------------------------------------------------------------- */
	INIT_LIST_HEAD(&cmds);

	/* exit deep standby mode -> standby mode */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET), 0x0000);
	jbt6k71_execute_cmds(dev, &cmds, 1);

	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET), 0x0000);
	jbt6k71_execute_cmds(dev, &cmds, 1);

	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET), 0x0000);
	jbt6k71_execute_cmds(dev, &cmds, 1);

	/* exit standy mode -> sleep mode */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(OSD, NOP),          0x0000);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET),  0x0000);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET),  0x0000);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, MODE_SET), 0x0005);
	jbt6k71_execute_cmds(dev, &cmds, 1);

	/* switch internal oscillator on */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, OSC_SET), 0x0001);
	jbt6k71_execute_cmds(dev, &cmds, 10);

	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, DRV_OUT_CTRL_SET), 0x0027);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, DRV_SIG_SET), 0x0200);

	/* set to 65k colours, reversed data */
	if (drvdata->use_262k_colors) {
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, ENTRY_MODE), 0xE120);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, MODE_1), 0x0004);
	}
	else {
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, ENTRY_MODE), 0x0020);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, MODE_1), 0x4004);
	}

	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, FR_FREQ_SET), 0x0011);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, LTPS_CTRL_1), 0x0303);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, LTPS_CTRL_2), 0x0102);

#if 0 // Avoid the screen blink at the boot

		/* internal amplifier set to optimum */
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(DISPLAY, AMP_CAB_SET), 0x0000);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, SUPPLY_CTRL_1), 0x00F6);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, SUPPLY_CTRL_2), 0x0007);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, SUPPLY_CTRL_4), 0x0111);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, SETTING_1), 0x0200);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, SETTING_2), 0x0002);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, SETTING_3), 0x0000);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, SETTING_4), 0x0300);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, SETTING_5), 0x0700);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(GRAYSCALE, BLUE_OFF_SET),		0x0070);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(WINDOW, FIRST_SCR_DRV_POS_1),	0x0000);
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(WINDOW, FIRST_SCR_DRV_POS_2),	0x013F);
		jbt6k71_execute_cmds(dev, &cmds, 0);
#endif

#ifdef CONFIG_FB_LCDBUS_JBT6K71_OSD
	/* enable OSD */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(OSD, FEATURE),			0x0001);

	/* set OSD0 y-start address */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(OSD, SCR_1_START_ADDR),	0x0000);

	/* set OSD1 y-start address */
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(OSD, SCR_2_START_ADDR), 304);
	jbt6k71_execute_cmds(dev, &cmds, 0);
#endif

#if 0 // Avoid the screen blink at the boot
		jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL),		0x0C10);
		jbt6k71_execute_cmds(dev, &cmds, 30);
#endif

	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, AUTO_MGMT_CTRL),		0x0001);
	jbt6k71_add_ctrl_cmd(dev, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL),		0xF7FE);
	jbt6k71_execute_cmds(dev, &cmds, 110);

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
 * jbt6k71_device_supported - perform hardware detection check
 * @dev:	pointer to the device which should be checked for support
 *
 */
static int __devinit
jbt6k71_device_supported(struct device *dev)
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
 * jbt6k71_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function converts the list of lcdfb_transfer into a list of resulting
 * lcdbus_cmd's which then gets sent to the display controller using the
 * underlying bus driver.
 */
static int
jbt6k71_write(const struct device *dev, const struct list_head *transfers)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev->parent);
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
		ret |= jbt6k71_add_transfer_cmd(dev->parent, &cmds, transfer);
	}

	if (ret >= 0) {
		/* execute the cmd list we build */
		ret = bus->write(dev->parent, &cmds);
	}

	/* Now buffers may be freed */
	jbt6k71_free_cmd_list(dev->parent, &cmds);

	/* Return the error code */
	return ret;
}

/**
 * jbt6k71_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev: device which has been used to call this function
 * @fsi: structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
jbt6k71_get_fscreeninfo(const struct device *dev,
		struct fb_fix_screeninfo *fsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!fsi) {
		return -EINVAL;
	}

	*fsi = jbt6k71_fix;

	/* Return the error code */
	return 0;
}

/**
 * jbt6k71_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev: device which has been used to call this function
 * @vsi: structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
jbt6k71_get_vscreeninfo(const struct device *dev,
		struct fb_var_screeninfo *vsi)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	*vsi = jbt6k71_var;

	/* Return the error code */
	return 0;
}


/**
 * jbt6k71_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
jbt6k71_display_on(struct device *dev)
{
	int ret = 0;
	struct device *devp = dev->parent;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(devp);

	/* Switch on the display if needed */
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		struct list_head cmds;

		INIT_LIST_HEAD(&cmds);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL),	0x0C10);
		jbt6k71_execute_cmds(devp, &cmds, 30);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, AUTO_MGMT_CTRL), 0x0001);
		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL),	 0xF7FE);
		jbt6k71_execute_cmds(devp, &cmds, 110);

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
 * jbt6k71_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
jbt6k71_display_off(struct device *dev)
{
	int ret = 0;
	struct device *devp = dev->parent;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(devp);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (drvdata->power_mode != FB_BLANK_POWERDOWN) {
		struct list_head cmds;

		INIT_LIST_HEAD(&cmds);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(DISPLAY, POWER_OFF_CNT_SET), 0x000A);
		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER,   DISPLAY_CTRL),      0xF7FE);
		jbt6k71_execute_cmds(devp, &cmds, 50);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL), 0xF012);
		jbt6k71_execute_cmds(devp, &cmds, 50);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL), 0xE011);
		jbt6k71_execute_cmds(devp, &cmds, 50);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL), 0xC011);
		jbt6k71_execute_cmds(devp, &cmds, 50);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL), 0x4011);
		jbt6k71_execute_cmds(devp, &cmds, 25);

		jbt6k71_add_ctrl_cmd(devp, &cmds, JBT6K71_REG(POWER, DISPLAY_CTRL), 0x0010);
		jbt6k71_execute_cmds(devp, &cmds, 50);

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
 * jbt6k71_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
jbt6k71_display_standby(struct device *dev)
{
	int ret = 0;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev->parent);

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
 * jbt6k71_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
jbt6k71_display_deep_standby(struct device *dev)
{
	int ret = 0;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev->parent);

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
 * jbt6k71_get_splash_info - copies the splash screen info into si
 * @dev: device which has been used to call this function
 * @si:  structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
jbt6k71_get_splash_info(struct device *dev,
		struct lcdfb_splash_info *si)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!si) {
		return -EINVAL;
	}

	*si = jbt6k71_splash_info;

	/* Return the error code */
	return 0;
}


/**
 * jbt6k71_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
jbt6k71_get_specific_config(struct device *dev,
		struct lcdfb_specific_config **sc)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!sc) {
		return -EINVAL;
	}

	(*sc) = &jbt6k71_specific_config;

	/* Return the error code */
	return 0;
}


/**
 * jbt6k71_get_device_attrs
 * @dev: device which has been used to call this function
 * @device_attrs: device attributes to be returned
 * @attrs_nbr: the number of device attributes
 *
 *
 * Returns the device attributes to be added to the SYSFS
 * entries (/sys/class/graphics/fbX/....
 * */
static int
jbt6k71_get_device_attrs(struct device *dev,
		struct device_attribute **device_attrs,
		unsigned int *attrs_nbr)
{
	pr_debug("%s()\n", __FUNCTION__);

	if (!device_attrs) {
		return -EINVAL;
	}

	(*device_attrs) = jbt6k71_device_attrs;

	(*attrs_nbr) = sizeof(jbt6k71_device_attrs)/sizeof(jbt6k71_device_attrs[0]);

	/* Return the error code */
	return 0;
}


/*
 * jbt6k71_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int jbt6k71_check_var(const struct device *dev,
		 struct fb_var_screeninfo *vsi)
{
	int ret = -EINVAL;

	pr_debug("%s()\n", __FUNCTION__);

	if (!vsi) {
		return -EINVAL;
	}

	/* check xyres */
	if ((vsi->xres != jbt6k71_var.xres) ||
		(vsi->yres != jbt6k71_var.yres)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xyres virtual */
	if ((vsi->xres_virtual != jbt6k71_var.xres_virtual) ||
		(vsi->yres_virtual != jbt6k71_var.yres_virtual)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xoffset */
	if (vsi->xoffset != jbt6k71_var.xoffset) {
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
 * jbt6k71_set_par
 * @dev: device which has been used to call this function
 * @vsi: structure  var screeninfo to set
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static inline void jbt6k71_init_color(struct fb_bitfield *color,
		__u32 offset, __u32 length, __u32 msb_right)
{
	color->offset     = offset;
	color->length     = length;
	color->msb_right  = msb_right;
}

static int jbt6k71_set_par(const struct device *dev,
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
		if (! memcmp(vsi, &jbt6k71_var, sizeof(struct fb_var_screeninfo))) {
			ret = 0;
			goto quit;
		}

		/* Check rotation */
		if (jbt6k71_var.rotate != vsi->rotate) {
			jbt6k71_var.rotate = vsi->rotate;
		}

		/* Check bpp */
		if (jbt6k71_var.bits_per_pixel != vsi->bits_per_pixel) {

			u32 bytes_per_pixel;

			jbt6k71_var.bits_per_pixel = vsi->bits_per_pixel;

			switch(jbt6k71_var.bits_per_pixel) {
			case 16:
				jbt6k71_init_color(&jbt6k71_var.red,  11, 5, 0);
				jbt6k71_init_color(&jbt6k71_var.green, 5, 6, 0);
				jbt6k71_init_color(&jbt6k71_var.blue,  0, 5, 0);
				break;

			case 24:
			case 32:
				jbt6k71_init_color(&jbt6k71_var.red,   16, 8, 0);
				jbt6k71_init_color(&jbt6k71_var.green,  8, 8, 0);
				jbt6k71_init_color(&jbt6k71_var.blue,   0, 8, 0);
				break;
			}

			bytes_per_pixel = jbt6k71_var.bits_per_pixel/8;

			/* Set the new line length (BPP or resolution changed) */
			jbt6k71_fix.line_length = jbt6k71_var.xres_virtual*bytes_per_pixel;

			/* Switch off the LCD HW */
			jbt6k71_display_off((struct device *)dev);

			/* Switch on the LCD HW */
			ret = jbt6k71_display_on((struct device *)dev);
		}

		/* TODO: Check other parameters */

		ret = 0;
	}

quit:
	/* Return the error code */
	return ret;
}


/**
 * jbt6k71_ioctl
 * @dev: device which has been used to call this function
 * @cmd: requested command
 * @arg: command argument
 * */
static int
jbt6k71_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int value;
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev->parent);

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
struct lcdfb_ops jbt6k71_ops = {
	.write           = jbt6k71_write,
	.get_fscreeninfo = jbt6k71_get_fscreeninfo,
	.get_vscreeninfo = jbt6k71_get_vscreeninfo,
	.get_splash_info = jbt6k71_get_splash_info,
	.get_specific_config = jbt6k71_get_specific_config,
	.get_device_attrs= jbt6k71_get_device_attrs,
	.display_on      = jbt6k71_display_on,
	.display_off     = jbt6k71_display_off,
	.display_standby = jbt6k71_display_standby,
	.display_deep_standby = jbt6k71_display_deep_standby,
	.check_var       = jbt6k71_check_var,
	.set_par         = jbt6k71_set_par,
	.ioctl           = jbt6k71_ioctl,
};


/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/
static int __devinit
jbt6k71_probe(struct device *dev)
{
	struct jbt6k71_drvdata *drvdata;
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
	ret = jbt6k71_device_supported(dev);
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

	drvdata->power_mode = jbt6k71_specific_config.boot_power_mode;
	drvdata->fb.ops = &jbt6k71_ops;
	drvdata->fb.dev.parent = dev;
	snprintf(drvdata->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", dev->bus_id);

	/* ------------------------------------------------------------------------
	 * GPIO configuration is specific for each LCD
	 * (Chech the HW datasheet (VDE_EOFI, VDE_EOFI_copy)
	 * ---- */
	ret = jbt6k71_set_gpio_config(dev);
	if (ret < 0) {
		printk(KERN_ERR "%s Failed ! (Could not set gpio config)\n",
				__FUNCTION__);
		ret = -EBUSY;
		goto err_free_cmds_list;
	}

	/* ------------------------------------------------------------------------
	 * Set the bus timings for this display device
	 * ---- */
	bus->set_timing(dev, &jbt6k71_timing);

	/* ------------------------------------------------------------------------
	 * Set the bus configuration for this display device
	 * ---- */
	ret = jbt6k71_set_bus_config(dev);
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
		ret = jbt6k71_bootstrap(dev);
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
jbt6k71_remove(struct device *dev)
{
	struct jbt6k71_drvdata *drvdata = dev_get_drvdata(dev);

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

static struct device_driver jbt6k71_driver = {
	.owner  = THIS_MODULE,
	.name   = JBT6K71_NAME,
	.bus    = &lcdctrl_bustype,
	.probe  = jbt6k71_probe,
	.remove = jbt6k71_remove,
};

static int __init
jbt6k71_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	return driver_register(&jbt6k71_driver);
}

static void __exit
jbt6k71_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	driver_unregister(&jbt6k71_driver);
}

module_init(jbt6k71_init);
module_exit(jbt6k71_exit);

MODULE_AUTHOR("Philippe CORNU, Faouaz TENOUTIT, ST-Ericsson");
MODULE_DESCRIPTION("LCDBUS driver for the jbt6k71 LCD");
MODULE_LICENSE("GPL");
