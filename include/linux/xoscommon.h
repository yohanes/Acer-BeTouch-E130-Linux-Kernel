/*****************************************************************************
 * File name:   xoscommon.h
 * Project:     Linux on Sy.Sol 7210
 *---------------------------------------------------------------------------*
 *                             DESCRIPTION
 *
 * Provide unique ID for cross-OS memory managed by PS
 *****************************************************************************/


#ifndef XOSCOMMON_H
#define XOSCOMMON_H

/* Each cross memory area is identified by 4 chars, which give the int id. */
/* We use lower case strings so as not to conflict with potential suppliers. */

#define NK_DEV_ID_NFI   0x6e666920    /* "nfi " */
#define NK_DEV_ID_MUX   0x6d757820    /* "mux " */
#define NK_DEV_ID_VT    0x7674656c    /* "vtel" */
#define NK_DEV_ID_PMU   0x706d7520    /* "pmu " */
#define NK_DEV_ID_PIPE  0x70697065    /* "pipe" */
#define NK_DEV_ID_TIMER 0x74696d65    /* "time" */
#define NK_DEV_ID_TOOLS 0x746f6f6c    /* "tool" */

#ifdef DUAL_OS_POWER_FTR
#define NK_DEV_ID_POWERSAVING 'nxPM'  /* nxp Power Management */
#endif

#define NK_DEV_ID_BOOTTIME 0x626f6f74 /* "boot" */

#endif /* XOSCOMMON_H */
