/*
 * linux/arch/arm/plat-pnx/include/mach/pnxfb.h
 *
 * PNX FB driver (Added capabilities)
 * Copyright (C) 2010 ST-Ericsson
 *
 */

#ifndef  __PNXFB_INC__
#define  __PNXFB_INC__


/* Available zoom mode for ICEBIRD Framebuffer (used by TVOUT) */
enum {
 PNXFB_ZOOM_NONE ,      /* No zoom is applied */
 PNXFB_ZOOM_STRETCH,    /* Use the whole available area */
 PNXFB_ZOOM_BEST_FIT,   /* Respect the aspect ratio between width and height */
 /*PNXFB_ZOOM_FILL_MAX */ /* To be implemented ? */

 PNXFB_ZOOM_MAX
};

/* TVOUT standard */
enum {
	PNXFB_TVO_STANDARD_NTSC,	/* NTSC-M  */
	PNXFB_TVO_STANDARD_PAL,		/* PAL-B/G */

	PNXFB_TVO_STANDARD_NTSC_J,	/* NTSC-J (Japan) */
	PNXFB_TVO_STANDARD_PAL_M,	/* PAL-M  (Brazil 525 lines) */
	PNXFB_TVO_STANDARD_PAL_NC,	/* PAL-NC (Argentina 625 lines) */

	PNXFB_TVO_STANDARD_MAX
};

/* Available output devices for ICEBIRD Framebuffer */
enum {
 PNXFB_OUTPUT_1, // Used for Main LCD
 PNXFB_OUTPUT_2, // Used for TVOut

 PNXFB_OUTPUT_MAX
};

/* Output connection */
#define PNXFB_OUTPUT_CONN_NONE  (0)                     /* No outputs */
#define PNXFB_OUTPUT_CONN_1     (1 << PNXFB_OUTPUT_1)   /* Main LCD output */
#define PNXFB_OUTPUT_CONN_2     (1 << PNXFB_OUTPUT_2)   /* TVOUT output */

/* Partial update */
#define PNXFB_PARTIAL_UPDATE    (0x54445055)            /* "UPDT" */


/* Connect these outputs to this framebuffer */
#define PNXFB_SET_OUTPUT_CONNECTION _IOW('F',0x20,size_t)

/* Which outputs are connected to this framebuffer */
#define PNXFB_GET_OUTPUT_CONNECTION _IOR('F',0x21,size_t)

/* Which outputs exist on this framebuffer */
#define PNXFB_GET_ALL_OUTPUTS       _IOR('F',0x22,size_t)


/* Get the zoom mode for the framebuffer */
#define PNXFB_GET_ZOOM_MODE         _IOW('F',0x23,size_t)

/* Set the zoom mode for the framebuffer */
#define PNXFB_SET_ZOOM_MODE         _IOW('F',0x24,size_t)


/* Get the output standard for the framebuffer */
#define PNXFB_TVO_GET_OUT_STANDARD  _IOR('F',0x25,size_t)

/* Set the output standard for the framebuffer */
#define PNXFB_TVO_SET_OUT_STANDARD  _IOW('F',0x26,size_t)


#endif   /* ----- #ifndef __PNXFB_INC__  ----- */


