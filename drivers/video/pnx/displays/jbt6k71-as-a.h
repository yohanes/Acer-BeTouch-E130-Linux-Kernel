/*
 * linux/drivers/video/pnx/displays/jbt6k71-as-a.h
 *
 * jbt6k71-as-a QVGA LCD driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_JBT6K71_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_JBT6K71_H

/*
 =========================================================================
 =                                                                       =
 =              LCD and FRAMEBUFFER CONFIGURATIONS                       =
 =                                                                       =
 =========================================================================
*/

#define JBT6K71_NAME	"jbt6k71"

#define JBT6K71_SCREEN_WIDTH  	240
#define JBT6K71_SCREEN_HEIGHT 	320

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define JBT6K71_SCREEN_BUFFERS	1

/* It is possible to choose the fb BPP between 16, 24 or 32 for applications.
 * The VDE will do the conversion in hw before sending the data to the LCD.
 * (This conversion is only done if BPP=16 or 32)
 */
#define JBT6K71_FB_BPP			16 /* 16, 24 or 32 */

/* Choose the LCD colors configuration:
 *   - LCD screen with  65k colors
 *   - LCD screen with 262k colors
 */
#define JBT6K71_262K_COLORS     0  /* (1/0 Enable or Disable the 262K mode */


/* LCD Commands ID codes -------------------------------------------------- */
enum instruction_category {
	CATEGORY_DISPLAY     = 0x00,
	CATEGORY_POWER       = 0x01,
	CATEGORY_DATA        = 0x02,
	CATEGORY_GRAYSCALE   = 0x03,
	CATEGORY_WINDOW      = 0x04,
	CATEGORY_OSD         = 0x05,
	CATEGORY_NOP         = 0x06,
};

enum display_instructions {
	DISPLAY_OSC_SET           = 0x00,
	DISPLAY_DRV_OUT_CTRL_SET  = 0x01,
	DISPLAY_DRV_SIG_SET       = 0x02,
	DISPLAY_ENTRY_MODE        = 0x03,
	DISPLAY_H_VAL_WIDTH_SET   = 0x06,
	DISPLAY_MODE_1            = 0x07,
	DISPLAY_MODE_2            = 0x08,
	DISPLAY_MODE_3            = 0x09,
	DISPLAY_MODE_4            = 0x0B,
	DISPLAY_EXT_SIG_SET_1     = 0x0C,
	DISPLAY_FR_FREQ_SET       = 0x0D,
	DISPLAY_EXT_SIG_SET_2     = 0x0E,
	DISPLAY_EXT_SIG_SET_3     = 0x0F,
	DISPLAY_LTPS_CTRL_1       = 0x12,
	DISPLAY_LTPS_CTRL_2       = 0x13,
	DISPLAY_LTPS_CTRL_3       = 0x14,
	DISPLAY_LTPS_CTRL_4       = 0x15,
	DISPLAY_LTPS_CTRL_5       = 0x18,
	DISPLAY_LTPS_CTRL_6       = 0x19,
	DISPLAY_LTPS_CTRL_7       = 0x1A,
	DISPLAY_LTPS_CTRL_8       = 0x1B,
	DISPLAY_AMP_CAB_SET       = 0x1C,
	DISPLAY_MODE_SET          = 0x1D,
	DISPLAY_POWER_OFF_CNT_SET = 0x1E,
};

enum power_instructions {
	POWER_DISPLAY_CTRL   = 0x00,
	POWER_AUTO_MGMT_CTRL = 0x01,
	POWER_SUPPLY_CTRL_1  = 0x02,
	POWER_SUPPLY_CTRL_2  = 0x03,
	POWER_SUPPLY_CTRL_3  = 0x04,
	POWER_SUPPLY_CTRL_4  = 0x05,
	POWER_EXT_POL_CTRL   = 0x08,
};

enum data_instructions {
	DATA_RAM_ADDR_SET_1 = 0x00,
	DATA_RAM_ADDR_SET_2 = 0x01,
	DATA_RAMWR          = 0x02,
	DATA_GRAPHIC_OP_1   = 0x03,
	DATA_GRAPHIC_OP_2   = 0x04,
};

enum grayscale_instructions {
	GRAYSCALE_SETTING_1    = 0x00,
	GRAYSCALE_SETTING_2    = 0x01,
	GRAYSCALE_SETTING_3    = 0x02,
	GRAYSCALE_SETTING_4    = 0x03,
	GRAYSCALE_SETTING_5    = 0x04,
	GRAYSCALE_BLUE_OFF_SET = 0x05,
};

enum window_instructions {
	WINDOW_V_SCROLL_CTRL_1      = 0x00,
	WINDOW_V_SCROLL_CTRL_2      = 0x01,
	WINDOW_FIRST_SCR_DRV_POS_1  = 0x02,
	WINDOW_FIRST_SCR_DRV_POS_2  = 0x03,
	WINDOW_SECOND_SCR_DRV_POS_1 = 0x04,
	WINDOW_SECOND_SCR_DRV_POS_2 = 0x05,
	WINDOW_H_RAM_ADDR_LOC_1     = 0x06,
	WINDOW_H_RAM_ADDR_LOC_2     = 0x07,
	WINDOW_V_RAM_ADDR_LOC_1     = 0x08,
	WINDOW_V_RAM_ADDR_LOC_2     = 0x09,
};

enum osd_instructions {
	OSD_FEATURE          = 0x00,
	OSD_SCR_1_START_ADDR = 0x04,
	OSD_SCR_2_START_ADDR = 0x05,
	OSD_NOP              = 0xFF,

};

#define JBT6K71_REG(c, r)	(((CATEGORY_ ## c) << 8) | (c ## _ ## r))

/* omitted NOP and test-purpose registers */
#define JBT6K71_REGISTERMAP_SIZE	0x6FF

#define HIBYTE(x)	((u8)((x) >> 8))
#define LOBYTE(x)	(u8)(x)

#endif /* __DRIVERS_VIDEO_LCDBUS_DISPLAYS_JBT6K71_H */
