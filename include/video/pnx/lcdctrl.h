/*
 * linux/include/video/pnx/lcdctrl.h
 *
 * LCD ctrl driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __LCDCTRL_H
#define __LCDCTRL_H

#include <linux/device.h>
#include <linux/list.h>

struct fb_fix_screeninfo;
struct fb_var_screeninfo;
struct fb_info;

extern struct bus_type lcdfb_bustype;

/**
 * struct lcdfb_transfer - describes a chunk of data to be sent to the display
 * @link: used to add this transfer to a list
 *
 * @x: top-left x coordinate of the display area to update.
 * @y: top-left y coordinate of the display area to update.
 * @w: width of the display area to update.
 * @h: height of the display area to update.
 *
 * @addr_logical: pointer to the data (logical address)
 * @addr_phys: pointer to the data (physical address)
 *
 * This structure is passed between the lcdfb and lcdctrl drivers. It uses the
 * Linux LLI to enabled transfer chaining.
 */
struct lcdfb_transfer {
	struct list_head link;
	u16 x;
	u16 y;
	u16 w;
	u16 h;
	void *addr_logical;
	u32   addr_phys;
} ;


/*
 * struct lcdfb_specific_config
 * @explicit_refresh: 1 to activate the explicit refresh
 * @boot_power_mode: display power mode during the boot (see FB_BLANK_)
 *
 * */
struct lcdfb_specific_config {
	int explicit_refresh;
	int boot_power_mode;
};


/*
 * struct lcdfb_splash_info
 * @images:    How many images
 * @loop:      1 for animation loop, 0 for no animation
 * @speed_ms:  Animation speed in ms
 * @data:      Image data, NULL for nothing
 * @data_size: Size of the data buffer in bytes
 *
 * * */
struct lcdfb_splash_info {
	int images;
	int loop;
	int speed_ms;
	u8 *data;
	u32 data_size;
};

/**
 * struct lcdfb_ops - the function call interface to the lcdctrl driver
 */
struct lcdfb_ops {
	int (*write)			(const struct device *dev,
							const struct list_head *transfers);

	int (*get_fscreeninfo)	(const struct device *dev,
							 struct fb_fix_screeninfo *fsi);

	int (*get_vscreeninfo)	(const struct device *dev,
							struct fb_var_screeninfo *vsi);

	int (*check_var) 		(const struct device *dev,
							 struct fb_var_screeninfo *vsi);

	int (*set_par) 	 		(const struct device *dev,
	 						 struct fb_info *info);

	int (*display_on)		(struct device *dev);

	int (*display_off)		(struct device *dev);

	int (*display_standby)	(struct device *dev);

	int (*display_deep_standby)	(struct device *dev);

	int (*get_splash_info)	(struct device *dev,
							 struct lcdfb_splash_info *si);

	int (*get_specific_config) (struct device *dev,
							 	struct lcdfb_specific_config **sc);

	int (*ioctl)(struct device *dev, unsigned int cmd, unsigned long arg);

	int (*get_device_attrs)(struct device *dev, 
						    struct device_attribute **device_attrs,
							unsigned int *attrs_number);
};

/**
 * struct lcdfb_device
 * @ops: the lcdfb function call interface implementation
 *
 */
struct lcdfb_device {
	struct lcdfb_ops *ops;
	struct device dev;
};
#define to_lcdfb_device(_dev) container_of(_dev, struct lcdfb_device, dev)

int lcdfb_device_register(struct lcdfb_device * lcdfb_dev);
void lcdfb_device_unregister(struct lcdfb_device * lcdfb_dev);

/**
 * struct lcdctl_device
 * @disp_nr: number of display "channel" this device uses
 * @ops: the lcdbus function call interface implementation
 *
 */
struct lcdctrl_device {
	int disp_nr;
	struct lcdbus_ops *ops;
	struct device dev;
};
#define to_lcdctrl_device(_dev) container_of(_dev, struct lcdctrl_device, dev)

int lcdctrl_device_register(struct lcdctrl_device *lcdctrl_dev);
void lcdctrl_device_unregister(struct lcdctrl_device *lcdctrl_dev);

#endif /* __LCDCTRL_H */

