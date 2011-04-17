/*	sx1502.h - SEMTECH GPIO expander
 *	
 *	Copyright (C) 2010 Selwyn Chen <SelwynChen@acertdc.com>
 *	Copyright (C) 2010 ACER Co., Ltd.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 */

#ifndef _SX1502_H_
#define _SX1502_H_

//register offset
#define SX1502_REGDATA		0x00
#define SX1502_REGDIR		0x01
#define SX1502_REGPULLUP	0x02
#define SX1502_REGPULLDOWN	0x03

#define SX1502_REGINTMASK	0x05
#define SX1502_REGSENSEHIGH	0x06
#define SX1502_REGSENSELOW	0x07
#define SX1502_REGINTSOURCE	0x08
#define SX1502_REGEVENTSTATUS	0x09
#define SX1502_REGPLDMODE	0x10
#define SX1502_REGPLDTABLE0	0x11
#define SX1502_REGPLDTABLE1	0x12
#define SX1502_REGPLDTABLE2	0x13
#define SX1502_REGPLDTABLE3	0x14
#define SX1502_REGPLDTABLE4	0x15

#define SX1502_REGADVANCED	0xAB

//event value
#define JOG_BALL_L		0x01
#define JOG_BALL_R		0x02
#define JOG_BALL_U		0x04
#define JOG_BALL_D		0x08
#define JOG_BALL_S		0x10
#define WLAN_POWERON		0x20
#define HPH_PWD_N		0x40
#define JB_POWER_CTRL		0x80


#endif /* _SX1502_H_ */

