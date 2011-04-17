/*
 *  linux/drivers/video/pnx/displays/hx8357.h
 *
 *  Copyright (C) 2006 ACER Taipei
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_HX8357_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_HX8357_H

/*
 =========================================================================
 =                                                                       =
 =              LCD and FRAMEBUFFER CONFIGURATIONS                       =
 =                                                                       =
 =========================================================================
*/

#define HX8357_NAME	"hx8357"

#define HX8357_SCREEN_WIDTH  	320
#define HX8357_SCREEN_HEIGHT 	240

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define HX8357_SCREEN_BUFFERS	1

/* It is possible to choose the fb BPP between 16, 24 or 32 for applications.
 * The VDE will do the conversion in hw before sending the data to the LCD.
 * (This conversion is only done if BPP=16 or 32)
 */
#define HX8357_FB_BPP			16 /* 16, 24 or 32 */

/* Choose the LCD colors configuration:
 *   - LCD screen with  65k colors
 *   - LCD screen with 262k colors
 */
#define HX8357_262K_COLORS     0  /* (1/0 Enable or Disable the 262K mode */

#endif /* __DRIVERS_VIDEO_LCDBUS_DISPLAYS_HX8357_H */

