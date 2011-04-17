/*
 * linux/drivers/video/pnx/displays/l5f30906.h
 *
 * l5f30906 WQVGA LCD driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_L5F30906_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_L5F30906_H

/*
 =========================================================================
 =                                                                       =
 =              LCD and FRAMEBUFFER CONFIGURATIONS                       =
 =                                                                       =
 =========================================================================
*/

#define L5F30906_NAME	"l5f30906"

#define L5F30906_SCREEN_WIDTH  	240
#define L5F30906_SCREEN_HEIGHT 	400

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define L5F30906_SCREEN_BUFFERS	1

/* It is possible to choose the fb BPP between 16, 24 or 32 for applications.
 * The VDE will do the conversion in hw before sending the data to the LCD.
 * (This conversion is only done if BPP=16 or 32)
 */
#define L5F30906_FB_BPP			16 /* 16, 24 or 32 */


/* LCD Commands ID codes -------------------------------------------------- */
#define L5F30906_SLPIN_REG        0x10
#define L5F30906_SLPOUT_REG       0x11
#define L5F30906_GAMSET_REG       0x26
#define L5F30906_DISPOFF_REG      0x28
#define L5F30906_DISPON_REG       0x29
#define L5F30906_CASET_REG        0x2A
#define L5F30906_PASET_REG        0x2B
#define L5F30906_RAMWR_REG        0x2C
#define L5F30906_RAMRD_REG        0x2E
#define L5F30906_TEOFF_REG        0x34
#define L5F30906_TEON_REG         0x35
#define L5F30906_MADCTL_REG       0x36
#define L5F30906_COLMOD_REG       0x3A
#define L5F30906_ADCCTL_REG       0x5B
#define L5F30906_SSLCTL_REG       0x70
#define L5F30906_PWMENB_REG       0x8A
#define L5F30906_DISCTL_REG       0xB0
#define L5F30906_PWRCTL_REG       0xB1
#define L5F30906_RGBIF_REG        0xB2
#define L5F30906_MADDEF_REG       0xB8
#define L5F30906_GAMMSETP0_REG    0xC0
#define L5F30906_GAMMSETN0_REG    0xC1
#define L5F30906_AMPCTL_REG       0xCC
#define L5F30906_DLS_REG          0xCD
#define L5F30906_MTPCTL_REG       0xD0
#define L5F30906_MIECTL_REG       0xE9
#define L5F30906_MIESIZCTL_REG    0x59
#define L5F30906_PWMCTL_REG       0xEA
#define L5F30906_EXTCMMOD1_REG    0xF0
#define L5F30906_EXTCMMOD2_REG    0xF1

/* LCD Commands parameters Size ------------------------------------------- */
#define L5F30906_SLPIN_SIZE         0     /* No                */
#define L5F30906_SLPOUT_SIZE        0     /* No                */
#define L5F30906_GAMSET_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_DISPOFF_SIZE       0     /* No                */
#define L5F30906_DISPON_SIZE        0     /* No                */
#define L5F30906_CASET_SIZE         4     /* Yes (4Byte)       */
#define L5F30906_PASET_SIZE         4     /* Yes (4Byte)       */
#define L5F30906_RAMWR_SIZE         0     /* Display-contents  */
#define L5F30906_RAMRD_SIZE         0     /* Display-contents  */
#define L5F30906_TEOFF_SIZE         0     /* No                */
#define L5F30906_TEON_SIZE          1     /* Yes (1Byte)       */
#define L5F30906_MADCTL_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_COLMOD_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_ADCCTL_SIZE        2     /* Yes (2Byte)       */
#define L5F30906_SSLCTL_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_PWMENB_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_DISCTL_SIZE       20     /* Yes (20Byte)      */
#define L5F30906_PWRCTL_SIZE       19     /* Yes (19Byte)      */
#define L5F30906_RGBIF_SIZE         6     /* Yes (6Byte)       */
#define L5F30906_MADDEF_SIZE        3     /* Yes (3Byte)       */
#define L5F30906_GAMMSETP0_SIZE    13     /* Yes (13Byte)      */
#define L5F30906_GAMMSETN0_SIZE    13     /* Yes (13Byte)      */
#define L5F30906_AMPCTL_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_DLS_SIZE           1     /* Yes (1Byte)       */
#define L5F30906_MTPCTL_SIZE        1     /* Yes (1Byte)       */
#define L5F30906_MIECTL_SIZE        4     /* Yes (4Byte)       */
#define L5F30906_MIESIZCTL_SIZE     5     /* Yes (5Byte)       */
#define L5F30906_PWMCTL_SIZE        3     /* Yes (3Byte)       */
#define L5F30906_EXTCMMOD1_SIZE     1     /* Yes (1Byte)       */
#define L5F30906_EXTCMMOD2_SIZE     1     /* Yes (1Byte)       */

#define L5F30906_REG_SIZE_MAX		20

#define L5F30906_HIBYTE(x)	(u8)(((x) & 0xFF00) >> 8)
#define L5F30906_LOBYTE(x)	(u8)((x) & 0x00FF)

#endif /* __DRIVERS_VIDEO_LCDBUS_DISPLAYS_L5F30906_H */

