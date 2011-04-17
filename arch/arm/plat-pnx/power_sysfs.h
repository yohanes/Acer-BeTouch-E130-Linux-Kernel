/*
 * linux/arch/arm/plat-pnx/power_sysfs.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:23:30 PM
 *      Author:  Vincent Guittot (VGU), vincent.guittot@stericsson.com
 */

#ifndef  POWER_SYSFS_INC
#define  POWER_SYSFS_INC

/*** Power kobject ***/
extern struct kobject pnx_power_kobj;

/*** sysfs file tree ***/
struct pnx_power_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define define_one_ro(_name)		\
static struct pnx_power_attr _name =		\
__ATTR(_name, 0444, show_##_name, NULL)

#define define_one_ro_ext(_name, _ext)		\
static struct pnx_power_attr _name##_ext =		\
__ATTR(_name, 0444, show_##_name##_ext, NULL)

#define define_one_rw(_name) \
static struct pnx_power_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

#define define_one_rw_ext(_name, _ext) \
static struct pnx_power_attr _name##_ext = \
__ATTR(_name, 0644, show_##_name##_ext, store_##_name##_ext)

#endif   /* ----- #ifndef POWER_SYSFS_INC  ----- */


