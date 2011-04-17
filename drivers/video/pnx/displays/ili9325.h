/*
 *  linux/drivers/video/pnx/displays/ili9325.h
 *  Change From jbt6k71-as-c.h by Ethan
 *  Copyright (C) 2006 ACER Taipei
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9325_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9325_H

/*
 =========================================================================
 =                                                                       =
 =              LCD and FRAMEBUFFER CONFIGURATIONS                       =
 =                                                                       =
 =========================================================================
*/

#define ILI9325_NAME	"ili9325"

#define ILI9325_SCREEN_WIDTH  	240
#define ILI9325_SCREEN_HEIGHT 	320

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define ILI9325_SCREEN_BUFFERS	1

/* It is possible to choose the fb BPP between 16, 24 or 32 for applications.
 * The VDE will do the conversion in hw before sending the data to the LCD.
 * (This conversion is only done if BPP=16 or 32)
 */
#define ILI9325_FB_BPP			16 /* 16, 24 or 32 */

/* Choose the LCD colors configuration:
 *   - LCD screen with  65k colors
 *   - LCD screen with 262k colors
 */
#define ILI9325_262K_COLORS     0  /* (1/0 Enable or Disable the 262K mode */


/* omitted NOP and test-purpose registers */
#define ILI9325_REGISTERMAP_SIZE	0x6FF

#define HIBYTE(x)	((u8)((x) >> 8))
#define LOBYTE(x)	(u8)(x)

#endif /* __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9325_H */

