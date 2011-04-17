/*
 *  boottime.c - Manage the status of connected accessories
 */

#include <linux/kernel.h>	/* We're doing kernel work, needed for container_of */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/types.h>	/* needed for dev_t */
#include <linux/kdev_t.h>	/* needed for MKDEV */
#include <linux/cdev.h>
#include <linux/input.h>
#include <asm/uaccess.h>	/* needed for copy_from_user */
/* #include <linux/spinlock.h> */
/* #include <linux/version.h> */
/* #include <linux/list.h> */
/* #include <asm/mach-types.h> */
#include <mach/boottime.h>
#include <osware/osware.h>
#include <linux/xoscommon.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emeric Vigier, ST-Ericsson");
MODULE_DESCRIPTION("Provides Linux boot time information to shared variable");

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) | ((b) << 8) | (c))
#endif

#ifdef CONFIG_DEBUG_BOOTTIME
#define PDEBUG(format, arg...) printk(KERN_DEBUG format , ## arg)
#define PTRACE(format, arg...)  printk(KERN_DEBUG format , ## arg)
#define PMESSAGE(format, arg...) printk(KERN_INFO format , ## arg)
#else
#define PDEBUG(format, arg...)
#define PTRACE(format, arg...)
#define PMESSAGE(format, arg...)
#endif

#define PERROR(format, arg...) printk(KERN_ERR format , ## arg)


static unsigned int boottime_major = 0, boottime_minor = 0;
static unsigned long *gl_pBootTimeSniffer;
static struct boottime_dev_t boottime_dev;
static unsigned long boottime_sniffer = 0;

module_param_named ( sniffer, boottime_sniffer, ulong, S_IRWXU );

/*
 * Bind the Boot time sniffer on RTK shared variable
 */
static int boottime_interface_start(void)
{
    int retval = 0;
	NkPhAddr physaddr = nkops.nk_dev_lookup_by_type(NK_DEV_ID_BOOTTIME, 0);

    if ( physaddr == 0 )
    {
        retval = -ENOMEM;
		PERROR("BootTime: unable to retrieve shared BootTime structure");
        goto bail_out;
    }

    physaddr += sizeof ( NkDevDesc );
    gl_pBootTimeSniffer = nkops.nk_ptov ( physaddr );

bail_out:
    return retval;
}


/*
 * This is called whenever a process attempts to open the device file
 */
static int boottime_open(struct inode *inode, struct file *filp)
{
	struct boottime_dev_t *boottime_dev; /* device information */

	PDEBUG("BootTime: boottime_open\n");

	boottime_dev = container_of(inode->i_cdev, struct boottime_dev_t, cdev);
	filp->private_data = boottime_dev; /* for other methods */

	/* This value represents the boot time up to Linux Prompt before mounting the userfs
	 * It should be called in rc.sysinit before "mounting other devices" */
	*gl_pBootTimeSniffer = boottime_sniffer;

	return 0;
}


static int boottime_release(struct inode *inode, struct file *filp)
{
	PDEBUG("BootTime: boottime_release\n");

	/* *gl_pBootTimeSniffer = 0xB0010006; */

   	return 0;
}

static inline ssize_t
boottime_user_input(struct boottime_dev_t *boottime_dev,
		const char __user *buffer, size_t count)
{
	unsigned int user_input;
	int err;

	PDEBUG("BootTime: boottime_user_input\n");

	/* EVI: Isn't it annoying for our usage? */
	if (count != sizeof(unsigned long))
		return -EINVAL;

	/* EVI: FIX ME */
	if (copy_from_user(&user_input, buffer, sizeof(unsigned long)))
	{
		err = -EFAULT;
		PERROR("BootTime: Error %d in boottime_user_input", err);
		return err;
	}
	else
	{
		PDEBUG("BootTime: user_input= %u, 0x%x\n in boottime_user_input",
				user_input, user_input);
	}

	return sizeof(unsigned long);
}

static ssize_t
boottime_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct boottime_dev_t *boottime_dev = file->private_data;
	int retval;

	PDEBUG("BootTime: boottime_write\n");

	/* EVI: FIX ME */
	retval = boottime_dev->state == BT_STATE_CREATED ?
			boottime_user_input(boottime_dev, buffer, count):
			boottime_user_input(boottime_dev, buffer, count);

    return retval;
}

/* Module Declarations */

/*
 * This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions.
 */
static struct file_operations boottime_fops = {
	.open = boottime_open,
	.release = boottime_release,	/* a.k.a. close */
	.write = boottime_write,
    .owner= THIS_MODULE
};

/*
 * Bound the device driver with file operations
 */
static void boottime_setup_cdev(struct boottime_dev_t *boottime_dev, int index)
{
    int err, devno = MKDEV(boottime_major, boottime_minor + index);

	cdev_init(&boottime_dev->cdev, &boottime_fops);
    boottime_dev->cdev.owner = THIS_MODULE;
    boottime_dev->cdev.ops = &boottime_fops;
    err = cdev_add (&boottime_dev->cdev, devno, 1);

	/* Fail gracefully if need be */
    if (err)
	{
		PERROR("BootTime: Error %d adding boottime%d", err, index);
	}
}

/*
 * Initialize the module - Register the character device
 *
 */
int boottime_init_driver(void)
{

	int result;
	dev_t dev;

	PDEBUG("BootTime: boottime_init_driver started\n");

	/*
	 * Register the character device (at least try),
	 * 0 in parameter requests dynamic major allocation
	 */
	/* boottime_major = register_chrdev(0, BOOTTIME_DEVICE_NAME, &boottime_fops); */

	if (BOOTTIME_MAJOR)
	{
		dev = MKDEV(BOOTTIME_MAJOR, BOOTTIME_MINOR);
		result = register_chrdev_region(dev, 1, BOOTTIME_DEVICE_NAME);
	}
	else
	{
		result = alloc_chrdev_region(&dev, boottime_minor,
				1, BOOTTIME_DEVICE_NAME );
		boottime_major = MAJOR(dev);
	}

	if (result < 0)
	{
		PERROR("BootTime: can't get major %d\n", boottime_major);
		return result;
	}

	boottime_dev.cdev.dev = dev;

	boottime_setup_cdev(&boottime_dev, 0);
	boottime_interface_start();

	return 0;
}

/*
 * Cleanup - unregister the appropriate file from /dev
 * release all the structure allocated
 * enable sleep mode
 */
static __exit
void boottime_cleanup_driver(void)
{
	/*
	 * Unregister the device
	 */
	/* unregister_chrdev(boottime_major, BOOTTIME_DEVICE_NAME); */

	cdev_del(&boottime_dev.cdev);
}

arch_initcall(boottime_init_driver);
module_exit(boottime_cleanup_driver);

MODULE_AUTHOR("Emeric Vigier, ST-Ericsson");
MODULE_DESCRIPTION("Linux boot time information $Revision: 1.0 $");
MODULE_LICENSE("GPL");


