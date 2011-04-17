/*
 * linux/drivers/video/pnx/displays/tvout.h
 *
 * TV Out driver 
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __DRIVER_VIDEO_DISPLAY_TVOUT_H__
#define __DRIVER_VIDEO_DISPLAY_TVOUT_H__

#include <mach/pnxfb.h>


/*---------------------------------------------------------------------------*/
/*t_TVOOutputStandard (TVO output standards params)                          */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVOOutputStandard;

#define TVO_NTSC_MAX_WIDTH					720
#define TVO_NTSC_MAX_HEIGHT					480

#define TVO_PAL_MAX_WIDTH					720
#define TVO_PAL_MAX_HEIGHT					576

#define TVO_NTSC_FIRST_COLUMN               1      /* NTSC dimensions */
#define TVO_NTSC_LAST_COLUMN                858
#define TVO_NTSC_FIRST_LINE                 1
#define TVO_NTSC_LAST_LINE                  526

#define TVO_NTSC_ACTIVE_COLUMN_START        138    /* Tvout active window */
#define TVO_NTSC_ACTIVE_COLUMN_STOP         858    /* for NTSC standard */
#define TVO_NTSC_ACTIVE_ODD_LINE_START      23
#define TVO_NTSC_ACTIVE_ODD_LINE_STOP       263
#define TVO_NTSC_ACTIVE_EVEN_LINE_START     286
#define TVO_NTSC_ACTIVE_EVEN_LINE_STOP      526

#define TVO_PAL_FIRST_COLUMN                1      /* PAL dimensions */
#define TVO_PAL_LAST_COLUMN                 864
#define TVO_PAL_FIRST_LINE                  1
#define TVO_PAL_LAST_LINE                   625

#define TVO_PAL_ACTIVE_COLUMN_START         143    /* Tvout active window */
#define TVO_PAL_ACTIVE_COLUMN_STOP          863    /* for PAL standard */
#define TVO_PAL_ACTIVE_ODD_LINE_START       23
#define TVO_PAL_ACTIVE_ODD_LINE_STOP        311
#define TVO_PAL_ACTIVE_EVEN_LINE_START      336
#define TVO_PAL_ACTIVE_EVEN_LINE_STOP       624

/*
=========================================================================
=                                                                       =
=              TVOUT and FRAMEBUFFER CONFIGURATIONS                     =
=                                                                       =
=========================================================================
*/
#define	TVO_NAME				"pnx-tvo"

#define TVO_SCREEN_WIDTH		240
#define TVO_SCREEN_HEIGHT		320

#define TVO_OUTPUT_STANDARD		PNXFB_TVO_STANDARD_NTSC
#define TVO_ZOOM_MODE			PNXFB_ZOOM_BEST_FIT


/*Number of extra screen buffer
* 0 = no extra buffer (no double buffering)
* 1 = activate double buffering (use panning ioctl for buffer permutation)
* >1  more than 1 extra screen, could be usefull for extra video mem. */
#define TVO_SCREEN_BUFFERS	1

/*It is possible to choose the fb BPP between 16, 24 for applications.
 * (32 BPP is not supported by the TVOUT)
 */
#define TVO_FB_BPP			16 /* 16 or 24 */



/*---------------------------------------------------------------------------*/
/*t_TVOImageBank                                                             */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVOImageBank;
#define TVO_SELECT_BANK_A					0
#define TVO_SELECT_BANK_B					1


/*---------------------------------------------------------------------------*/
/*t_TVOStreamMode                                                            */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVOStreamMode;
#define TVO_SINGLE_PICTURE_MODE				0
#define TVO_LOOP_MODE						1

/*---------------------------------------------------------------------------*/
/*t_TVODebugProbeA                                                           */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVODebugProbeA;
#define TVO_PROBE_A_DVDO_OUTPUT             0
#define TVO_PROBE_A_COLOR_MATRIX_OUTPUT     1
#define TVO_PROBE_A_ANTI_FLICKER_OUTPUT     2

/*----------------------------------------------------------------------------*/
/* t_TVODebugProbeB                                                           */
/*----------------------------------------------------------------------------*/
typedef uint8_t                             t_TVODebugProbeB;
#define TVO_DEBUG_SIGNAL_Y_R				0
#define TVO_DEBUG_SIGNAL_U_G				1
#define TVO_DEBUG_SIGNAL_V_B				2

/*----------------------------------------------------------------------------*/
/* t_TVODebugProbeC                                                           */
/*----------------------------------------------------------------------------*/
typedef uint8_t                             t_TVODebugProbeC;
#define TVO_PROBE_C_DENC_OUTPUT				0
#define TVO_PROBE_C_FIELD_FORMATTER_OUTPUT	1
#define TVO_PROBE_C_FIFO_THRESHOLD			2
#define TVO_PROBE_C_PROBE_A_OUTPUT			3

/*---------------------------------------------------------------------------*/
/*t_TVODacMap                                                                */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVODacMap;
#define TVO_DAC_TO_DENC_CVBS				0
#define TVO_DAC_TO_DENC_Y					1
#define TVO_DAC_TO_DENC_C					2
#define TVO_DAC_TO_DENC_R					3
#define TVO_DAC_TO_DENC_G					4
#define TVO_DAC_TO_DENC_B					5


/*---------------------------------------------------------------------------*/
/*t_TVODacEdge                                                               */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVODacEdge;
#define TVO_DAC_RISING_EDGE					0
#define TVO_DAC_FALLING_EDGE				1

/*---------------------------------------------------------------------------*/
/*t_TVOAntiFlickerMode                                                       */
/*---------------------------------------------------------------------------*/
typedef unsigned char						t_TVOAntiFlickerMode;
#define TVO_ANTIFLICKER_MODE_NORMAL			0
#define TVO_ANTIFLICKER_MODE_INTERLEAVED	1
#define TVO_ANTIFLICKER_MODE_PROGRESSIVE	2

/*---------------------------------------------------------------------------*/
/*t_TVOColorMatrixCoef                                                       */
/*---------------------------------------------------------------------------*/
typedef struct {
	unsigned short coef21_H;
	unsigned short coef21_L;
	
	unsigned short coef43_L;
	unsigned short coef43_M;

	unsigned short coef65_H;
	unsigned short coef65_L;
	
	unsigned short coef87_L;
	unsigned short coef87_M;

	unsigned short coef109_H;
	unsigned short coef109_L;

	unsigned short coef1211_L;
	unsigned short coef1211_M;

	unsigned char  ucYColorClip;

}t_TVOColorMatrixCoef;

#define TVO_COLOR_MATRIX_COEF01 /*  16 */  0x010
#define TVO_COLOR_MATRIX_COEF02 /*  32 */  0x020
#define TVO_COLOR_MATRIX_COEF03 /*   6 */  0x006
#define TVO_COLOR_MATRIX_COEF04 /*  15 */  0x00F
#define TVO_COLOR_MATRIX_COEF05 /* -10 */  0xFF6
#define TVO_COLOR_MATRIX_COEF06 /* -19 */  0xFED
#define TVO_COLOR_MATRIX_COEF07 /*  28 */  0x01C
#define TVO_COLOR_MATRIX_COEF08 /* 127 */  0x07F
#define TVO_COLOR_MATRIX_COEF09 /*  27 */  0x01B
#define TVO_COLOR_MATRIX_COEF10 /* -24 */  0xFE8
#define TVO_COLOR_MATRIX_COEF11 /*  -5 */  0xFFB
#define TVO_COLOR_MATRIX_COEF12 /* 127 */  0x07F

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define TVO_COLOR_MATRIX_BYPASSED		0
#define TVO_COLOR_MATRIX_ENABLED		1


/*----------------------------------------------------------------------------*/
/*t_TVOAntiFlickerFilter                                                     */
/*----------------------------------------------------------------------------*/
typedef struct {
	unsigned char VEvenCbCr;
	unsigned char VEvenY;
	unsigned char VOddCbCr;
	unsigned char VOddY;
	unsigned char HCbCr;
	unsigned char HY;
}t_TVOAntiFlickerFilter;

/*----------------------------------------------------------------------------*/
/*t_TVOAntiFlicker                                                           */
/*----------------------------------------------------------------------------*/
typedef struct {
	t_TVOAntiFlickerMode	mode;
	t_TVOAntiFlickerFilter	filter;
}t_TVOAntiFlicker;

/*----------------------------------------------------------------------------*/
/*t_TVOGenConfig                                                             */
/*----------------------------------------------------------------------------*/
typedef struct {
	t_TVOStreamMode		streamMode;
	t_TVOOutputStandard	outputStandard;
	t_TVODebugProbeA	debugProbeA;
    t_TVODebugProbeB    debugProbeB;
    t_TVODebugProbeC    debugProbeC;
	t_TVODacMap			DACMap;
	t_TVODacEdge		DACEdge;
	unsigned short		colorMatrixEn;
	unsigned short		loadIdCalEn;
}t_TVOGenConfig;

/*----------------------------------------------------------------------------*/
/* t_TVODENCRegisters                                                         */
/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned char		WssCtrlLsb;
    unsigned char		WssCtrlMsb;
    unsigned char		BurstStart;
    unsigned char		BurstEnd;
    unsigned char		OutPortCtl;
    unsigned char		DacPd;
    unsigned char		GainR;
    unsigned char		GainG;
    unsigned char		GainB;
    unsigned char		InPortCtl;
    unsigned char		DacsCtl0;
    unsigned char		DacsCtl3;
    unsigned char		VpsEnable;
    unsigned char		VpsByte5;
    unsigned char		VpsByte11;
    unsigned char		VpsByte12;
    unsigned char		VpsByte13;
    unsigned char		VpsByte14;
    unsigned char		ChromaPhase;
    unsigned char		GainULsb;
    unsigned char		GainVLsb;
    unsigned char		GainUBlackLev;
    unsigned char		GainVBlackLev;
    unsigned char		CcrVertBlankLev;
    unsigned char		StdCtl;
    unsigned char		BurstAmpl;
    unsigned char		Fsc0;
    unsigned char		Fsc1;
    unsigned char		Fsc2;
    unsigned char		Fsc3;
    unsigned char		HTrig;
    unsigned char		VTrig;
    unsigned char		MultiCtl;
    unsigned char		FirstActiveLnLsb;
    unsigned char		LastActLnLsb;
} t_TVODENCRegisters;


/*----------------------------------------------------------------------------*/
/* t_TVOInputFormat                                                          */
/*----------------------------------------------------------------------------*/
typedef unsigned int						t_TVOInputFormat;
#define TVO_FORMAT_BGR444_HALFWORD_ALIGNED	0xE3E40000
#define TVO_FORMAT_BGR565_HALFWORD_ALIGNED	0xC7E40000
#define TVO_FORMAT_BGR666_PACKED_ON_24BIT	0xB7E40000
#define TVO_FORMAT_BGR666_WORD_ALIGNED		0xB3C60000
#define TVO_FORMAT_BGR888_PACKED_ON_24BIT	0x8FE40000
#define TVO_FORMAT_BGR888_WORD_ALIGNED		0x93C60000
#define TVO_FORMAT_RGB332_BYTE_ALIGNED		0xF3E40000
#define TVO_FORMAT_RGB444_HALFWORD_ALIGNED	0xD0E4E4E4
#define TVO_FORMAT_RGB565_HALFWORD_ALIGNED	0xC3E40000
#define TVO_FORMAT_RGB666_PACKED_ON_24BIT	0xA3E40000
#define TVO_FORMAT_RGB666_WORD_ALIGNED		0xB3E40000
#define TVO_FORMAT_RGB888_PACKED_ON_24BIT	0x8BE40000
#define TVO_FORMAT_RGB888_WORD_ALIGNED		0x93E40000
#define TVO_FORMAT_YUV400_PLANAR			0x20E40000
#define TVO_FORMAT_YUV411_PLANAR			0x10E4E4E4
#define TVO_FORMAT_YUV411_SEMI_PLANAR_CBCR	0x11E4E400
#define TVO_FORMAT_YUV411_SEMI_PLANAR_CRCB	0x11E4B100
#define TVO_FORMAT_YUV420_PLANAR			0x0CE4E4E4
#define TVO_FORMAT_YUV420_SEMI_PLANAR_CBCR	0x0DE4E400
#define TVO_FORMAT_YUV420_SEMI_PLANAR_CRCB	0x0DE4B100
#define TVO_FORMAT_YUV422_CO_PLANAR_CBYCRY	0x0BB10000
#define TVO_FORMAT_YUV422_CO_PLANAR_CRYCBY	0x0B390000
#define TVO_FORMAT_YUV422_CO_PLANAR_YCBYCR	0x0BE40000
#define TVO_FORMAT_YUV422_CO_PLANAR_YCRYCB	0x0BC60000
#define TVO_FORMAT_YUV422_PLANAR			0x08E4E4E4
#define TVO_FORMAT_YUV422_SEMI_PLANAR_CBCR	0x09E4E400
#define TVO_FORMAT_YUV422_SEMI_PLANAR_CRCB	0x09E4B100
#define TVO_FORMAT_YUV444_CO_PLANAR			0x07E40000
#define TVO_FORMAT_YUV444_PLANAR			0x04E4E4E4
#define TVO_FORMAT_YUV444_SEMI_PLANAR_CBCR	0x05E4E400
#define TVO_FORMAT_YUV444_SEMI_PLANAR_CRCB	0x05E4B100

#define TVO_FORMAT_IS_RGB(format)  ((format) & 0x80000000)

/*----------------------------------------------------------------------------*/
/* t_TVOFrameSize                                                             */
/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned short	width;
    unsigned short	height;
} t_TVOFrameSize;

/*----------------------------------------------------------------------------*/
/* t_TVOPosition                                                              */
/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned short	horzPos;
	unsigned short	vertPos;
} t_TVOPosition;

/*----------------------------------------------------------------------------*/
/* t_TVOFieldPixColor                                                         */
/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned char CrComponent;
    unsigned char CbComponent;
    unsigned char YComponent;
} t_TVOFieldPixColor;

/*----------------------------------------------------------------------------*/
/* t_TVOLoadId                                                            */
/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned short horzStart;
    unsigned short horzStop;
    unsigned short vertStart;
    unsigned short vertStop;
} t_TVOLoadId;

/*----------------------------------------------------------------------------*/
/* t_TVOFieldFormatter                                                        */
/*----------------------------------------------------------------------------*/
typedef struct {
    t_TVOPosition		oddBeg;
    t_TVOPosition		oddEnd;
    t_TVOPosition		evenBeg;
    t_TVOPosition		evenEnd;
    t_TVOFieldPixColor	fieldPixColor;
    t_TVOLoadId			loadId;
} t_TVOFieldFormatter;


/*----------------------------------------------------------------------------*/
/* t_TVOInputImage															  */
/*----------------------------------------------------------------------------*/
typedef struct {
	unsigned int		pixelBlanking;
    unsigned int		lineBlanking;
    unsigned int		verticalBlanking;
	unsigned int		inputAddressPlane[3];
    unsigned short		inputStridePlane[3];

	unsigned short	stripeSize;
	unsigned short	imageBank;

} t_TVOInputImage;

#endif	/* __DRIVER_VIDEO_DISPLAY_TVOUT_H__ */
