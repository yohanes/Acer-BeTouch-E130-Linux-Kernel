/*
 *     lis331dle.h- ST MEMS motion sensor
 *     Modified by Stefan Chuang
 *
 *     Copyright (C) 2008 Pasley Lin <pasleylin@tp.cmcs.com.tw>
 *     Copyright (C) 2008 Chi Mei Communication Systems Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#ifndef _TEA5991_H_
#define _TEA5991_H_

#include <linux/device.h>

#define POWER_UP	

#define WHO_AM_I	0x0F
#define CTRL_REG1	0x20
#define CTRL_REG2	0x21
#define CTRL_REG3	0x22
#define CTRL_REG4	0x23
#define CTRL_REG5	0x24
#define HP_FILTER_RESET	0x25
#define REFERENCE	0x26
#define STATUS_REG	0x27

#define OUT_X_L		0x28
#define OUT_X_H		0x29
#define OUT_Y_L		0x2A
#define OUT_Y_H		0x2B
#define OUT_Z_L		0x2C
#define OUT_Z_H		0x2D
#define INT1_CFG	0x30
#define INT1_SOURCE	0x31
#define INT1_THS	0x32
#define INT1_DURATION	0x33
#define INT2_CFG	0x34
#define INT2_SOURCE	0x35
#define INT2_THS	0x36
#define INT2_DURATION	0x37

#define I_AM_LIS331_DLH	0x32

//CTRL_REG1
#define CTRL_REG1_XEN		(1 << 0)
#define CTRL_REG1_YEN		(1 << 1)
#define CTRL_REG1_ZEN		(1 << 2)
#define CTRL_REG1_DR0		(1 << 3)
#define CTRL_REG1_DR1		(1 << 4)
#define CTRL_REG1_PM0		(1 << 5)
#define CTRL_REG1_PM1		(1 << 6)
#define CTRL_REG1_PM2		(1 << 7)

// CTRL_REG3
#define CTRL_REG3_PP_OD		(1 << 6)
#define CTRL_REG3_IHL		(1 << 7)

// CTRL_REG4
#define CTRL_REG4_ST		(1 << 1)

//INT1_CFG
#define INT1_CFG_XLIE		(1 << 0)
#define INT1_CFG_XHIE		(1 << 1)
#define INT1_CFG_YLIE		(1 << 2)
#define INT1_CFG_YHIE		(1 << 3)
#define INT1_CFG_ZLIE		(1 << 4)
#define INT1_CFG_ZHIE		(1 << 5)
#define INT1_CFG_6D		(1 << 6)
#define INT1_CFG_AOI		(1 << 7)

//INT1_SRC
#define INT1_SRC_XL		(1 << 0)	
#define INT1_SRC_XH		(1 << 1)
#define INT1_SRC_YL		(1 << 2)
#define INT1_SRC_YH		(1 << 3)
#define INT1_SRC_ZL		(1 << 4)
#define INT1_SRC_ZH		(1 << 5)
#define INT1_SRC_IA		(1 << 6)

//STATUS_REG
#define STATUS_REG_XDA		(1 << 0)
#define STATUS_REG_YDA		(1 << 1)
#define STATUS_REG_ZDA		(1 << 2)
#define STATUS_REG_ZYXDA	(1 << 3)

struct lis331_xyz_data
{
	short x;
	short y;
	short z;
};
#if defined(CONFIG_SENSORS_LIS331) || defined(CONFIG_SENSORS_LIS331_MODULE)
struct lis_platform_data
{
	int	(*init_irq)(void);
	int	(*ack_irq)(void);
	void (*platform_init)(void);
	struct i2c_client *client;
	struct mutex update_lock;
	int chipid;
	unsigned int power_state : 1;
	struct work_struct      work;
	spinlock_t      lock;
};
#endif

#endif
