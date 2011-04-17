/*
 * linux/drivers/video/pnx/displays/ili9481bb.h
 *
 * ili9481bb HVGA LCD driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9481BB_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9481BB_H

/*
 =========================================================================
 =                                                                       =
 =              LCD and FRAMEBUFFER CONFIGURATIONS                       =
 =                                                                       =
 =========================================================================
*/

#define ILI9481BB_NAME	"ili9481bb"

#define ILI9481BB_SCREEN_WIDTH  	320
#define ILI9481BB_SCREEN_HEIGHT 	480

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define ILI9481BB_SCREEN_BUFFERS	1

/* It is possible to choose the fb BPP between 16, 24 or 32 for applications.
 * The VDE will do the conversion in hw before sending the data to the LCD.
 * (This conversion is only done if BPP=16 or 32)
 */
#define ILI9481BB_FB_BPP			16 /* 16, 24 or 32 */

/* Choose the LCD colors configuration:
 *   - LCD screen with  65k colors
 *   - LCD screen with 262k colors
 */
#define ILI9481BB_262K_COLORS     0  /* (1/0 Enable or Disable the 262K mode*/


/* LCD Commands ID codes -------------------------------------------------- */
#define ILI9481BB_SOFT_RESET_REG	               0x01
#define ILI9481BB_GET_RED_CHANNEL_REG              0x06
#define ILI9481BB_GET_GREEN_CHANNEL_REG            0x07
#define ILI9481BB_GET_BLUE_CHANNEL_REG             0x08
#define ILI9481BB_GET_POWER_MODE_REG               0x0A
#define ILI9481BB_GET_ADDRESS_MODE_REG             0x0B
#define ILI9481BB_GET_PIXEL_FORMAT_REG             0x0C
#define ILI9481BB_GET_DISPLAY_MODE_REG             0x0D
#define ILI9481BB_GET_SIGNAL_MODE_REG              0x0E
#define ILI9481BB_GET_DIAGNOSTIC_RESULT_REG        0x0F
#define ILI9481BB_ENTER_SLEEP_MODE_REG             0x10
#define ILI9481BB_EXIT_SLEEP_MODE_REG              0x11
#define ILI9481BB_ENTER_PARTIAL_MODE_REG           0x12
#define ILI9481BB_ENTER_NORMAL_MODE_REG            0x13
#define ILI9481BB_EXIT_INVERT_MODE_REG             0x20
#define ILI9481BB_ENTER_INVERT_MODE_REG            0x21
#define ILI9481BB_SET_GAMMA_CURVE_REG              0x26
#define ILI9481BB_SET_DISPLAY_OFF_REG              0x28
#define ILI9481BB_SET_DISPLAY_ON_REG               0x29
#define ILI9481BB_SET_COLUMN_ADDRESS_REG           0x2A
#define ILI9481BB_SET_PAGE_ADDRESS_REG             0x2B
#define ILI9481BB_WRITE_MEMORY_START_REG           0x2C
#define ILI9481BB_WRITE_LUT_REG                    0x2D
#define ILI9481BB_READ_MEMORY_START_REG            0x2E
#define ILI9481BB_SET_PARTIAL_AREA_REG             0x30
#define ILI9481BB_SET_SCROLL_AREA_REG              0x33
#define ILI9481BB_SET_TEAR_OFF_REG                 0x34
#define ILI9481BB_SET_TEAR_ON_REG                  0x35
#define ILI9481BB_SET_ADDRESS_MODE_REG             0x36
#define ILI9481BB_SET_SCROLL_START_REG             0x37
#define ILI9481BB_EXIT_IDLE_MODE_REG               0x38
#define ILI9481BB_ENTER_IDLE_MODE_REG              0x39
#define ILI9481BB_SET_PIXEL_FORMAT_REG             0x3A
#define ILI9481BB_WRITE_MEMORY_CONTINUE_REG        0x3C
#define ILI9481BB_READ_MEMORY_CONTINUE_REG         0x3E
#define ILI9481BB_SET_TEAR_SCANLINE_REG            0x44
#define ILI9481BB_GET_SCANLINE_REG                 0x45
#define ILI9481BB_READ_DDB_START_REG               0xA1
#define ILI9481BB_COMMAND_ACCESS_PROTECT_REG       0xB0
#define ILI9481BB_FRAME_MEMORY_ACCESS_REG          0xB3
#define ILI9481BB_DISPLAY_MODE_FRAME_MEMORY_REG    0xB4
#define ILI9481BB_DEVICE_CODE_READ_REG             0xBF
#define ILI9481BB_PANEL_DRIVING_SETTING_REG        0xC0
#define ILI9481BB_DISPLAY_TIMING_NORMAL_MODE_REG   0xC1
#define ILI9481BB_DISPLAY_TIMING_PARTIAL_MODE_REG  0xC2
#define ILI9481BB_DISPLAY_TIMING_IDLE_MODE_REG     0xC3
#define ILI9481BB_FRAME_RATE_INVERSION_CONTROL_REG 0xC5
#define ILI9481BB_INTERFACE_CONTROL_REG            0xC6
#define ILI9481BB_GAMMA_SETTING_REG                0xC8
#define ILI9481BB_POWER_SETTING_REG                0xD0
#define ILI9481BB_VCOM_CONTROL_REG                 0xD1
#define ILI9481BB_POWER_SETTING_NORMAL_MODE_REG    0xD2
#define ILI9481BB_POWER_SETTING_PARTIAL_MODE_REG   0xD3
#define ILI9481BB_POWER_SETTING_IDLE_MODE_REG      0xD4
#define ILI9481BB_NV_MEMORY_WRITE_REG              0xE0
#define ILI9481BB_NV_MEMORY_CONTROL_REG            0xE1
#define ILI9481BB_NV_MEMORY_STATUS_REG             0xE2
#define ILI9481BB_NV_MEMORY_PROTECT_REG            0xE3


/* LCD Commands parameters Size ------------------------------------------- */
#define ILI9481BB_SOFT_RESET_SIZE	                0  /* No                */
#define ILI9481BB_GET_RED_CHANNEL_SIZE              1  /* Yes (1Byte)       */
#define ILI9481BB_GET_GREEN_CHANNEL_SIZE            1  /* Yes (1Byte)       */
#define ILI9481BB_GET_BLUE_CHANNEL_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_GET_POWER_MODE_SIZE               1  /* Yes (1Byte)       */
#define ILI9481BB_GET_ADDRESS_MODE_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_GET_PIXEL_FORMAT_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_GET_DISPLAY_MODE_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_GET_SIGNAL_MODE_SIZE              1  /* Yes (1Byte)       */
#define ILI9481BB_GET_DIAGNOSTIC_RESULT_SIZE        1  /* Yes (1Byte)       */
#define ILI9481BB_ENTER_SLEEP_MODE_SIZE             0  /* No                */
#define ILI9481BB_EXIT_SLEEP_MODE_SIZE              0  /* No                */
#define ILI9481BB_ENTER_PARTIAL_MODE_SIZE           0  /* No                */
#define ILI9481BB_ENTER_NORMAL_MODE_SIZE            0  /* No                */
#define ILI9481BB_EXIT_INVERT_MODE_SIZE             0  /* No                */
#define ILI9481BB_ENTER_INVERT_MODE_SIZE            0  /* No                */
#define ILI9481BB_SET_GAMMA_CURVE_SIZE              1  /* Yes (1Byte)       */
#define ILI9481BB_SET_DISPLAY_OFF_SIZE              0  /* No                */
#define ILI9481BB_SET_DISPLAY_ON_SIZE               0  /* No                */
#define ILI9481BB_SET_COLUMN_ADDRESS_SIZE           4  /* Yes (4Byte)       */
#define ILI9481BB_SET_PAGE_ADDRESS_SIZE             4  /* Yes (4Byte)       */
#define ILI9481BB_WRITE_MEMORY_START_SIZE           0  /* Display-contents  */
#define ILI9481BB_WRITE_LUT_SIZE                    0  /* Display-contents  */
#define ILI9481BB_READ_MEMORY_START_SIZE            0  /* Display-contents  */
#define ILI9481BB_SET_PARTIAL_AREA_SIZE             4  /* Yes (4Byte)       */
#define ILI9481BB_SET_SCROLL_AREA_SIZE              6  /* Yes (6Byte)       */
#define ILI9481BB_SET_TEAR_OFF_SIZE                 0  /* No                */
#define ILI9481BB_SET_TEAR_ON_SIZE                  1  /* Yes (1Byte)       */
#define ILI9481BB_SET_ADDRESS_MODE_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_SET_SCROLL_START_SIZE             2  /* Yes (2Byte)       */
#define ILI9481BB_EXIT_IDLE_MODE_SIZE               0  /* No                */
#define ILI9481BB_ENTER_IDLE_MODE_SIZE              0  /* No                */
#define ILI9481BB_SET_PIXEL_FORMAT_SIZE             1  /* Yes (1Byte)       */
#define ILI9481BB_WRITE_MEMORY_CONTINUE_SIZE        0  /* Display-contents  */
#define ILI9481BB_READ_MEMORY_CONTINUE_SIZE         0  /* Display-contents  */
#define ILI9481BB_SET_TEAR_SCANLINE_SIZE            2  /* Yes (2Byte)       */
#define ILI9481BB_GET_SCANLINE_SIZE                 3  /* Yes (3Byte)       */
#define ILI9481BB_READ_DDB_START_SIZE               6  /* Yes (6Byte)       */
#define ILI9481BB_COMMAND_ACCESS_PROTECT_SIZE       1  /* Yes (1Byte)       */
#define ILI9481BB_FRAME_MEMORY_ACCESS_SIZE          4  /* Yes (4Byte)       */
#define ILI9481BB_DISPLAY_MODE_FRAME_MEMORY_SIZE    1  /* Yes (1Byte)       */
#define ILI9481BB_DEVICE_CODE_READ_SIZE             6  /* Yes (6Byte)       */
#define ILI9481BB_PANEL_DRIVING_SETTING_SIZE        5  /* Yes (5Byte)       */
#define ILI9481BB_DISPLAY_TIMING_NORMAL_MODE_SIZE   3  /* Yes (3Byte)       */
#define ILI9481BB_DISPLAY_TIMING_PARTIAL_MODE_SIZE  3  /* Yes (3Byte)       */
#define ILI9481BB_DISPLAY_TIMING_IDLE_MODE_SIZE     3  /* Yes (3Byte)       */
#define ILI9481BB_FRAME_RATE_INVERSION_CONTROL_SIZE 1  /* Yes (1Byte)       */
#define ILI9481BB_INTERFACE_CONTROL_SIZE            1  /* Yes (1Byte)       */
#define ILI9481BB_GAMMA_SETTING_SIZE                12 /* Yes (12Byte)      */
#define ILI9481BB_POWER_SETTING_SIZE                3  /* Yes (3Byte)       */
#define ILI9481BB_VCOM_CONTROL_SIZE                 3  /* Yes (3Byte)       */
#define ILI9481BB_POWER_SETTING_NORMAL_MODE_SIZE    2  /* Yes (2Byte)       */
#define ILI9481BB_POWER_SETTING_PARTIAL_MODE_SIZE   2  /* Yes (2Byte)       */
#define ILI9481BB_POWER_SETTING_IDLE_MODE_SIZE      2  /* Yes (2Byte)       */
#define ILI9481BB_NV_MEMORY_WRITE_SIZE              1  /* Yes (1Byte)       */
#define ILI9481BB_NV_MEMORY_CONTROL_SIZE            1  /* Yes (1Byte)       */
#define ILI9481BB_NV_MEMORY_STATUS_SIZE             3  /* Yes (3Byte)       */
#define ILI9481BB_NV_MEMORY_PROTECT_SIZE            2  /* Yes (2Byte)       */


#define ILI9481BB_REG_SIZE_MAX				12

#define ILI9481BB_HIBYTE(x)	(u8)(((x) & 0xFF00) >> 8)
#define ILI9481BB_LOBYTE(x)	(u8)((x) & 0x00FF)

#endif /* __DRIVERS_VIDEO_LCDBUS_DISPLAYS_ILI9481BB_H */
