/*
 *  linux/drivers/video/pnx/displays/ili9325.c
 *  Change from jbt6k71-as-c.c by Ethan
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

#include "ili9325.h"

#define MODULE_NAME "ili9325"
/*
 * NOTE	the LCD IC is associated with VDE LCD0, means /dev/fb0
 *			(see ili9325_device_supported() function)
 * TODO undapte rotation for -90 and +90 degrees
 * TODO x panning has not been tested (but problably less important
 *      than y panning)
 *
 * TODO Manage all ACTIVATE mode in ili9325_set_par
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
static struct lcdfb_specific_config ili9325_specific_config = {
	.explicit_refresh = 1,		//Add by Ethan follow DVL7 change
	.boot_power_mode = FB_BLANK_UNBLANK,
};


/* Splash screen management
 * note: The splash screen could be png files or raw files */
#ifdef CONFIG_FB_LCDBUS_ILI9325_KERNEL_SPLASH_SCREEN
#include "ili9325_splash.h"
static struct lcdfb_splash_info ili9325_splash_info = {
	.images      = 1,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = ili9325_splash_data, /* Image data, NULL for nothing */
	.data_size   = sizeof(ili9325_splash_data),
};
#else
/* No animation parameters */
static struct lcdfb_splash_info ili9325_splash_info = {
	.images      = 0,    /* How many images */
	.loop        = 0,    /* 1 for animation loop, 0 for no animation */
	.speed_ms    = 0,    /* Animation speed in ms */
	.data        = NULL, /* Image data, NULL for nothing */
	.data_size   = 0,
};
#endif

/* FB_FIX_SCREENINFO (see fb.h) */
static struct fb_fix_screeninfo ili9325_fix = {
	.id          = ILI9325_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.xpanstep    = 0,
#if (ILI9325_SCREEN_BUFFERS == 0)
	.ypanstep    = 0, /* no panning */
#else
	.ypanstep    = 1, /* y panning available */
#endif
	.ywrapstep   = 0,
	.accel       = FB_ACCEL_NONE,
	.line_length = ILI9325_SCREEN_WIDTH * (ILI9325_FB_BPP/8),
};

/* FB_VAR_SCREENINFO (see fb.h) */
static struct fb_var_screeninfo ili9325_var = {
	.xres           = ILI9325_SCREEN_WIDTH,
	.yres           = ILI9325_SCREEN_HEIGHT,
	.xres_virtual   = ILI9325_SCREEN_WIDTH,
	.yres_virtual   = ILI9325_SCREEN_HEIGHT * (ILI9325_SCREEN_BUFFERS + 1),
	.xoffset        = 0,
	.yoffset        = 0,
	.bits_per_pixel = ILI9325_FB_BPP,

#if (ILI9325_FB_BPP == 32)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (ILI9325_FB_BPP == 24)
	.red            = {16, 8, 0},
	.green          = {8, 8, 0},
	.blue           = {0, 8, 0},

#elif (ILI9325_FB_BPP == 16)
	.red            = {11, 5, 0},
	.green          = {5, 6, 0},
	.blue           = {0, 5, 0},

#else
	#error "Unsupported color depth (see driver doc)"
#endif

	.vmode          = FB_VMODE_NONINTERLACED,
	.height         = 60,
	.width          = 45,
	.rotate         = FB_ROTATE_UR,
};

/* Hw LCD timings */
static struct lcdbus_timing ili9325_timing = {
	/* bus */
	.mode = 0, /* VDE_CONF_MODE_16_BIT_PARALLEL_INTERFACE */
	.ser  = 0, /* serial mode is not used*/
	.hold = 0, /* VDE_CONF_HOLD_A0_FOR_COMMAND */
	/* read */
	.rh = 7,
	.rc = 4,
	.rs = 0,
	/* write */
	.wh = 4, /* 2 */
	.wc = 6, /* 3 */
	.ws = 3,
	/* misc */
	.ch = 0,
	.cs = 0,
};

/* ----------------------------------------------------------------------- */

/* cached register values of the ILI9325 display */
//static u16 ili9325_register_values[ILI9325_REGISTERMAP_SIZE];	//Remove by Ethan Tsai 09/06/16

/* ILI9325 driver data */
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
struct ili9325_drvdata {

	struct lcdfb_device fb;

	struct lcdbus_cmd *cmds_list;
	u32    cmds_list_phys;
	u32    cmds_list_max_size;
	struct lcdbus_cmd *curr_cmd;
	struct lcdbus_cmd *last_cmd;
	struct mutex lock;
	u16 power_mode;
	u16 byte_shift;
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
/* ili9325_show_explicit_refresh
 *
 */ 
static ssize_t 
ili9325_show_explicit_refresh(struct device *device, 
						   struct device_attribute *attr,
						   char *buf)
{
	return sprintf(buf, "%d\n", ili9325_specific_config.explicit_refresh);
}


/* ili9325_store_explicit_refresh
 *
 */ 
static ssize_t 
ili9325_store_explicit_refresh(struct device *device,
							struct device_attribute *attr,
							const char *buf,
							size_t count)
{
	if (strncasecmp(buf, "0", count-1) == 0) {
		ili9325_specific_config.explicit_refresh = 0;
	}
	else if (strncasecmp(buf, "1", count-1) == 0) {
		ili9325_specific_config.explicit_refresh = 1;
	}

	return count;
}


static struct device_attribute ili9325_device_attrs[] = {
	__ATTR(explicit_refresh, S_IRUGO|S_IWUSR, ili9325_show_explicit_refresh, 
            ili9325_store_explicit_refresh)
};

/*
 =========================================================================
 =                                                                       =
 =              Helper functions                                         =
 =                                                                       =
 =========================================================================
*/


/**
 * ili9325_set_bus_config - Sets the bus (VDE) config
 *
 * Configure the VDE colors format according to the LCD
 * colors formats (conversion)
 */
static int 
ili9325_set_bus_config(struct device *dev)
{
    struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct lcdbus_conf busconf;
	int ret = 0;

	/* Tearing management */
	busconf.eofi_del  = 0;
	busconf.eofi_skip = 0;
	busconf.eofi_pol  = 0;

	/* CSKIP & BSWAP params */
	busconf.cskip = 0;
	busconf.bswap = 0;

	/* Data & cmd params */
	busconf.cmd_ifmt  = LCDBUS_INPUT_DATAFMT_TRANSP;
	busconf.cmd_ofmt  = LCDBUS_OUTPUT_DATAFMT_TRANSP_16_BITS;

	switch (ili9325_var.bits_per_pixel) {

	// Case 1: 16BPP
	case 16:
		busconf.data_ifmt = LCDBUS_INPUT_DATAFMT_RGB565;
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
					ili9325_var.bits_per_pixel);
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
 * ili9325_set_gpio_config - Sets the gpio config
 *
 */
static int 
//ili9325_set_gpio_config(struct device *dev, struct ili9325_drvdata *drvdata)
ili9325_set_gpio_config(struct device *dev)
{
	/* struct ili9325_drvdata *drvdata = dev_get_drvdata(dev); */
	int ret=0;

	/* EOFI pin is connected to the GPIOB7 */
	gpio_request(GPIO_B7,MODULE_NAME);
	pnx_gpio_set_mode(GPIO_B7, GPIO_MODE_MUX0);
	gpio_direction_input(GPIO_B7); 
	// TODO: Check the Pull Up/Down mode

	/* Return the error code */
	return ret;
}


/**
 * ili9325_check_cmd_list_overflow
 * 
 */
#define ili9325_check_cmd_list_overflow()                                      \
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
 * ili9325_free_cmd_list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 *
 * This function removes and free's all lcdbus_cmd's from cmds.
 */
static void
ili9325_free_cmd_list(struct device *dev, struct list_head *cmds)
{
	struct lcdbus_cmd *cmd, *tmp;
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);


		list_for_each_entry_safe(cmd, tmp, cmds, link) {
			list_del(&cmd->link);
		}		





	/* Reset the first command pointer (position) */
	drvdata->curr_cmd = drvdata->cmds_list;
}

/**
 * ili9325_add_ctrl_cmd
 * cmds: commandes list
 * @dev: device which has been used to call this function
 * @reg: the register that is addressed
 * @value: the value that should be copied into the register
 *
 * This function adds the given command to the internal command chain
 * of the LCD driver.
 */
static int ili9325_add_ctrl_cmd(struct device *dev,struct list_head *cmds,const u16 reg, u16 value)
{
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);

	//struct lcdbus_cmd *cmd;
	pr_debug("%s()\n", __FUNCTION__);

	ili9325_check_cmd_list_overflow();

	drvdata->curr_cmd->cmd = LOBYTE(reg);
	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_CMD;
	drvdata->curr_cmd->data_phys = drvdata->curr_cmd->data_int_phys;
	drvdata->curr_cmd->data_int[1] = HIBYTE(value);
	drvdata->curr_cmd->data_int[0] = LOBYTE(value);
	/* FIXME dirty hack as reg 0x0000 seems to have no arguments when
	 * using it for sync */
	drvdata->curr_cmd->len = 2;
	list_add_tail(&drvdata->curr_cmd->link, cmds);
	drvdata->curr_cmd ++; /* Next command */
	return 0;
}

/**
 * ili9325_add_data_cmd - add a request to the internal command chain
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
ili9325_add_data_cmd(struct device *dev,struct list_head *cmds,struct lcdfb_transfer *transfer)
{
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);

	pr_debug("%s()\n", __FUNCTION__);

	ili9325_check_cmd_list_overflow();

	drvdata->curr_cmd->type = LCDBUS_CMDTYPE_DATA;
	drvdata->curr_cmd->cmd  = 0x22;//accesss GRAM);
	drvdata->curr_cmd->w = transfer->w;
	drvdata->curr_cmd->h = transfer->h;
	drvdata->curr_cmd->bytes_per_pixel = ili9325_var.bits_per_pixel >> 3;
	drvdata->curr_cmd->stride = ili9325_fix.line_length;
	drvdata->curr_cmd->data_phys = transfer->addr_phys + 
			transfer->x * drvdata->curr_cmd->bytes_per_pixel + 
			transfer->y * drvdata->curr_cmd->stride;		
	
	list_add_tail(&drvdata->curr_cmd->link, cmds);

	drvdata->curr_cmd ++; /* Next command */
	return 0;
}
/**
 * ili9325_execute_cmds - execute the internal command chain
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
ili9325_execute_cmds(struct device *dev, struct list_head *cmds, u16 delay)
{
	int ret;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;

	pr_debug("%s()\n", __FUNCTION__);

	/* Write data to the bus */
	ret = bus->write(dev, cmds);
	ili9325_free_cmd_list(dev, cmds);

	/* Need to wait ? */
	if (delay != 0)
		mdelay(delay);

	/* Return the error code */
	return ret;
}
/**
 * ili9325_add_transfer_cmd - adds lcdbus_cmd's from a transfer to list
 * @dev: device which has been used to call this function
 * @cmds: a LLI list of lcdbus_cmd's
 * @transfer: the lcdfb_transfer to be converted
 *
 * This function creates a command list for a given transfer. Therefore it
 * selects the x- and y-start position in the display ram, issues the write-ram
 * command and sends the data.
 */
static int
ili9325_add_transfer_cmd(struct device *dev,
	                     struct list_head *cmds,
	                     struct lcdfb_transfer *transfer)
{
	//u16 rotation;
	u16 l, t, r, b;

	pr_debug("%s()\n", __FUNCTION__);

	l = transfer->x;
	t = transfer->y;
	r = transfer->x + transfer->w - 1;
	b = transfer->y + transfer->h - 1;

	/*Add by Ethan Tsai@20090813 for partial display*/
	
	ili9325_add_ctrl_cmd(dev, cmds, 0x0050, l);		//H-Start
	ili9325_add_ctrl_cmd(dev, cmds, 0x0052, t);		//V-Start

	ili9325_add_ctrl_cmd(dev, cmds, 0x0051, r); 		//H-End
	ili9325_add_ctrl_cmd(dev, cmds, 0x0053, b);		//V-End

	ili9325_add_ctrl_cmd(dev, cmds, 0x0020, l);
	ili9325_add_ctrl_cmd(dev, cmds, 0x0021, t);
	
	ili9325_execute_cmds(dev, cmds, 0); 

	ili9325_add_data_cmd(dev, cmds, transfer);
	ili9325_execute_cmds(dev, cmds, 0); 			//modified by wesley, 2009.09.04
	return 0;
}



/*
 * ili9325_switchon - execute the switch on commands list
 * @dev: device which has been used to call this function
 *
 */

static int
ili9325_switchon(struct device *dev)
{
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);
	//struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	//struct lcdbus_ops *bus = ldev->ops;	
	int ret = 0;
	struct list_head cmds;

	pr_debug("%s()\n", __FUNCTION__);

	/* ------------------------------------------------------------------------
	 * Set the bus configuration for the display
	 * --------------------------------------------------------------------- */
	ret = ili9325_set_bus_config(dev);
	if (ret < 0) {
		dev_err(dev, "Could not set bus config\n");
		return -EBUSY;
	}

	/* ------------------------------------------------------------------------
	 * Initialize the LCD HW
	 * --------------------------------------------------------------------- */
	
	INIT_LIST_HEAD(&cmds);

#ifndef ACER_K2_PR1
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0001, 0x0100);    // set SS and SM bit
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0002, 0x0700);    // set 1 line inversion
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0003, 0x1030);    // set GRAM write direction and BGR=1.
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0008, 0x0207);    // set the back porch and front porch
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0010, 0x1190);    // Power Control 2 (R11)
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0011, 0x0227);    // DC1[2:0], DC0[2:0], VC[2:0]
	ili9325_execute_cmds(dev, &cmds, 50);                // Delay 50ms
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0012, 0x009D);    // VREG1OUT voltage
	ili9325_execute_cmds(dev, &cmds, 50);                // Delay 50ms
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0013, 0x1600);    // VDV[4:0] for VCOM amplitude

 	ili9325_add_ctrl_cmd(dev, &cmds, 0x0020, 0x0000);    // GRAM horizontal Address
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0021, 0x0000);    // GRAM Vertical Address
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0029, 0x001A);    // VCM[4:0] for VCOMH      Delayms(50);  29
	ili9325_add_ctrl_cmd(dev, &cmds, 0x002B, 0x000B);    // Frane rate setting

	ili9325_add_ctrl_cmd(dev, &cmds, 0x0030, 0x0000);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0031, 0x0707);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0032, 0x0407);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0035, 0x0407);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0036, 0x0404);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0037, 0x0003);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0038, 0x0000);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0039, 0x0707);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x003C, 0x0704);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x003D, 0x0006);

	ili9325_add_ctrl_cmd(dev, &cmds, 0x0050, 0x0000);    // Horizontal GRAM Start Address
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0051, 0x00EF);    // Horizontal GRAM End Address
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0052, 0x0000);    // Vertical GRAM Start Address
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0053, 0x013F);    // Vertical GRAM End Address

	ili9325_add_ctrl_cmd(dev, &cmds, 0x0060, 0xA700);    // Gate Scan Line
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0061, 0x0001);    // NDL,VLE, REV

	ili9325_add_ctrl_cmd(dev, &cmds, 0x0007, 0x0133);    // 262K color and display ON
	ili9325_execute_cmds(dev, &cmds, 1);                 // Delay 1ms

	ili9325_add_ctrl_cmd(dev, &cmds, 0x00A2, 0x0001);
#else
//========= Reset LCD Driver =======//

	ili9325_add_ctrl_cmd(dev, &cmds, 0x00E3, 0x3008);    // Set internal timing       
	ili9325_add_ctrl_cmd(dev, &cmds, 0x00E7, 0x0012);    // Set internal timing                
	ili9325_add_ctrl_cmd(dev, &cmds, 0x00EF, 0x1231);    // Set internal timing                
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0001, 0x0100);    // set SS and SM bit                  
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0002, 0x0700);    // set 1 line inversion               
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0003, 0x1030);    // set GRAM write direction and BGR=1.
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0004, 0x0000);    // Resize register                    
	
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0008, 0x0207);    // set the back porch and front porch 
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0009, 0x0000);    // set non-display area refresh cycle 
	                                                                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x000A, 0x0008);    // FMARK function                     
	ili9325_add_ctrl_cmd(dev, &cmds, 0x000C, 0x0000);    // RGB interface setting       //Caption       
	ili9325_add_ctrl_cmd(dev, &cmds, 0x000D, 0x0000);    // Frame marker Position              
	ili9325_add_ctrl_cmd(dev, &cmds, 0x000F, 0x0000);    // RGB interface polarity             


	//======Power On sequence =====//	
#if 0
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0010, 0x0000);    // SAP, BT[3:0], AP, DSTB, SLP, STB 
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0011, 0x0007);    // DC1[2:0], DC0[2:0], VC[2:0]      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0012, 0x0000);    // VREG1OUT voltage                 
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0013, 0x0000);    // VDV[4:0] for VCOM amplitude      
	ili9325_execute_cmds(dev, &cmds, 50); 
#endif
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0010, 0x1590);    // SAP, BT[3:0], AP, DSTB, SLP, STB DDVDH=2.0XVCI1=5.6V VGH=5*VCI1=14V VGL=3X2.8=-8.4V
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0011, 0x0225);    // setting vci1= 0.7xVCI(2.8)=1.96V                                                   
	ili9325_execute_cmds(dev, &cmds, 20);          
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0011, 0x0227);    // setting vci1= 1.0xVCI(2.8)=2.8V                                                    
	ili9325_execute_cmds(dev, &cmds, 50);		// Delay 50ms
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0012, 0x009E);	// VREG1OUT =2.5X1.9=4.75V
	ili9325_execute_cmds(dev, &cmds, 50);		// Delay 50ms
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0013, 0x1700);    // VDV[4:0] for VCOM amplitude //0x1a  VCOMAC=1.08X4.75=5.13V 
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0029, 0x0026);    // VCM[4:0] for VCOMH      Delayms(50);  29          
	
 	ili9325_add_ctrl_cmd(dev, &cmds, 0x0020, 0x0000);    // GRAM horizontal Address                                    
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0021, 0x0000);    // GRAM Vertical Address                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x002B, 0x000C);    // Frane rate setting                                         

// ====== Gamma  Curve ======//	
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0030, 0x0103);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0031, 0x0207);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0032, 0x0102);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0035, 0x0302);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0036, 0x000A);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0037, 0x0506);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0038, 0x0005);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0039, 0x0406);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x003C, 0x0203);
	ili9325_add_ctrl_cmd(dev, &cmds, 0x003D, 0x0C04);

//======== Set GRAM area =======//	
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0050, 0x0000);    // Horizontal GRAM Start Address  
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0051, 0x00EF);    // Horizontal GRAM End Address    
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0052, 0x0000);    // Vertical GRAM Start Address    
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0053, 0x013F);    // Vertical GRAM End Address    
	                                                                                  
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0060, 0xA700);    // Gate Scan Line                 
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0061, 0x0001);    // NDL,VLE, REV                   
	ili9325_add_ctrl_cmd(dev, &cmds, 0x006A, 0x0000);    // set scrolling line             
	                                                                                  
//====== Partial Display  ======//	                                                  
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0080, 0x0060);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0081, 0x0060);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0082, 0x00E0);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0083, 0x0000);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0084, 0x0000);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0085, 0x0000);                                      
	                                                                                  
//====== Panel Control =========//	                                                  
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0090, 0x0010);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0092, 0x0000);                                      
	ili9325_add_ctrl_cmd(dev, &cmds, 0x0007, 0x0133);    // 262K color and display ON      


	ili9325_execute_cmds(dev, &cmds, 1);		// Delay 1ms
#endif

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
 * ili9325_device_supported - perform hardware detection check
 * @dev:	pointer to the device which should be checked for support
 *
 */
static int __devinit
ili9325_device_supported(struct device *dev)
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
 * ili9325_write - implementation of the write function call
 * @dev:	device which has been used to call this function
 * @transfers:	list of lcdfb_transfer's
 *
 * This function converts the list of lcdfb_transfer into a list of resulting
 * lcdbus_cmd's which then gets sent to the display controller using the
 * underlying bus driver.
 */
static int
ili9325_write(const struct device *dev, const struct list_head *transfers)
{
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev->parent);
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


	/* now get on with the real stuff */
	INIT_LIST_HEAD(&cmds);

	/* moved out of the loop for performance improvements */
	drvdata->byte_shift = (ili9325_var.bits_per_pixel >> 3) - 1;
	list_for_each_entry(transfer, transfers, link) {
		ret |= ili9325_add_transfer_cmd(dev->parent, &cmds, transfer);
	}
//modified by wesley,2009.09.04
#if 0
		if (ret >= 0) {
			/* execute the cmd list we build */
			ret = bus->write(dev->parent, &cmds);
		}		
	/* Now buffers may be freed by the application */
	ili9325_free_cmd_list(dev->parent, &cmds);
#endif

	/* unlock hardware access */
	mutex_unlock(&drvdata->lock);


	/* Return the error code */
	return ret;
}

/**
 * ili9325_get_fscreeninfo - copies the fix screeninfo into fsi
 * @dev:	device which has been used to call this function
 * @fsi:	structure to which the fix screeninfo should be copied
 *
 * Get the fixed information of the screen.
 */
static int
ili9325_get_fscreeninfo(const struct device *dev,
						struct fb_fix_screeninfo *fsi)
{
	BUG_ON(!fsi);

	pr_debug("%s()\n", __FUNCTION__);

	*fsi = ili9325_fix;
	/* Return the error code */
	return 0;
}

/**
 * ili9325_get_vscreeninfo - copies the var screeninfo into vsi
 * @dev:	device which has been used to call this function
 * @vsi:	structure to which the var screeninfo should be copied
 *
 * Get the variable screen information.
 */
static int
ili9325_get_vscreeninfo(const struct device *dev,
						struct fb_var_screeninfo *vsi)
{
	BUG_ON(!vsi);

	pr_debug("%s()\n", __FUNCTION__);

	*vsi = ili9325_var;
	/* Return the error code */
	return 0;
}

/**
 * ili9325_display_on - execute "Display On" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display on.
 */
static int
ili9325_display_on(struct device *dev)
{
	int ret = 0;
	struct ili9325_drvdata *drvdata = dev_get_drvdata((struct device *)dev->parent);
	/* Switch on the display if needed */
	if (drvdata->power_mode != FB_BLANK_UNBLANK) {
		ret = ili9325_switchon(dev->parent);
	}
	else {
		dev_err(dev, "Display already in FB_BLANK_UNBLANK mode\n");
		ret = -EPERM; /* Operation not permitted */
	}

	/* Return the error code */
	return ret;
}

/**
 * ili9325_display_off - execute "Display Off" sequence
 * @dev: device which has been used to call this function
 *
 * This function switches the display off.
 */
static int
ili9325_display_off(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;			//Ethan 09/06/17
	struct device *devp = dev->parent;
	struct ili9325_drvdata *drvdata = dev_get_drvdata(devp);

	pr_debug("%s()\n", __FUNCTION__);

	/* Switch off the display if needed */
	if (drvdata->power_mode != FB_BLANK_POWERDOWN) {

		INIT_LIST_HEAD(&cmds);
		/*Add by Ethan Tsai@20090812 for display off*/
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0007, 0x0131);
		ili9325_execute_cmds(devp, &cmds, 10);
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0007, 0x0130);
		ili9325_execute_cmds(devp, &cmds, 10);
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0007, 0x0000);
		ili9325_execute_cmds(devp, &cmds, 1);
		//============Power OFF sequence=========
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0010, 0x0000);
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0011, 0x0000);
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0012, 0x0000);
		ili9325_add_ctrl_cmd(devp, &cmds, 0x0013, 0x0000);
		ili9325_execute_cmds(devp, &cmds, 200);
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
 * ili9325_display_standby - enter standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from normal mode
 * to standby mode.
 */
static int
ili9325_display_standby(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;			//Ethan 09/06/17
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);

	if (drvdata->power_mode != FB_BLANK_VSYNC_SUSPEND) {
		INIT_LIST_HEAD(&cmds);
		/*Add by Ethan Tsai@20090812 for display standby*/
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0131);
		ili9325_execute_cmds(dev->parent, &cmds, 10);	
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0130);
		ili9325_execute_cmds(dev->parent, &cmds, 10);	
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0000);	//Display OFF
		ili9325_execute_cmds(dev->parent, &cmds, 1);

		//===== Power OFF sequence =====//
		/*Add by Ethan Tsai@20090820 for display standby*/
		ili9325_add_ctrl_cmd(dev->parent,&cmds, 0x0010, 0x0080);	// SAP, BT[3:0], APE, AP, DSTB, SLP 
		ili9325_add_ctrl_cmd(dev->parent,&cmds, 0x0011, 0x0000);	// DC1[2:0], DC0[2:0], VC[2:0]
		ili9325_add_ctrl_cmd(dev->parent,&cmds, 0x0012, 0x0000);	// VREG1OUT voltage
		ili9325_add_ctrl_cmd(dev->parent,&cmds, 0x0013, 0x0000);	// VDV[4:0] for VCOM amplitude
		ili9325_execute_cmds(dev->parent, &cmds, 200);			// Dis-charge capacitor power voltage
		ili9325_add_ctrl_cmd(dev->parent,&cmds, 0x0010, 0x0082);	// SAP, BT[3:0], APE, AP, DSTB, SLP
		ili9325_execute_cmds(dev->parent, &cmds, 1);
	
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
 * ili9325_display_deep_standby - enter deep standby mode
 * @dev: device which has been used to call this function
 *
 * This function switches the display from standby mode to
 * deep standby mode.
 */
static int
ili9325_display_deep_standby(struct device *dev)
{
	int ret = 0;
	struct list_head cmds;											//Ethan 09/06/17
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pr_debug("%s()\n", __FUNCTION__);
	if (drvdata->power_mode != FB_BLANK_VSYNC_SUSPEND) {
		INIT_LIST_HEAD(&cmds);
		/*Add by Ethan Tsai@20090812 for deep standby*/
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0131);
		ili9325_execute_cmds(dev->parent, &cmds, 10);	
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0130);
		ili9325_execute_cmds(dev->parent, &cmds, 10);	
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0007, 0x0000);
		ili9325_execute_cmds(dev->parent, &cmds, 1);	
		//============Power OFF sequence=========
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0010, 0x0080);
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0011, 0x0000);
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0012, 0x0000);
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0013, 0x0000);
		ili9325_execute_cmds(dev->parent, &cmds, 200);
		ili9325_add_ctrl_cmd(dev->parent, &cmds, 0x0010, 0x0082);
		ili9325_execute_cmds(dev->parent, &cmds, 1);	
		
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
 * ili9325_get_splash_info - copies the splash screen info into si
 * @dev:	device which has been used to call this function
 * @si:		structure to which the splash screen info should be copied
 *
 * Get the splash screen info.
 * */
static int
ili9325_get_splash_info(struct device *dev,
		struct lcdfb_splash_info *si)
{
	BUG_ON(!si);

	pr_debug("%s()\n", __FUNCTION__);

	*si = ili9325_splash_info;

	/* Return the error code */
	return 0;
}


/**
 * ili9325_get_specific_config - return the explicit_refresh configuration
 * @dev:	device which has been used to call this function
 *
 * */
static int
ili9325_get_specific_config(struct device *dev,
		struct lcdfb_specific_config **sc)
{
	BUG_ON(!sc);

	pr_debug("%s()\n", __FUNCTION__);

	(*sc) = &ili9325_specific_config;

	/* Return the error code */
	return 0;
}
/**
 * ili9325_get_device_attrs
 * @dev: device which has been used to call this function
 * @device_attrs: device attributes to be returned
 * @attrs_nbr: the number of device attributes
 * 
 *
 * Returns the device attributes to be added to the SYSFS
 * entries (/sys/class/graphics/fbX/....
 * */
static int 
ili9325_get_device_attrs(struct device *dev, 
		struct device_attribute **device_attrs,
		unsigned int *attrs_nbr)
{
	
	BUG_ON(!device_attrs);

	pr_debug("%s()\n", __FUNCTION__);

	(*device_attrs) = ili9325_device_attrs;

	(*attrs_nbr) = sizeof(ili9325_device_attrs)/sizeof(ili9325_device_attrs[0]);

	/* Return the error code */
	return 0;
}


/*
 * ili9325_check_var
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to check
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static int ili9325_check_var(const struct device *dev,
		 struct fb_var_screeninfo *vsi)
{
	int ret = -EINVAL;

	BUG_ON(!vsi);

	pr_debug("%s()\n", __FUNCTION__);

	/* check xyres */
	if ((vsi->xres != ili9325_var.xres) ||
		(vsi->yres != ili9325_var.yres)) {
		ret = -EPERM;	
		goto ko;
	}

	/* check xyres virtual */
	if ((vsi->xres_virtual != ili9325_var.xres_virtual) ||
		(vsi->yres_virtual != ili9325_var.yres_virtual)) {
		ret = -EPERM;
		goto ko;
	}

	/* check xoffset */
	if (vsi->xoffset != ili9325_var.xoffset) {
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
 * ili9325_set_par
 * @dev:	device which has been used to call this function
 * @vsi:	structure  var screeninfo to set
 *
 * TODO check more parameters and cross cases (virtual xyres vs. rotation,
 *      panning...
 *
 * */
static inline void ili9325_init_color(struct fb_bitfield *color, 
		__u32 offset, __u32 length, __u32 msb_right)
{
	color->offset     = offset;
	color->length     = length;
	color->msb_right  = msb_right;	
}

static int ili9325_set_par(const struct device *dev,
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
		if (! memcmp(vsi, &ili9325_var, sizeof(struct fb_var_screeninfo))) {
			ret = 0;
			goto quit;
		}

		/* Check rotation */
		if (ili9325_var.rotate != vsi->rotate) {
			ili9325_var.rotate = vsi->rotate;
		}

		/* Check bpp */
		if (ili9325_var.bits_per_pixel != vsi->bits_per_pixel) {

			u32 bytes_per_pixel;
			
			ili9325_var.bits_per_pixel = vsi->bits_per_pixel;

			switch(ili9325_var.bits_per_pixel) {
			case 16:
				ili9325_init_color(&ili9325_var.red,  11, 5, 0);
				ili9325_init_color(&ili9325_var.green, 5, 6, 0);
				ili9325_init_color(&ili9325_var.blue,  0, 5, 0);
				break;

			case 24:
			case 32:
				ili9325_init_color(&ili9325_var.red,   16, 8, 0);
				ili9325_init_color(&ili9325_var.green,  8, 8, 0);
				ili9325_init_color(&ili9325_var.blue,   0, 8, 0);
				break;
			}

			bytes_per_pixel = ili9325_var.bits_per_pixel/8;
		
			/* Set the new line length (BPP or resolution changed) */
			ili9325_fix.line_length = ili9325_var.xres_virtual*bytes_per_pixel;

			/* Switch off the LCD HW */
			ili9325_display_off((struct device *)dev);

			/* Switch on the LCD HW */
			ret = ili9325_display_on((struct device *)dev);
		}

		/* TODO: Check other parameters */

		ret = 0;
	}

quit:	
	/* Return the error code */
	return ret;
}


/**
 * ili9325_ioctl
 * @dev: device which has been used to call this function
 * @cmd: requested command
 * @arg: command argument
 * */
static int
ili9325_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int value;
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev->parent);

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
struct lcdfb_ops ili9325_ops = {
	.write           = ili9325_write,
	.get_fscreeninfo = ili9325_get_fscreeninfo,
	.get_vscreeninfo = ili9325_get_vscreeninfo,
	.get_splash_info = ili9325_get_splash_info,
	.get_specific_config = ili9325_get_specific_config,
	.get_device_attrs= ili9325_get_device_attrs,
	.display_on      = ili9325_display_on,
	.display_off     = ili9325_display_off,
	.display_standby = ili9325_display_standby,
	.display_deep_standby = ili9325_display_deep_standby,	
	.check_var       = ili9325_check_var,
	.set_par         = ili9325_set_par,
	.ioctl           = ili9325_ioctl,
};


/*
 =========================================================================
 =                                                                       =
 =              module stuff (init, exit, probe, release)                =
 =                                                                       =
 =========================================================================
*/

static int __devinit ili9325_probe(struct device *dev)

{
	struct ili9325_drvdata *drvdata;
	struct lcdctrl_device *ldev = to_lcdctrl_device(dev);
	struct lcdbus_ops *bus = ldev->ops;
	struct lcdbus_cmd *cmd;
	u32 cmds_list_phys;
	int ret, i;

	pr_debug("%s()\n", __FUNCTION__);

	BUG_ON(!bus || !bus->read || !bus->write || !bus->get_conf ||
	       !bus->set_conf || !bus->set_timing);

	/* --------------------------------------------------------------------
	 * Hardware detection
	 * ---- */
	ret = ili9325_device_supported(dev);
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

	pr_debug("%s (cmd %p, curr %p, last %p)\n", 
			__FUNCTION__, drvdata->cmds_list, drvdata->curr_cmd, 
			drvdata->last_cmd); 

	/* Set drvdata */
	dev_set_drvdata(dev, drvdata);

	drvdata->power_mode = ili9325_specific_config.boot_power_mode;
	drvdata->fb.ops = &ili9325_ops;
	drvdata->fb.dev.parent = dev;
	snprintf(drvdata->fb.dev.bus_id, BUS_ID_SIZE, "%s-fb", dev->bus_id);

	/* --------------------------------------------------------------------
	 * Set display timings before anything else happens
	 * ---- */
	bus->set_timing(dev, &ili9325_timing);

	if (ILI9325_262K_COLORS == 0) {
		drvdata->use_262k_colors = 1;
	}


	/* initialize mutex to lock hardware access */
	mutex_init(&(drvdata->lock));


	/* --------------------------------------------------------------------
	 * GPIO configuration is specific for each LCD
	 * (Chech the HW datasheet (VDE_EOFI, VDE_EOFI_copy)
	 * ---- */
	ret = ili9325_set_gpio_config(dev);
	if (ret < 0) {
		dev_err(dev, "Could not set gpio config\n");
		return -EBUSY;
	}

	/* --------------------------------------------------------------------
	 * Initialize the LCD if the initial state is ON (FB_BLANK_UNBLANK)
	 * ---- */
	if (drvdata->power_mode == FB_BLANK_UNBLANK) {
            //Selwyn 2009/1/20 modified for Acer power off charging
	    #ifdef DISABLE_CHARGING_ANIMATION
            if(!strstr(saved_command_line, "androidboot.mode=2"))
            #endif
            //~Selwyn 2009/1/20 modified
            {
		ret = ili9325_switchon(dev);
		if (ret < 0) {
			ret = -EBUSY;
			goto err_free_cmds_list;
		}
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
ili9325_remove(struct device *dev)
{
	struct ili9325_drvdata *drvdata = dev_get_drvdata(dev);

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

static struct device_driver ili9325_driver = {
	.owner  = THIS_MODULE,
	.name   = ILI9325_NAME,
	.bus    = &lcdctrl_bustype,
	.probe  = ili9325_probe,
	.remove = ili9325_remove,
};

static int __init
ili9325_init(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	return driver_register(&ili9325_driver);
}

static void __exit
ili9325_exit(void)
{
	pr_debug("%s()\n", __FUNCTION__);

	driver_unregister(&ili9325_driver);
}

module_init(ili9325_init);
module_exit(ili9325_exit);

MODULE_AUTHOR("ACER");
MODULE_DESCRIPTION("LCDBUS driver for the ILI9325 LCD");
