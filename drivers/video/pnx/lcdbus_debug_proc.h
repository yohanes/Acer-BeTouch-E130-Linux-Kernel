/*
 * linux/drivers/video/pnx/lcdbus_debug_proc.h
 *
 * PNX framebuffer - proc and debug
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_PNX_LCDBUS_DEBUG_H
#define __DRIVERS_VIDEO_PNX_LCDBUS_DEBUG_H


#include "lcdbus_pagefaultfb.h"

/*****************************************************************************
 * COMMON PART
 *
 *****************************************************************************/
void lcdbus_debug_proc_init(struct lcdfb_drvdata *drvdata);

void lcdbus_debug_proc_deinit(struct lcdfb_drvdata *drvdata, int device_id);

/*****************************************************************************
 * DEBUG PART
 *
 *****************************************************************************/

void lcdbus_debug_fps_on_screen(struct lcdfb_drvdata *drvdata);

void lcdbus_debug_timer_fps(unsigned long data);

void lcdbus_debug_update(struct lcdfb_drvdata *drvdata, int output);


/*****************************************************************************
 * /PROC PART
 *
 *****************************************************************************/
inline void lcdbus_proc_time_before_transfer(int n);

inline void lcdbus_proc_time_after_transfer(int n);

void lcdbus_proc_reset_values(int n);


#endif // __DRIVERS_VIDEO_PNX_LCDBUS_DEBUG_H
