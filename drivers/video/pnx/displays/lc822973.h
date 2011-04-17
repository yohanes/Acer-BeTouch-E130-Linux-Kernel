/*
 * linux/drivers/video/pnx/displays/lc822973.h
 *
 * lc822973 LCD driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVERS_VIDEO_LCDBUS_DISPLAYS_LC822973_H
#define __DRIVERS_VIDEO_LCDBUS_DISPLAYS_LC822973_H

/*
 =========================================================================
 =                                                                       =
 =              TVOUT and FRAMEBUFFER CONFIGURATIONS                     =
 =                                                                       =
 =========================================================================
*/

#define LC822973_NAME	"lc822973"

#define LC822973_SCREEN_WIDTH  	240
#define LC822973_SCREEN_HEIGHT 	320

/* Number of extra screen buffer
 * 0 = no extra buffer (no double buffering)
 * 1 = activate double buffering (use panning ioctl for buffer permutation)
 * >1  more than 1 extra screen, could be usefull for extra video mem. */
#define LC822973_SCREEN_BUFFERS	0

/* Two cases:
 * 1) TVout uses 8-bit parallel on VDE (MODE=15, 24bpp)
 *      LC822973_FB_BPP possible value is 24
 * 2) TVout uses 16-bit parallel on VDE (MODE=0, 16bpp)
 *      LC822973_FB_BPP possible values are 16 or 32
 */
#define LC822973_FB_BPP			16 /* 16, 24 or 32 */


/* Tvout Commands ID codes -----------------------------------------------------*/
#define LC822973_REG_nop             0x0000
#define LC822973_REG_clkcont         0x0001
#define LC822973_REG_divm            0x0002
#define LC822973_REG_divn            0x0003
#define LC822973_REG_divp            0x0004
#define LC822973_REG_gpo             0x0005
#define LC822973_REG_Int             0x0006
#define LC822973_REG_Inten           0x0007
#define LC822973_REG_sysctl1         0x0008
#define LC822973_REG_sysctl2         0x0009
#define LC822973_REG_sysctl3         0x000A
#define LC822973_REG_memset1         0x000B
#define LC822973_REG_memset2         0x000C
#define LC822973_REG_memset3         0x000D
#define LC822973_REG_imgwrite        0x000E
#define LC822973_REG_imgreadgo       0x000F
#define LC822973_REG_imgread         0x0010
#define LC822973_REG_imgabort        0x0011
#define LC822973_REG_scale           0x0012
#define LC822973_REG_wfbhofst        0x0013
#define LC822973_REG_wfbvofst        0x0014
#define LC822973_REG_wfbhlen         0x0015
#define LC822973_REG_wfbvlen         0x0016
#define LC822973_REG_wfbhstart       0x0017
#define LC822973_REG_wfbvstart       0x0018
#define LC822973_REG_rfbhofst        0x0019
#define LC822973_REG_rfbvofst        0x001A
#define LC822973_REG_dsphofst        0x001B
#define LC822973_REG_dspvofst        0x001C
#define LC822973_REG_dsphlen         0x001D
#define LC822973_REG_dspvlen         0x001E
#define LC822973_REG_bgcolor1        0x001F
#define LC822973_REG_bgcolor2        0x0020
#define LC822973_REG_encmode         0x0021
#define LC822973_REG_encgain1        0x0022
#define LC822973_REG_encgain2        0x0023
#define LC822973_REG_encbst1         0x0024
#define LC822973_REG_encbst2         0x0025
#define LC822973_REG_encbbplt        0x0026
#define LC822973_REG_encrhval        0x0027
#define LC822973_REG_enchblk         0x0028
#define LC822973_REG_encvblk         0x0029
#define LC822973_REG_version         0x002A
#define LC822973_REG_testmode        0x002B
#define LC822973_REG_osdcont1        0x002C
#define LC822973_REG_osdwfbhofst     0x002D
#define LC822973_REG_osdwfbvofst     0x002E
#define LC822973_REG_osdwfbhlen      0x002F
#define LC822973_REG_osdwfbvlen      0x0030
#define LC822973_REG_osdrfbhofst1    0x0031
#define LC822973_REG_osdrfbvofst1    0x0032
#define LC822973_REG_osdhofst1       0x0033
#define LC822973_REG_osdvofst1       0x0034
#define LC822973_REG_osdhlen1        0x0035
#define LC822973_REG_osdvlen1        0x0036
#define LC822973_REG_osdcont2        0x0037
#define LC822973_REG_osdrfbhofst2    0x0038
#define LC822973_REG_osdrfbvofst2    0x0039
#define LC822973_REG_osdhofst2       0x003A
#define LC822973_REG_osdvofst2       0x003B
#define LC822973_REG_osdhlen2        0x003C
#define LC822973_REG_osdvlen2        0x003D
#define LC822973_REG_osdcolory       0x003E
#define LC822973_REG_osdcoloru       0x003F
#define LC822973_REG_osdcolorv       0x0040
#define LC822973_REG_osdwrite        0x0041
#define LC822973_REG_osdabort        0x0042
#define LC822973_REG_macro           0x005B
#define LC822973_REG_fb				 0x00FB // FIXME hidden register !


#define LC822973_REG(r)				(LC822973_REG_ ## r)

/* maximum register number (used for register cache */
#define LC822973_REGISTERMAP_SIZE	0x80

enum LC822973_supported_tv_standard {
	TV_PAL,
	TV_NTSC,
};

#define HIBYTE(x)	((u8)((x) >> 8))
#define LOBYTE(x)	(u8)((x) & (0x00FF))

enum issue_cmds {
	LC822973_WRITE,
	LC822973_EXEC,
	LC822973_WAIT,
	LC822973_END,
};

struct lc822973_issue {
	enum issue_cmds cmd;
	u16 reg;
	u32 data;
};

struct lc822973_cmd {
	int (*cmd) (struct device *dev, const struct lc822973_issue *issue);
};

/*
 * -----------------------------------------------------------------
 * Switch on sequence for the LC822973 TV out IC
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_bootstrap_values[] = {

/* Software reset & wait 10ms */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(sysctl2),   .data = 0x0001},
	{.cmd = LC822973_EXEC},
	{.cmd = LC822973_WAIT,                                  .data = 0x0010},

/* PLL div_m */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(divm),      .data = 0x0096},
/* PLL div_n */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(divn),      .data = 0x001B},
/* PLL div_p */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(divp),      .data = 0x0003},
/* Clock ON */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(clkcont),   .data = 0x16D8},

 /* Interrupt message clear */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(Int),       .data = 0x07FF},


/* memset1 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(memset1),   .data = 0x0027},
/* memset2 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(memset2),   .data = 0x030D},
/* memset3 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(memset3),   .data = 0x0804},


/* sysctl1 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(sysctl1),   .data = 0x0020},
/* sysctl2 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(sysctl2),   .data = 0x04176},
/* sysctl3 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(sysctl3),   .data = 0x0000},


/* bgcolor1 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(bgcolor1),   .data = 0x0010}, // 0x006C
/* bgcolor2 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(bgcolor2),   .data = 0x8080}, // 0x4075

/* encbbplt */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(encbbplt),  .data = 0x43BF},

/* OSD1 off */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(osdcont1),  .data = 0x0000},
/* OSD2 off */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(osdcont2),  .data = 0x0000},

/* encgain1 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(encgain2),   .data = 0x0745},
/* encgain2 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(encgain2),   .data = 0x0286},

/* encbst1 */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(encbst1),   .data = 0x0020},

/* macrovision off */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(macro),     .data = 0x0000},

/* encmode */
	{.cmd = LC822973_WRITE, .reg = LC822973_REG(encmode),   .data = 0x0000},

	{.cmd = LC822973_EXEC},
	{.cmd = LC822973_END}
};


/*
 * -----------------------------------------------------------------
 * Display on sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_on_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

/*
 * -----------------------------------------------------------------
 * Display off sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_off_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

/*
 * -----------------------------------------------------------------
 * Enter standby mode sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_standby_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

/*
 * -----------------------------------------------------------------
 * Leave standby mode sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_leave_standby_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

/*
 * -----------------------------------------------------------------
 * Enter deep standby mode sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_deep_standby_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

/*
 * -----------------------------------------------------------------
 * Leave deep standby mode sequence
 * -----------------------------------------------------------------
 */
static const struct lc822973_issue lc822973_display_leave_deep_standby_seq[] = {
	{.cmd = LC822973_WAIT,                                  .data = 0x0000},

	{.cmd = LC822973_END}
};

#endif

