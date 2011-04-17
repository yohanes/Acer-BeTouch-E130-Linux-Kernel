/*
 * linux/arch/arm/plat-pnx/update-flash.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Alexandre Torgue <alexandre.torgue@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <net/9p/9p.h>
#include <net/9p/client.h>
#include <linux/err.h>

#define DRIVER_NAME "updateflash"

struct pnx_updateflash {
	struct p9_client *client;
	struct p9_fid *fid_root;
	struct p9_fid *fid_file;
	int    file_register;
	int    file_open;
};

static struct pnx_updateflash drv;

static int updateflash_register(void)
{
	int ret = 0;

	if (drv.file_register == 0) {

		drv.client = p9_client_create(DRIVER_NAME,
				"trans=xoscore,noextend");
		if (IS_ERR(drv.client)) {
			ret = PTR_ERR(drv.client);
			drv.client = NULL;
			printk(KERN_ERR
				"updateflash: cannot create 9P client\n");
			goto out;
		}
		drv.file_register = 1;

		drv.fid_root = p9_client_attach(drv.client,
				NULL, "nobody", 0, "");
		if (IS_ERR(drv.fid_root)) {
			printk(KERN_ERR
				"updateflash: cannot attach to 9P server\n");
			ret = PTR_ERR(drv.fid_root);
			p9_client_destroy(drv.client);
			drv.client = NULL;
			drv.fid_root = NULL;
			goto out;
		}
	}
out:
	return ret;
}

static int updateflash_open(void)
{
	struct p9_fid *p9_dirfid;
	char *path[] = { "updateflash", "0" };
	int ret = 0;

	if (drv.file_open == 0) {
		p9_dirfid = p9_client_walk(drv.fid_root,
				ARRAY_SIZE(path), path, 1);

		drv.fid_file = p9_dirfid;

		if (IS_ERR(p9_dirfid)) {
			printk(KERN_ERR "cannot walk on dir:%s\n", path[0]);
			ret = PTR_ERR(p9_dirfid);
			goto out;
		}


		ret = p9_client_open(p9_dirfid, P9_OWRITE);
		if (IS_ERR_VALUE(ret)) {
			printk(KERN_ERR "updateflash cannot open %s dir\n"
					, path[0]);
			goto out;
		}
	drv.file_open = 1;
	}
out:
	return ret;

}

static int updateflash_write(void)
{
	char buf = 0;
	int ret = 0;

	p9_client_write(drv.fid_file, &buf, NULL, 0, sizeof(buf));

	return ret;
}

static void updateflash_close(void)
{
	if (drv.fid_file != NULL) {
		p9_client_clunk(drv.fid_file);
		drv.file_open = 0;
	}

}

static int updateflash_resume(struct platform_device *pdev)
{
	int ret;

	/*  update flash for each resume */
	ret = updateflash_register();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot register files err=%d\n", ret);
		goto out;
	}
	ret = updateflash_open();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot open files err=%d\n", ret);
		goto out;
	}

	ret = updateflash_write();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot write in files err=%d\n", ret);
		goto out;
	}
out:
	return ret;
}



static struct platform_driver updateflash_driver = {
	.driver.name = "updateflash",
	.resume = updateflash_resume,
};

static struct platform_device updateflash_device = {
	.name = "updateflash",
};

static int __init updateflash_init(void)
{

	int ret;

	drv.client = NULL;
	drv.fid_root = NULL;
	drv.fid_file = NULL;
	drv.file_register = 0;
	drv.file_open = 0;

	ret = platform_device_register(&updateflash_device);
	if (ret) {
		pr_err("updateflash_init: platform_device_register failed\n");
		goto err;
	}
	ret = platform_driver_register(&updateflash_driver);
	if (ret) {
		pr_err("updateflash_init: platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	return 0;

err_platform_driver_register:
	platform_device_unregister(&updateflash_device);
err:

	return ret;

}

static void __exit updateflash_exit(void)
{
	int ret;

	ret = updateflash_register();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot register files err=%d\n", ret);
		goto out;
	}
	ret = updateflash_open();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot open files err=%d\n", ret);
		goto out;
	}

	ret = updateflash_write();

	if (IS_ERR_VALUE(ret)) {
		printk(KERN_ERR "cannot write in files err=%d\n", ret);
		goto out_and_close;
	}



out_and_close:
	updateflash_close();
out:
	platform_driver_unregister(&updateflash_driver);
}

late_initcall(updateflash_init);
module_exit(updateflash_exit);

MODULE_AUTHOR("Alexandre TORGUE - Copyright (C) ST-Ericsson 2010");

