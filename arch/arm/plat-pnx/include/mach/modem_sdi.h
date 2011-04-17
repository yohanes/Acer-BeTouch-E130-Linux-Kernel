/*
 *  linux/arch/arm/plat-pnx/include/mach/modem_sdi.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_MODEM_SDI_H
#define __ARCH_ARM_PNX_MODEM_SDI_H

#if defined (ACER_L1_K2) || defined (ACER_L1_K3) || defined (ACER_L1_AS1)
#define NBR_MEM_COMBOS 4
#elif defined(CONFIG_MACH_PNX67XX_V2_WAVEB_2GB) || \
	defined(CONFIG_MACH_PNX67XX_V2_WAVEC_2GB) || \
	defined(CONFIG_MACH_PNX67XX_WAVED)
#define NBR_MEM_COMBOS 2
#endif

struct pnx_modem_sdi_timing { 
  __u32    timingTrcd;             /* tRCD timing parameter                */
  __u32    timingTrc;              /* tRC timing parameter                 */
  __u32    timingTwtr;             /* tWTR timing parameter                */
  __u32    timingTwr;              /* tWR timing parameter                 */
  __u32    timingTrp;              /* tRP timing parameter                 */
  __u32    timingTras;             /* tRAS timing parameter                */
  __u32    timingTrrd;             /* tRRD timing parameter                */

  __u32    timingTrfc;             /* tRFC timing parameter                */
  __u32    timingTmrd;             /* tMRD timing parameter                */
  __u32    timingLtcy;             /* Latency timing parameter             */
  __u32    refresh;                /* Refresh parameter                    */
  __u32    timingTrtw;             /* tRTW timing parameter                */
  __u32    timingTxsr;             /* Exit self-refresh timing parameter   */
  __u32    timingTcke;             /* tCKE timing parameter                */
 
};

#endif
