/*
 * linux/drivers/video/pnx/splash/splash_png.h
 *
 * PNX PNG Splash 
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_SPLASH_SPLASH_PNG_H
#define __DRIVERS_VIDEO_SPLASH_SPLASH_PNG_H

/* Maximum screen resolution, used for memory allocation */
#define SPLASH_MAXX				(320)
#define SPLASH_MAXY				(320)

/* Maximum png file size in bytes */
#define SPLASH_MAXPNGFILESIZE	(100000) // FIXME size of the biggest png file

/* Maximum decompressed image memory in bytes
 * (+1 because of the filter byte before each line)
 * (*4 for RGBA) */
#define SPLASH_MAXIMGMEM		((4*SPLASH_MAXX + 1) * SPLASH_MAXY)

int splash_start(struct lcdfb_drvdata *drvdata);
void splash_stop(struct lcdfb_drvdata *drvdata);

#endif // __DRIVERS_VIDEO_SPLASH_SPLASH_PNG_H
