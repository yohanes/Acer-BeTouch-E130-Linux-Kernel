/*
 *  linux/drivers/video/pnx/displays/hx8357.c
 *
 *  Copyright (C) 2006 Philips Semiconductors Germany GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <mach/hwmem_api.h>
#include <asm/uaccess.h>

#include "hx8357.h"
#define ACER_L1_K3
/*
 * NOTE	the LCD IC is associated with VDE LCD0, means /dev/fb0
 *			(see hx8357_device_supported() function)
 * TODO undapte rotation for -90 and +90 degrees
 * TODO x panning has not been tested (but problably less important
 *      than y panning)
 *
 * TODO Manage all ACTIVATE mode in hx8357_set_par
 *
 * */

/* Activate the following flag to use HwMem allocator instead of
 * dma allocator */
//#define HWMEM_ALLOC


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
static struct lcdfb_specific_config hx8357_specific_config = {
	.explicit_refresh = 1,
	.boot_power_mode = FB_BLANK_UNBLANK,
};
/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_HX8357_KERNEL_SPLASH_SCREEN
#include "hx8357_splash.h"
static struct lcdfb_splash_info hx8357_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = hx8357_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(hx8357_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info hx8357_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif

/* FB_FIX_SCREENINFO (see fb.h) */
static struct fb_fix_screeninfo hx8357_fix = {
	.id          = HX8357_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (HX8357_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = HX8357_SCREEN_WIDTH * (HX8357_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo hx8357_var = {
	.xres           = HX8357_SCREEN_WIDTH,
	.yres           = HX8357_SCREEN_HEIGHT,
	.xres_virtual   = HX8357_SCREEN_WIDTH,
	.yres_virtual   = HX8357_SCREEN_HEIGHT * (HX8357_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = HX8357_FB_BPP,

#if (HX8357_FB_BPP == 32)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (HX8357_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (HX8357_FB_BPP == 16)
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
static struct lcdbus_timing hx8357_timing = {
	/* bus */
	.mode = 2, /* VDE_CONF_MODE_9_BIT_PARALLEL_INTERFACE */
	.ser  = 0, /* serial mode is not used*/
	.hold = 0, /* VDE_CONF_HOLD_A0_FOR_COMMAND */
	/* read */
	.rh = 7,
	.rc = 7, /* AU3.B-1319 modified by Selwyn 2010.1.18 */
	.rs = 3, /* AU3.B-1319 modified by Selwyn 2010.1.18 */
	/* write */
	.wh = 2, /* AU2.FC-262 modified by Wesley 2009.10.01 */
	.wc = 2, 
	.ws = 2,
	/* misc */
	.ch = 0,
	.cs = 0,
};
typedef enum lcd_device_id {
	HX8357,
	HX8368,
	HX_NONE,
}lcd_device_id_t;
/* ----------------------------------------------------------------------- */

/* hx8357 driver data */
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
 * @byte_shift : 
 * @zoom_mode  : 
 * @use_262k_colors : 
 */
struct hx8357_drvdata {
	struct lcdfb_device fb;
	struct lcdbus_cmd *cmds_list;
	u32    cmds_list_phys;
	u32    cmds_list_max_size;
	struct lcdbus_cmd *curr_cmd;
	struct lcdbus_cmd *last_cmd;
	struct mutex lock;/*AU2.FC-181,modified by wesley 2009.09.08*/
	u16 power_mode;
	u16 byte_shift;
	u16 zoom_mode;
	u16 use_262k_colors;
};
static u8 cmd_data[32];
static u8 gram_cmd;
static lcd_device_id_t device_id = HX8357;
//Selwyn 2009-03-26 modified
static u8 standby_mode = 0;
//~Selwyn modified
/*
 =========================================================================
 =                                                                       =
 =          SYSFS  section                                               =
 =                                                                       =
 =========================================================================
*/
/* hx8357_show_explicit_refresh
 *
 */ 
static ssize_t 
hx8357_show_explicit_refresh(struct device *device, 
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", hx8357_specific_config.explicit_refresh);
}


/* hx8357_store_explicit_refresh
 *
 */ 
static ssize_t 
hx8357_store_explicit_refresh(struct device *device,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	if (strncasecmp(buf, "0", count - 1) == 0) {
		hx8357_specific_config.explicit_refresh = 0;
	}
	else if (strncasecmp(buf, "1", count - 1) == 0) {
		hx8357_specific_config.explicit_refresh = 1;
	}

	return count;
}


static struct device_attribute hx8357_device_attrs[] = {
	__ATTR(explicit_refresh, S_IRUGO|S_IWUSR, hx8357_show_explicit_refresh, hx8357_store_explicit_refresh)
};

/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/


/**
 * hx8357_set_bus_config - Sets the bus (VDE) config
 *
 * Configure the VDE colors format according to the LCD
 * colors formats (conversion)
 */
static int 
hx8357_set_bus_config(struct device *dev)
{
  struct hx8357_drvdata *drvdata= dev_get_drvdata(dev);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct lcdbus_conf busconf;
	int ret = 0;

	/* Tearing management */
	busconf.eofi_del  = 65000;
	busconf.eofi_skip = 0;
	busconf.eofi_pol  = 1;

	/* CSKIP & BSWAP params */
	busconf.cskip = 0;
	busconf.bswap = 0;

	/* Data & cmd params */
	busconf.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP;
	busconf.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS;

	switch (hx8357_var.bits_per_pixel) {

	// Case 1: 16BPP
	case 16:
		busconf.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB565;
		busconf.data_ofmt = LCDBUS_OUTPUT_DATAFMT_RGB666;
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
					hx8357_var.bits_per_pixel);
		ret = -EINVAL;
		break;
	}

	/* Set the bus config */
	if (ret == 0) {
		ret = bus->set_conf(dev, &busconf);
	}

	return ret;
}

/**
 * hx8357_set_gpio_config - Sets the gpio config
 *
 */
static int 
hx8357_set_gpio_config(struct device *dev)
{
	int ret=0;

	/* EOFI pin is connected to the GPIOB7 */
	pnx_gpio_request(GPIO_B7);
	pnx_gpio_set_mode(GPIO_B7, GPIO_MODE_MUX0);
	pnx_gpio_set_direction(GPIO_B7, GPIO_DIR_INPUT); // TODO: Check the Pull Up/Down mode

	return ret;
}
/**
 * hx8357_check_cmd_list_overflow
 * 
 */
#define hx8357_check_cmd_list_overflow()                                      \
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
 * hx8357_free_cmd_list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 *
 * This function removes and free's all lcdbus_cmd's from cmds.
 */
static void
hx8357_free_cmd_list(struct device *dev, struct list_head *cmds)
{
	struct lcdbus_cmd *cmd, *tmp;
  struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	list_for_each_entry_safe(cmd, tmp, cmds, link) {
		list_del(&cmd->link);
	}

	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;
}

/**
 * hx8357_add_ctrl_cmd
 * cmds: commandes list
 * @dev: device which has been used to call this function
 * @reg: the register that is addressed
 * @value: the value that should be copied into the register
 *
 * This function adds the given command to the internal command chain
 * of the LCD driver.
 */
static int hx8357_add_ctrl_cmd(struct device *dev, struct list_head *cmds, const u8 reg, u8 *value, u32 len)
{
  struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);
	pr_debug("%s()\n", __FUNCTION__);

  	hx8357_check_cmd_list_overflow();


	drvdata->curr_cmd->cmd = reg;
	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->data_phys = drvdata->curr_cmd->data_int_phys;
	if(device_id == HX8357)
	{
		drvdata->curr_cmd->data_int[0] = *value;
	drvdata->curr_cmd->len = 1;
	}
	else
	{
		memcpy(drvdata->curr_cmd->data_int,value,len);
		drvdata->curr_cmd->len = len;
	}
	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */
	return 0;
}

/**
 * hx8357_add_data_cmd - add a request to the internal command chain
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
hx8357_add_data_cmd(struct device *dev, struct list_head *cmds, struct lcdfb_transfer *transfer)
{
	struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);
  	hx8357_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_DATA;
	drvdata->curr_cmd->cmd  = gram_cmd;//accesss GRAM);
	drvdata->curr_cmd->w =transfer->w -(transfer->x);
	drvdata->curr_cmd->h = transfer->h -(transfer->y);
	drvdata->curr_cmd->bytes_per_pixel = hx8357_var.bits_per_pixel >> 3;
	drvdata->curr_cmd->stride = hx8357_fix.line_length;
	drvdata->curr_cmd->data_phys = transfer->addr_phys + 
			transfer->x * drvdata->curr_cmd->bytes_per_pixel + 
			transfer->y * drvdata->curr_cmd->stride;
	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */
	return 0;
}
/**
 * hx8357_execute_cmds - execute the internal command chain
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
hx8357_execute_cmds(struct device *dev, struct list_head *cmds, u16 delay)
{
	int ret;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;

	pr_debug("%s()\n", __FUNCTION__);

	/* Write data to the bus */
	ret = bus->write(dev, cmds);
	hx8357_free_cmd_list(dev,cmds);

	/* Need to wait ? */
	if (delay != 0)
		mdelay(delay);

	return ret;
}
/**
 * hx8357_add_transfer_cmd - adds lcdbus_cmd's from a transfer to list
 * @dev: device which has been used to call this function
 * @transfer: the lcdfb_transfer to be converted
 * @cmds: a LLI list of lcdbus_cmd's
 *
 * This function creates a command list for a given transfer. Therefore it
 * selects the x- and y-start position in the display ram, issues the write-ram
 * command and sends the data.
 */
 static u8 cmd_data[32];
static int
hx8357_add_transfer_cmd(struct device *dev,
	                     struct list_head *cmds,
	                     struct lcdfb_transfer *transfer)
{
	//struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;	
	//u16 rotation;
	
	pr_debug("%s()\n", __FUNCTION__);
	hx8357_timing.mode = 1;
	bus->set_timing(dev, &hx8357_timing);	
	if(device_id == HX8368)
	{
		memset(cmd_data,0,sizeof(cmd_data));
		cmd_data[0] = (u8)(transfer->x >> 8);
		cmd_data[1] = (u8)(transfer->x);
		cmd_data[2] = (u8)((transfer->w -1) >> 8);
		cmd_data[3] = (u8)(transfer->w -1) ;
		hx8357_add_ctrl_cmd(dev, cmds, 0x2A, cmd_data,4);
		hx8357_execute_cmds(dev, cmds, 0);
		memset(cmd_data,0,sizeof(cmd_data));
		cmd_data[0] = 0x00;
		cmd_data[1] = (u8)transfer->y;
		cmd_data[2] = 0x00;
		cmd_data[3] = (u8)(transfer->h -1);
		hx8357_add_ctrl_cmd(dev, cmds, 0x2B, cmd_data,4);
		hx8357_execute_cmds(dev, cmds, 0);		
	}
	else if(device_id == HX8357)
	{
		memset(cmd_data,0,sizeof(cmd_data));
		cmd_data[0] = (u8)(transfer->x >> 8);
		hx8357_add_ctrl_cmd(dev, cmds, 0x02, cmd_data,1);
		cmd_data[0] = (u8)(transfer->x);
		hx8357_add_ctrl_cmd(dev, cmds, 0x03, cmd_data,1);
		cmd_data[0] = (u8)((transfer->w -1) >> 8);
		hx8357_add_ctrl_cmd(dev, cmds, 0x04, cmd_data,1);
		cmd_data[0] = (u8)(transfer->w -1) ;
		hx8357_add_ctrl_cmd(dev, cmds, 0x05, cmd_data,1);
		cmd_data[0] = 0x00;
		hx8357_add_ctrl_cmd(dev, cmds, 0x06, cmd_data,1);
		cmd_data[0] = (u8)transfer->y;
		hx8357_add_ctrl_cmd(dev, cmds, 0x07, cmd_data,1);
		cmd_data[0] = 0x00;
		hx8357_add_ctrl_cmd(dev, cmds, 0x08, cmd_data,1);
		cmd_data[0] = (u8)(transfer->h -1);
		hx8357_add_ctrl_cmd(dev, cmds, 0x09, cmd_data,1);
		cmd_data[0] = (u8)(transfer->x >> 8);																
		hx8357_add_ctrl_cmd(dev, cmds, 0x80, cmd_data,1);			//column address counter 2
		cmd_data[0] = (u8)(transfer->x);
		hx8357_add_ctrl_cmd(dev, cmds, 0x81, cmd_data,1);		//column address counter 1
		cmd_data[0] = 0x00;
		hx8357_add_ctrl_cmd(dev, cmds, 0x82, cmd_data,1);		//row address counter 2
		cmd_data[0] = (u8)transfer->y;
		hx8357_add_ctrl_cmd(dev, cmds, 0x83, cmd_data,1);		//row address counter 1
		hx8357_execute_cmds(dev, cmds, 0);
	}
	hx8357_timing.mode = 2;
	bus->set_timing(dev, &hx8357_timing);
	hx8357_add_data_cmd(dev,cmds, transfer);
	hx8357_execute_cmds(dev, cmds, 0);
	return 0;
}




static int
hx8357_switchon(struct device *dev)
{
	struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;	
	int ret = 0;
	struct list_head cmds;

	pr_debug("%s()\n", __FUNCTION__);
	hx8357_timing.mode = 1;
	bus->set_timing(dev, &hx8357_timing);		

	/* ------------------------------------------------------------------------
	 * Set the bus configuration for the display
	 * --------------------------------------------------------------------- */
	ret = hx8357_set_bus_config(dev);
	if (ret < 0) {
		dev_err(dev, "Could not set bus config\n");
		return -EBUSY;
	}

	/* ------------------------------------------------------------------------
	 * Initialize the LCD HW
	 * --------------------------------------------------------------------- */
	INIT_LIST_HEAD(&cmds);
	//Selwyn modified for K3
	#ifdef ACER_L1_K3
	{
            if(standby_mode)
            {
		//exit_sleep_mode
		hx8357_add_ctrl_cmd(dev, &cmds, 0x11, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 120);

		//set_display_on
		hx8357_add_ctrl_cmd(dev, &cmds, 0x29, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 10);
            }
            else
            {
		//SW reset
		//hx8357_add_ctrl_cmd(dev, &cmds, 0x01, cmd_data,0);
		//hx8357_execute_cmds(dev, &cmds, 120);
#if 1
		cmd_data[0] = 0xFF;
		cmd_data[1] = 0x83;
		cmd_data[2] = 0x68;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB9, cmd_data,3);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0x4A;
		cmd_data[1] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xCB, cmd_data,2);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0xEE;
		cmd_data[1] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB0, cmd_data,2);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0x11;
		cmd_data[1] = 0x00;
		cmd_data[2] = 0x00;
		cmd_data[3] = 0x08;
		cmd_data[4] = 0x08;
		cmd_data[5] = 0x03;
		cmd_data[6] = 0xA3;	
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB4, cmd_data,7);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		hx8357_add_ctrl_cmd(dev, &cmds, 0x11, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 100);

		cmd_data[0] = 0x00;
		cmd_data[1] = 0x2B;
		cmd_data[2] = 0x2A;
		cmd_data[3] = 0x39;
		cmd_data[4] = 0x33;
		cmd_data[5] = 0x39;
		cmd_data[6] = 0x38;
		cmd_data[7] = 0x7F;
		cmd_data[8] = 0x0C;
		cmd_data[9] = 0x05;
		cmd_data[10] = 0x09;
		cmd_data[11] = 0x0F;
		cmd_data[12] = 0x1A;
		cmd_data[13] = 0x06;
		cmd_data[14] = 0x0C;
		cmd_data[15] = 0x06;
		cmd_data[16] = 0x15;
		cmd_data[17] = 0x14;
		cmd_data[18] = 0x3F;
		cmd_data[19] = 0x00;
		cmd_data[20] = 0x47;
		cmd_data[21] = 0x05;
		cmd_data[22] = 0x10;
		cmd_data[23] = 0x16;
		cmd_data[24] = 0x1A;
		cmd_data[25] = 0x13;
		cmd_data[26] = 0x00;

		hx8357_add_ctrl_cmd(dev, &cmds, 0xE0, cmd_data,27);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		hx8357_add_ctrl_cmd(dev, &cmds, 0x35, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));

		hx8357_add_ctrl_cmd(dev, &cmds, 0x29, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 100);
#endif
//Selwyn 2010-6-17 marked this because of tearing effect issue
#if 0
		//set EXTC
		cmd_data[0] = 0xFF;
		cmd_data[1] = 0x83;
		cmd_data[2] = 0x68;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB9, cmd_data,3);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));

		//set VCOM Voltage
		//cmd_data[0] = 0x80;
		//cmd_data[1] = 0x81;
		//cmd_data[1] = 0x76;
		//hx8357_add_ctrl_cmd(dev, &cmds, 0xB6, cmd_data,3);
		//hx8357_execute_cmds(dev, &cmds, 10);
		//memset(cmd_data,0,sizeof(cmd_data));

		//set Power
		cmd_data[0] = 0x00;
		cmd_data[1] = 0x01;
		cmd_data[2] = 0x1E;
		cmd_data[3] = 0x00;
		cmd_data[4] = 0x22;
		cmd_data[5] = 0x11;
		cmd_data[6] = 0x8D;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB1, cmd_data,7);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));

		//set gamma curve
		cmd_data[0] = 0x00;
		cmd_data[1] = 0x2B;
		cmd_data[2] = 0x2A;
		cmd_data[3] = 0x39;
		cmd_data[4] = 0x33;
		cmd_data[5] = 0x39;
		cmd_data[6] = 0x38;
		cmd_data[7] = 0x7F;
		cmd_data[8] = 0x0C;
		cmd_data[9] = 0x05;
		cmd_data[10] = 0x09;
		cmd_data[11] = 0x0F;
		cmd_data[12] = 0x1A;
		cmd_data[13] = 0x06;
		cmd_data[14] = 0x0C;
		cmd_data[15] = 0x06;
		cmd_data[16] = 0x15;
		cmd_data[17] = 0x14;
		cmd_data[18] = 0x3F;
		cmd_data[19] = 0x00;
		cmd_data[20] = 0x47;
		cmd_data[21] = 0x05;
		cmd_data[22] = 0x10;
		cmd_data[23] = 0x16;
		cmd_data[24] = 0x1A;
		cmd_data[25] = 0x13;
		cmd_data[26] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE0, cmd_data,27);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));

		//set tearing mode
		hx8357_add_ctrl_cmd(dev, &cmds, 0x35, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 10);

		//set panel
		cmd_data[0] = 0x0F;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xCC, cmd_data,1);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));

		//exit_sleep_mode
		hx8357_add_ctrl_cmd(dev, &cmds, 0x11, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 120);

		//set_display_on
		hx8357_add_ctrl_cmd(dev, &cmds, 0x29, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 10);
#endif
            }
	}
	#else
	if(device_id == HX8368)
	{
		cmd_data[0] = 0xFF;
		cmd_data[1] = 0x83;
		cmd_data[2] = 0x68;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB9, cmd_data,3);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0x4A;
		cmd_data[1] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xCB, cmd_data,2);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));


		cmd_data[0] = 0xEE;
		cmd_data[1] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB0, cmd_data,2);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0x11;
		cmd_data[1] = 0x00;
		cmd_data[2] = 0x00;
		cmd_data[3] = 0x08;
		cmd_data[4] = 0x08;
		cmd_data[5] = 0x03;
		cmd_data[6] = 0xA3;	
		hx8357_add_ctrl_cmd(dev, &cmds, 0xB4, cmd_data,7);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));


		hx8357_add_ctrl_cmd(dev, &cmds, 0x11, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 100);


		//cmd_data[0] = 0xC0;
		//hx8357_add_ctrl_cmd(dev, &cmds, 0x36, cmd_data,1);
		//hx8357_execute_cmds(dev, &cmds, 5);
		//memset(cmd_data,0,sizeof(cmd_data));

		cmd_data[0] = 0x00;
		cmd_data[1] = 0x25;
		cmd_data[2] = 0x23;
		cmd_data[3] = 0x21;
		cmd_data[4] = 0x21;
		cmd_data[5] = 0x3F;
		cmd_data[6] = 0x18;
		cmd_data[7] = 0x5E;
		cmd_data[8] = 0x09;
		cmd_data[9] = 0x05;
		cmd_data[10] = 0x08;
		cmd_data[11] = 0x0D;
		cmd_data[12] = 0x17;
		cmd_data[13] = 0x00;
		cmd_data[14] = 0x1E;
		cmd_data[15] = 0x1E;
		cmd_data[16] = 0x1C;
		cmd_data[17] = 0x1A;
		cmd_data[18] = 0x3F;
		cmd_data[19] = 0x21;
		cmd_data[20] = 0x67;
		cmd_data[21] = 0x07;
		cmd_data[22] = 0x12;
		cmd_data[23] = 0x18;
		cmd_data[24] = 0x1A;
		cmd_data[25] = 0x16;
		cmd_data[26] = 0xFF;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE0, cmd_data,27);
		hx8357_execute_cmds(dev, &cmds, 5);
		memset(cmd_data,0,sizeof(cmd_data));

		hx8357_add_ctrl_cmd(dev, &cmds, 0x35, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 10);
		memset(cmd_data,0,sizeof(cmd_data));


		hx8357_add_ctrl_cmd(dev, &cmds, 0x29, cmd_data,0);
		hx8357_execute_cmds(dev, &cmds, 100);
	}
	else if(device_id == HX8357)
	{
		cmd_data[0] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xFF, cmd_data,1);
		cmd_data[1] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE4, cmd_data+1,1);
		cmd_data[2] = 0x1C;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE5, cmd_data+2,1);
		cmd_data[3] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE6, cmd_data+3,1);
		cmd_data[4] = 0x1C;
		hx8357_add_ctrl_cmd(dev, &cmds, 0xE7, cmd_data+4,1);
		cmd_data[5] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x19, cmd_data+5,1);
		hx8357_execute_cmds(dev, &cmds, 10);
	
		memset(cmd_data,0,sizeof(cmd_data));


		cmd_data[0] = 0x11;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x18, cmd_data,1);
		cmd_data[1] = 0x11;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x2A, cmd_data+1,1);
		cmd_data[2] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x02, cmd_data+2,1);
		cmd_data[3] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x03, cmd_data+3,1);
		cmd_data[4] = 0x01;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x04, cmd_data+4,1);
		cmd_data[5] = 0x3F;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x05, cmd_data+5,1);
		cmd_data[6] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x06, cmd_data+6,1);
		cmd_data[7] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x07, cmd_data+7,1);
		cmd_data[8] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x08, cmd_data+8,1);
		cmd_data[9] = 0xEF;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x09, cmd_data+9,1);
		cmd_data[10] = 0x22;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x24, cmd_data+10,1);
		cmd_data[11] = 0x64;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x25, cmd_data+11,1);	
		cmd_data[12] = 0x9A;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x23, cmd_data+12,1);
		cmd_data[13] = 0x0E;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1B, cmd_data+13,1);		
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[14] = 0x11;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1D, cmd_data+14,1);		
		hx8357_execute_cmds(dev, &cmds, 10);

		memset(cmd_data,0,sizeof(cmd_data));


		cmd_data[0] = 0x01;																//Gamma control
		hx8357_add_ctrl_cmd(dev, &cmds, 0x40, cmd_data,1);
		cmd_data[1] = 0x2D;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x41, cmd_data+1,1);
		cmd_data[2] = 0x29;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x42, cmd_data+2,1);
		cmd_data[3] = 0x2F;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x43, cmd_data+3,1);
		cmd_data[4] = 0x2C;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x44, cmd_data+4,1);
		cmd_data[5] = 0x31;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x45, cmd_data+5,1);
		cmd_data[6] = 0x24;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x46, cmd_data+6,1);
		cmd_data[7] = 0x76;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x47, cmd_data+7,1);
		cmd_data[8] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x48, cmd_data+8,1);
		cmd_data[9] = 0x03;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x49, cmd_data+9,1);
		cmd_data[10] = 0x07;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x4A, cmd_data+10,1);
		cmd_data[11] = 0x11;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x4B, cmd_data+11,1);	
		cmd_data[12] = 0x11;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x4C, cmd_data+12,1);
		cmd_data[13] = 0x0E;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x50, cmd_data+13,1);		
		cmd_data[14] = 0x13;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x51, cmd_data+14,1);
		cmd_data[15] = 0x10;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x52, cmd_data+15,1);
		cmd_data[16] = 0x16;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x53, cmd_data+16,1);
		cmd_data[17] = 0x12;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x54, cmd_data+17,1);
		cmd_data[18] = 0x3E;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x55, cmd_data+18,1);
		cmd_data[19] = 0x09;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x56, cmd_data+19,1);
		cmd_data[20] = 0x5B;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x57, cmd_data+20,1);
		cmd_data[21] = 0x0E;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x58, cmd_data+21,1);
		cmd_data[22] = 0x0E;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x59, cmd_data+22,1);
		cmd_data[23] = 0x18;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x5A, cmd_data+23,1);
		cmd_data[24] = 0x1B;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x5B, cmd_data+24,1);
		cmd_data[25] = 0x1F;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x5C, cmd_data+25,1);	
		cmd_data[26] = 0xC0;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x5D, cmd_data+26,1);
		cmd_data[27] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x01, cmd_data+27,1);		//Display mode control register

		hx8357_execute_cmds(dev, &cmds, 10);	
		memset(cmd_data,0,sizeof(cmd_data));



		cmd_data[0] = 0x03;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1C, cmd_data,1);		//power control 3
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[1] = 0x8C;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1F, cmd_data+1,1);		//power control 6
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[2] = 0x84;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1F, cmd_data+2,1);		
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[3] = 0x94;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1F, cmd_data+3,1);		
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[4] = 0xD4;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1F, cmd_data+4,1);		
		hx8357_execute_cmds(dev, &cmds, 10);

		cmd_data[5] = 0x38;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x28, cmd_data+5,1);		//display control 3
		hx8357_execute_cmds(dev, &cmds, 40);

		cmd_data[6] = 0x3C;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x28, cmd_data+6,1);	
		hx8357_execute_cmds(dev, &cmds, 40);

		memset(cmd_data,0,sizeof(cmd_data));


		cmd_data[0] = 0x00;																
		hx8357_add_ctrl_cmd(dev, &cmds, 0x80, cmd_data,1);			//column address counter 2
		cmd_data[1] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x81, cmd_data+1,1);		//column address counter 1
		cmd_data[2] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x82, cmd_data+2,1);		//row address counter 2
		cmd_data[3] = 0x00;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x83, cmd_data+3,1);		//row address counter 1
		cmd_data[4] = 0x05;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x1A, cmd_data+4,1);
		cmd_data[5] = 0x08;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x60, cmd_data+5,1);
		cmd_data[6] = 0x06;
		hx8357_add_ctrl_cmd(dev, &cmds, 0x17, cmd_data+6,1);

		hx8357_execute_cmds(dev, &cmds, 10);	

		memset(cmd_data,0,sizeof(cmd_data));

	}
	#endif //~ACER_L1_K3
	//~Selwyn modified
	hx8357_timing.mode = 2;
	bus->set_timing(dev, &hx8357_timing);
	drvdata->power_mode = FB_BLANK_UNBLANK;

	return ret;
}
static void check_lcd_status(struct device *dev)
{
	struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);
	
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct list_head commands;
	struct lcdbus_cmd *tmp1, *tmp2;
	u8 value = 0;

	hx8357_timing.mode = 1;
	bus->set_timing(dev, &hx8357_timing);

	INIT_LIST_HEAD(&commands);

	
	drvdata->curr_cmd->cmd = 0x0A;
	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->data_phys = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->len = 2;
  	list_add_tail(&drvdata->curr_cmd->link, &commands);
  //drvdata->curr_cmd ++; /* Next command */	
	bus->read(dev, &commands);
	
	value = drvdata->curr_cmd->data_int[1];
	list_for_each_entry_safe(tmp1, tmp2, &commands, link) {
		list_del(&tmp1->link);
	}
	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;	
	
	if( value != 0x9C)		// device id
	{
		hx8357_switchon(dev);
		mdelay(500);
		printk("LCM status error = %x", value);
	}
	hx8357_timing.mode = 2;
	bus->set_timing(dev, &hx8357_timing);
	
}
/*
 =========================================================================
 =                                                                       =
 =              device detection and bootstrapping                       =
 =                                                                       =
 =========================================================================
*/

/**
 * hx8357_device_supported - perform hardware detection check
 * @dev:	pointer to the device which should be checked for support
 *
 */
static int __devinit
hx8357_device_supported(struct device *dev)
{
	int ret = 0;

	/*
	 * Hardware detection of the display does not seem to be supported
	 * by the hardware, thus we assume the display is there!
	 */
	if (strcmp(dev->bus_id, "pnx-vde-lcd0"))
		ret = -ENODEV; // (pnx-vde-lcd0 not detected (pb during VDE init)

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
 * hx8357_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function converts the list of lcdfb_transfer into a list of resulting
 * lcdbus_cmd's which then gets sent to the display controller using the
 * underlying bus driver.
 */
static int
hx8357_write(const struct device *dev, const struct list_head *transfers)
{
	struct hx8357_drvdata *drvdata =
		dev_get_drvdata((struct device *)dev->parent);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;
	struct list_head cmds;
	struct lcdfb_transfer *transfer;
	int ret = 0;
	pr_debug("%s()\n", __FUNCTION__);
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		dev_warn((struct device *)dev, "NOT allowed (refresh while power off)\n");
		return 0;
	}
	if (list_empty(transfers)) {
		dev_warn((struct device *)dev, "Got an empty transfer list\n");
		return 0;
	}


	/* lock hardware access */
	mutex_lock(&drvdata->lock);

	if(device_id == HX8368)
	{
		check_lcd_status(dev->parent);
	}
	/* now get on with the real stuff */
	INIT_LIST_HEAD(&cmds);

	/* moved out of the loop for performance improvements */
	drvdata->byte_shift = (hx8357_var.bits_per_pixel >> 3) - 1;
	list_for_each_entry(transfer, transfers, link) {
		ret |= hx8357_add_transfer_cmd(dev->parent, &cmds, transfer);
	}
//modified by wesley,2009.09.04
#if 0
	if (ret >= 0) {
		/* execute the cmd list we build */
		ret = bus->write(dev->parent, &cmds);
	}
	/* Now buffers may be freed by the application */
	hx8357_free_cmd_list(dev->parent,&cmds);
#endif


	/* unlock hardware access */
	mutex_unlock(&drvdata->lock);


	return ret;
}

/**
 * hx8357_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev: device which has been used to call this function
 * @fsi: structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
hx8357_get_fscreeninfo(const struct device *dev,
		struct fb_fix_screeninfo *fsi)
{
	BUG_ON(!fsi);

	pr_debug("%s()\n", __FUNCTION__);

	*fsi = hx8357_fix;
	return 0;
}

/**
 * hx8357_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev: device which has been used to call this function
 * @vsi: structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
hx8357_get_vscreeninfo(const struct device *dev,
		struct fb_var_screeninfo *vsi)
{
	BUG_ON(!vsi);

	pr_debug("%s()\n", __FUNCTION__);

	*vsi = hx8357_var;
	return 0;
}

/**
 * hx8357_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
hx8357_display_on(struct device *dev)
{
	int ret = 0;
	struct hx8357_drvdata *drvdata = dev_get_drvdata((struct device *)dev->parent);
	/* Switch on the display if needed */
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		ret = hx8357_switchon(dev->parent);
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_UNBLANK mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}

/**
 * hx8357_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
hx8357_display_off(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;
	struct hx8357_drvdata *drvdata = dev_get_drvdata((struct device *)dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (drvdata->power_mode != FB_BLANK_POWERDOWN) {
		hx8357_timing.mode = 1;
		bus->set_timing(dev->parent, &hx8357_timing);	
		INIT_LIST_HEAD(&cmds);
		if(device_id == HX8368)
		{
			hx8357_add_ctrl_cmd(dev->parent, &cmds, 0x28, cmd_data, 0);
			hx8357_execute_cmds(dev->parent, &cmds, 40);
			//Selwyn 2010-03-22 modified because we use standby mode to replace display poweroff
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x10, cmd_data,0);
			hx8357_execute_cmds(dev->parent, &cmds, 10);
			standby_mode = 1;
			//~Selwyn modified
		}
		hx8357_timing.mode = 2;
		bus->set_timing(dev->parent, &hx8357_timing);		
		drvdata->power_mode = FB_BLANK_POWERDOWN;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_POWERDOWN mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return ret;
}
/**
 * hx8357_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
hx8357_display_standby(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;		
	struct hx8357_drvdata *drvdata = dev_get_drvdata((struct device *)dev->parent);
	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_VSYNC_SUSPEND) {
		hx8357_timing.mode = 1;
		bus->set_timing(dev->parent, &hx8357_timing);	
		INIT_LIST_HEAD(&cmds);
		if(device_id == HX8368)
		{
			//Selwyn modified
			hx8357_add_ctrl_cmd(dev->parent, &cmds, 0x28, cmd_data, 0);
			hx8357_execute_cmds(dev->parent, &cmds, 40);
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x10, cmd_data,0);
			hx8357_execute_cmds(dev->parent, &cmds, 10);
			standby_mode = 1;
			//~Selwyn modified	
		}
		else
		{
			memset(cmd_data,0,sizeof(cmd_data));
			cmd_data[0] = 0x38;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data,1);		//power control 3
			hx8357_execute_cmds(dev->parent, &cmds, 50);

			cmd_data[1] = 0x24;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data+1,1);		//power control 6
			hx8357_execute_cmds(dev->parent, &cmds, 50);

			cmd_data[2] = 0x04;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data+2,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);

			cmd_data[3] = 0x89;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x1f, cmd_data+3,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);

			cmd_data[4] = 0x00;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x19, cmd_data+4,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);
			
		}	
		hx8357_timing.mode = 2;
		bus->set_timing(dev->parent, &hx8357_timing);		
		drvdata->power_mode = FB_BLANK_VSYNC_SUSPEND;
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_VSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return  ret;
}

/**
 * hx8357_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
hx8357_display_deep_standby(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev->parent);
	struct lcdbus_ops *bus = ldev->ops;	
	struct hx8357_drvdata *drvdata = dev_get_drvdata((struct device *)dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_HSYNC_SUSPEND) 
	{
		hx8357_timing.mode = 1;
		bus->set_timing(dev->parent, &hx8357_timing);	
		INIT_LIST_HEAD(&cmds);
		
		if(device_id == HX8368)
		{
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x10, cmd_data,0);
			hx8357_execute_cmds(dev->parent, &cmds, 120);
	
	}
		else
		{
			memset(cmd_data,0,sizeof(cmd_data));
			cmd_data[0] = 0x38;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data,1);
			hx8357_execute_cmds(dev->parent, &cmds, 50);

			cmd_data[1] = 0x24;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data+1,1);
			hx8357_execute_cmds(dev->parent, &cmds, 50);

			cmd_data[2] = 0x04;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x28, cmd_data+2,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);

			cmd_data[3] = 0x89;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x1f, cmd_data+3,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);

			cmd_data[4] = 0x00;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x19, cmd_data+4,1);		
			hx8357_execute_cmds(dev->parent, &cmds, 10);

			cmd_data[5] = 0xc0;
			hx8357_add_ctrl_cmd(dev->parent,&cmds, 0x01, cmd_data+5,1);
			hx8357_execute_cmds(dev->parent, &cmds, 10);
			
		}
	
		hx8357_timing.mode = 2;
		bus->set_timing(dev->parent, &hx8357_timing);			
		drvdata->power_mode = FB_BLANK_HSYNC_SUSPEND;
	}
	else 
	{
		dev_err(dev, "Display already in FB_BLANK_HSYNC_SUSPEND mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	return  ret;
}

/**
 * hx8357_get_splash_info - copies the splash screen info into si
 * @dev: device which has been used to call this function
 * @si:  structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
hx8357_get_splash_info(struct device *dev,
		struct lcdfb_splash_info *si)
{
	BUG_ON(!si);

	pr_debug("%s()\n", __FUNCTION__);

	*si = hx8357_splash_info;
	return 0;
}


/**
 * hx8357_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
hx8357_get_specific_config(struct device *dev,
		struct lcdfb_specific_config **sc)
{
	BUG_ON(!sc);

	pr_debug("%s()\n", __FUNCTION__);

	(*sc) = &hx8357_specific_config;
	return 0;
}
/**
 * hx8357_get_device_attrs
 * @dev: device which has been used to call this function
 * @device_attrs: device attributes to be returned
 * @attrs_nbr: the number of device attributes
 * 
 *
 * Returns the device attributes to be added to the SYSFS
 * entries (/sys/class/graphics/fbX/....
 * */
static int 
hx8357_get_device_attrs(struct device *dev, 
		struct device_attribute **device_attrs,
		unsigned int *attrs_nbr)
{
	
	BUG_ON(!device_attrs);

	pr_debug("%s()\n", __FUNCTION__);

	(*device_attrs) = hx8357_device_attrs;

	(*attrs_nbr) = sizeof(hx8357_device_attrs)/sizeof(hx8357_device_attrs[0]);

	/* Return the error code */
	return 0;
}
/*
 * hx8357_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int hx8357_check_var(const struct device *dev,
		 struct fb_var_screeninfo *vsi)
{
	int ret = -EINVAL;

	BUG_ON(!vsi);

	pr_debug("%s()\n", __FUNCTION__);

	/* check xyres */
	if ((vsi->xres != hx8357_var.xres) ||
		(vsi->yres != hx8357_var.yres)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xyres virtual */
	if ((vsi->xres_virtual != hx8357_var.xres_virtual) ||
		(vsi->yres_virtual != hx8357_var.yres_virtual)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xoffset */
	if (vsi->xoffset != hx8357_var.xoffset) {
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
	return ret;
}

/*
 * hx8357_set_par
 * @dev: device which has been used to call this function
 * @vsi: structure  var screeninfo to set
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static inline void hx8357_init_color(struct fb_bitfield *color, 
		__u32 offset, __u32 length, __u32 msb_right)
{
	color->offset     = offset;
	color->length     = length;
	color->msb_right  = msb_right;
}

static int hx8357_set_par(const struct device *dev,
		struct fb_info *info)
{
	int ret = -EINVAL;
	int set_params = 0;
	struct fb_var_screeninfo *vsi = &info->var;

	BUG_ON(!vsi);

	pr_debug("%s()\n", __FUNCTION__);

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
		if (! memcmp(vsi, &hx8357_var, sizeof(struct fb_var_screeninfo))) {
			ret = 0;
			goto quit;
		}

		/* Check rotation */
		if (hx8357_var.rotate != vsi->rotate) {
			hx8357_var.rotate = vsi->rotate;
		}

		/* Check bpp */
		if (hx8357_var.bits_per_pixel != vsi->bits_per_pixel) {

			u32 bytes_per_pixel;
			
			hx8357_var.bits_per_pixel = vsi->bits_per_pixel;

			switch(hx8357_var.bits_per_pixel) {
			case 16:
				hx8357_init_color(&hx8357_var.red,  11, 5, 0);
				hx8357_init_color(&hx8357_var.green, 5, 6, 0);
				hx8357_init_color(&hx8357_var.blue,  0, 5, 0);
				break;

			case 24:
			case 32:
				hx8357_init_color(&hx8357_var.red,   16, 8, 0);
				hx8357_init_color(&hx8357_var.green,  8, 8, 0);
				hx8357_init_color(&hx8357_var.blue,   0, 8, 0);
				break;
			}

			bytes_per_pixel = hx8357_var.bits_per_pixel/8;
		
			/* Set the new line length (BPP or resolution changed) */
			hx8357_fix.line_length = hx8357_var.xres_virtual*bytes_per_pixel;

			/* Switch off the LCD HW */
			hx8357_display_off((struct device *)dev);

			/* Switch on the LCD HW */
			ret = hx8357_display_on((struct device *)dev);
		}

		/* TODO: Check other parameters */

		ret = 0;
	}

quit:	
	return ret;
}


/**
 * hx8357_ioctl
 * @dev: device which has been used to call this function
 * @cmd: requested command
 * @arg: command argument
 * */
static int
hx8357_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int value;
	struct hx8357_drvdata *drvdata =
						dev_get_drvdata((struct device *)dev->parent);

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
struct lcdfb_ops hx8357_ops = {
	.write           = hx8357_write,
	.get_fscreeninfo = hx8357_get_fscreeninfo,
	.get_vscreeninfo = hx8357_get_vscreeninfo,
	.get_splash_info = hx8357_get_splash_info,
	.get_specific_config = hx8357_get_specific_config,
	.get_device_attrs= hx8357_get_device_attrs,
	.display_on      = hx8357_display_on,
	.display_off     = hx8357_display_off,
	.display_standby = hx8357_display_standby,
	.display_deep_standby = hx8357_display_deep_standby,	
	.check_var       = hx8357_check_var,
	.set_par         = hx8357_set_par,
	.ioctl           = hx8357_ioctl,
};

/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/
static lcd_device_id_t check_lcd_device(struct device *dev)
{
	struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);
	
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct list_head commands;
	struct lcdbus_cmd *tmp1, *tmp2;
	lcd_device_id_t id;

	INIT_LIST_HEAD(&commands);

	
	drvdata->curr_cmd->cmd = 0x04;
	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->data_phys = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->len = 4;
  	list_add_tail(&drvdata->curr_cmd->link, &commands);
  //drvdata->curr_cmd ++; /* Next command */	
	bus->read(dev, &commands);
	if( drvdata->curr_cmd->data_int[3] == 0x88)		// device id
	{
		
		id = HX8368;
	}
	else
	{
		id = HX8357;
	}


	list_for_each_entry_safe(tmp1, tmp2, &commands, link) {
		list_del(&tmp1->link);
	}

	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;
	return id;
}


static int __devinit hx8357_probe(struct device *dev)
{
	struct hx8357_drvdata *drvdata;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct lcdbus_cmd *cmd;
    u32 cmds_list_phys;
	int ret;
	int i;

	pr_debug("%s()\n", __FUNCTION__);
	BUG_ON(!bus || !bus->read || !bus->write || !bus->get_conf ||
	       !bus->set_conf || !bus->set_timing);

	/* --------------------------------------------------------------------
	 * Hardware detection
	 * ---- */
	ret = hx8357_device_supported(dev);
	if (ret < 0)
		goto out;

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

#ifdef HWMEM_ALLOC
	drvdata->cmds_list = (struct lcdbus_cmd *)hwmem_alloc(
			drvdata->cmds_list_max_size);
	drvdata->cmds_list_phys = hwmem_pa(drvdata->cmds_list);

#else
	drvdata->cmds_list = (struct lcdbus_cmd *)dma_alloc_coherent(NULL, 
			drvdata->cmds_list_max_size, &(drvdata->cmds_list_phys), 
			GFP_KERNEL | GFP_DMA);
#endif 

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

	printk("%s (cmd %p, curr %p, last %p)\n", 
			__FUNCTION__, drvdata->cmds_list, drvdata->curr_cmd, 
			drvdata->last_cmd); 

	/* Set drvdata */
	dev_set_drvdata(dev, drvdata);

	drvdata->power_mode = hx8357_specific_config.boot_power_mode;
	drvdata->fb.ops = &hx8357_ops;
	drvdata->fb.dev.parent = dev;
	snprintf(drvdata->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", dev->bus_id);

	/* --------------------------------------------------------------------
	 * Set display timings before anything else happens
	 * ---- */
	 hx8357_timing.mode = 1;
	bus->set_timing(dev, &hx8357_timing);

	if (HX8357_262K_COLORS == 0) {
		drvdata->use_262k_colors = 1;
	}


	/* initialize mutex to lock hardware access */
	mutex_init(&(drvdata->lock));


	/* --------------------------------------------------------------------
	 * GPIO configuration is specific for each LCD
	 * (Chech the HW datasheet (VDE_EOFI, VDE_EOFI_copy)
	 * ---- */
	ret = hx8357_set_gpio_config(dev);
	if (ret < 0) {
		dev_err(dev, "Could not set gpio config\n");
		return -EBUSY;
	}

	/* --------------------------------------------------------------------
	 * Initialize the LCD if the initial state is ON (FB_BLANK_UNBLANK)
	 * ---- */
	if (drvdata->power_mode == FB_BLANK_UNBLANK) {
			hx8357_set_bus_config(dev);

			#ifdef ACER_L1_K3
			device_id = HX8368;
			#else
			device_id = check_lcd_device( dev);
			#endif

			if(device_id == HX8357)
			{
				gram_cmd = 0x22;
			}
			else if(device_id == HX8368)
			{
				gram_cmd = 0x2c;
			}
			else
			{
				printk("hx8357_probe can not find any lcd device \n");
			}
		/*
		ret = hx8357_switchon(dev);
		if (ret < 0) {
			ret = -EBUSY;
			goto out;
		}*/
	}
	hx8357_timing.mode = 2;
	bus->set_timing(dev, &hx8357_timing);
	ret = lcdfb_device_register(&drvdata->fb);
	if (ret < 0) {
		goto err_free_cmds_list;
	}
	/* Everything is OK, so return */
	goto out;

err_free_cmds_list:

#ifdef HWMEM_ALLOC
	hwmem_free(drvdata->cmds_list);
#else
	dma_free_coherent(NULL, drvdata->cmds_list_max_size, drvdata->cmds_list, 
			(dma_addr_t)drvdata->cmds_list_phys);
#endif

err_free_drvdata:
	kfree(drvdata);

out:
	return ret;
}

static int __devexit
hx8357_remove(struct device *dev)
{
	struct hx8357_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	lcdfb_device_unregister(&drvdata->fb);

#ifdef HWMEM_ALLOC
	hwmem_free(drvdata->cmds_list);
#else
	dma_free_coherent(NULL, drvdata->cmds_list_max_size, drvdata->cmds_list, 
			(dma_addr_t)drvdata->cmds_list_phys);
#endif

	kfree(drvdata);

	return 0;
}

static struct device_driver hx8357_driver = {
	.owner  = THIS_MODULE,
	.name   = HX8357_NAME,
	.bus    = &lcdctrl_bustype,
	.probe  = hx8357_probe,
	.remove = hx8357_remove,
};

static int __init
hx8357_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	return driver_register(&hx8357_driver);
}

static void __exit
hx8357_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	driver_unregister(&hx8357_driver);
}

module_init(hx8357_init);
module_exit(hx8357_exit);

MODULE_AUTHOR("ACER");
MODULE_DESCRIPTION("LCDBUS driver for the hx8357 LCD");
