/*****************************************************************************
 * File name:   sharedperf.h
 * Project:     Icebird Linux
 *---------------------------------------------------------------------------*
 *                             DESCRIPTION
 *
 * Provide common structure and ID for sharing Perf between Linux and RTK-E
 *****************************************************************************/


#ifndef SHAREDPERF_H
#define SHAREDPERF_H

#ifdef DUAL_OS_PERF_FTR
typedef struct {
    nku32_f     rtk_it;     /* RTK interrupt state, is it useful? */
    nku32_f     linux_it;   /* Linux interrupt state, useful to trace VDE, VDC and IPP latencies */
    NkOsId      rtk_id;     /* RTK OS identifier */
    NkOsId      linux_id ;  /* Linux OS identifier */
} NkPerf;
#endif /* DUAL_OS_PERF_FTR */

#ifdef DUAL_OS_BOOTTIME_FTR
typedef nku32_f NkBootTime;
#endif /* DUAL_OS_BOOTTIME_FTR */

/* /proc entry for debug */
#define PROCPERF_STR    "perf"

#endif /* SHAREDPERF_H */
