#define MODULE_NAME "wlanhwdrv"
#define WLANHWDRV_C

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <mach/gpio.h>
#include <linux/delay.h>

#include "wlanhwdrv.h"

MODULE_DESCRIPTION("WLAN power control driver. Controls low level hardware parts.");
MODULE_AUTHOR("Copyright (c) Foxconn 2010");
MODULE_LICENSE("GPL");

#define ACER_K3_PR2

/*ACER Ed 2010-04-14 for WIFI_3V3_EN*/
#if defined(ACER_K3_PR2) || defined(ACER_K3_PCR)
#define WLANHWDRV_GPIO_EN		GPIO_D21
#else
#define WLANHWDRV_GPIO_EN
#endif
/*ACER Ed 2010-04-14*/

// Module internal
#define WLANHWDRV_MINOR		MISC_DYNAMIC_MINOR

// Internal MACROS
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
#define WLANHWDRV_GPIO_SET(status, gpio, value)		status = pnx_write_gpio_pin(gpio, value)
#define WLANHWDRV_GPIO_REQUEST(gpio)			pnx_request_gpio(gpio)
#define WLANHWDRV_GPIO_SET_MODE(gpio, mode)		pnx_set_gpio_mode(gpio, mode);
#define WLANHWDRV_GPIO_SET_DIR(gpio, dir, value)		pnx_set_gpio_direction(gpio, direction);\
							pnx_write_gpio_pin(gpio,value);
#define WLANHWDRV_GPIO_FREE(gpio)				pnx_free_gpio(gpio)
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) */
#define WLANHWDRV_GPIO_SET(status, gpio, value)		gpio_direction_output(gpio, value);status = 0;
#define WLANHWDRV_GPIO_REQUEST(gpio)			gpio_request(gpio, "WLANHWDRV");
#define WLANHWDRV_GPIO_SET_MODE(gpio, mode)		pnx_gpio_set_mode(gpio, mode);
#define WLANHWDRV_GPIO_SET_DIR(gpio, dir, value)		if(dir==GPIO_DIR_OUTPUT)gpio_direction_output(gpio, value);\
							else gpio_direction_input(gpio);
#define WLANHWDRV_GPIO_FREE(gpio)				gpio_free(gpio)
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) */

#if 0
#define DBG(format, arg...) printk(KERN_ERR  "%s - " format "\n", __FUNCTION__, ## arg)
#else
#define DBG(format, arg...)
#endif

static int wlanhwdrv_open(struct inode *inode,
			struct file *file)
{
	DBG("Open...");
	return 0;
}

static int wlanhwdrv_close(struct inode *inode, 
			 struct file *file)
{
	DBG("Close...");
	return 0;
}

static int wlanhwdrv_ioctl(struct inode *inode,
                         struct file *file,
			 unsigned int cmd,
                  	 unsigned long arg)
{
	int res = 0;

	DBG("Command:[%d]", cmd);

	switch (cmd)
	{

#if defined(ACER_K3_PR2) || defined(ACER_K3_PCR)
		case WLANHWDRV_EN_ON:
			WLANHWDRV_GPIO_SET(res, WLANHWDRV_GPIO_EN, 1);
			mdelay(100);
			break;
		case WLANHWDRV_EN_OFF:
			WLANHWDRV_GPIO_SET(res, WLANHWDRV_GPIO_EN, 0);
			mdelay(100);
			break;
#else
		case WLANHWDRV_EN_ON:
			break;
		case WLANHWDRV_EN_OFF:
			break;
#endif
		default :
			printk(KERN_ALERT "[WLANHWDRV]ERROR: Unrecognized IOCTL cmd[%d]\n", cmd);
			res = -EINVAL;
			break;

	}
	return res;
}

static int wlanhwdrv_read(struct file *fd, char __user *buff, size_t size, loff_t *myLoff)
{
	DBG("Read...");
	return 0;
}

static int wlanhwdrv_write(struct file *fd, const char __user *buff, size_t size, loff_t *myLoff)
{
	DBG("Write...");
	return 0;
}

static struct file_operations wlanhwdrv_fops = {
    .owner    = THIS_MODULE,
    .ioctl    = wlanhwdrv_ioctl,
    .open     = wlanhwdrv_open,
    .release  = wlanhwdrv_close,
    .read     = wlanhwdrv_read,
    .write    = wlanhwdrv_write,
};

static struct miscdevice wlanhwdrv_dev = {
    .minor    = WLANHWDRV_MINOR,
    .name     = "wlanhw",
    .fops     = &wlanhwdrv_fops,
};

static int wlanhwdrv_init(void)
{
    int status=0;
    DBG("WLAN Power Control Driver Init ...");

    status = misc_register(&wlanhwdrv_dev);
    if (status) {
	printk(KERN_ALERT "[WLANHWDRV]ERROR %d: Registering driver\n", status);
	goto end;
    }

#if defined(ACER_K3_PR2) || defined(ACER_K3_PCR)
    /* Request GPIO for futur use */
    status = WLANHWDRV_GPIO_REQUEST(WLANHWDRV_GPIO_EN);
    if (status) {
        printk(KERN_ALERT "[WLANHWDRV]ERROR %d: Request WLANHWDRV_GPIO_EN %d\n", status, WLANHWDRV_GPIO_EN);
        goto end;
    }

    /* Set it to GPIO mode: GPIO_Ex have MUX1 */
    DBG("pnx_set_gpio_mode %d %d", WLANHWDRV_GPIO_EN, GPIO_MODE_MUX1);
    WLANHWDRV_GPIO_SET_MODE(WLANHWDRV_GPIO_EN, GPIO_MODE_MUX1);

    /* Set direction as output */
	/*ACER Ed 2010-04-21 For temporarily we would force control WiFi GPIO for PR2*/
    WLANHWDRV_GPIO_SET_DIR(WLANHWDRV_GPIO_EN, GPIO_DIR_OUTPUT, 1);
#endif

end:
    printk(KERN_INFO "[WLANHWDRV]WLAN Power Control Driver Init return %d\n", status);
    return status;
}

static void wlanhwdrv_exit(void)
{
    int status = 0;

    DBG("WLAN Power Control Driver cleanup...");

    status = misc_deregister(&wlanhwdrv_dev);
    DBG("misc_deregister[%d]", status);

#if defined(ACER_K3_PR2) || defined(ACER_K3_PCR)
    WLANHWDRV_GPIO_FREE(WLANHWDRV_GPIO_EN);
#endif

    printk(KERN_INFO "[WLANHWDRV]WLAN Power Control Driver Exit\n");
}

module_init(wlanhwdrv_init);
module_exit(wlanhwdrv_exit);


#undef WLANHWDRV_C
