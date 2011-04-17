/*
 * Filename:     ebi.h
 *
 * Description:  EBI init structure used for Linux EBI bus configuration
 *
 * Version:      1.0
 * Created:      28.01.2010 17:00:09
 *
 * Author:       jean-philippe foures (JFO), jean-philippe.foures-nonst@
 *						stericsson.com
 * Copyright:    (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * changelog:
 *
 */

#ifndef __ARCH_ARM_EBI_H
#define __ARCH_ARM_EBI_H

/*============================================================================*/
/* the ebi config register structure                                          */
/*============================================================================*/
struct pnx_ebi_reg { 
  void __iomem* ebi_reg_addr;
  u32 ebi_reg_value;
};

/*============================================================================*/
/* the init data structure                                                    */
/*============================================================================*/
struct pnx_ebi_config { 
    struct	pnx_ebi_reg ebi_mainCfg;        /* Main conf. register for chip select   */
    struct	pnx_ebi_reg ebi_readCfg;        /* Read timing configuration register    */
	struct	pnx_ebi_reg ebi_writeCfg;       /* Write timing configuration register   */
    struct	pnx_ebi_reg ebi_burstCfg;		/* Burst/page configuration register     */
};

/*============================================================================*/
/* the EBI communication to RTK												  */
/*============================================================================*/
struct pnx_config_ebi_to_rtk { 
	u32	ebi_to_rtk_start;
    u32 	ebi_cs;        /* EBI chip select   */
    u32     ebi_phys_addr; /* EBI Physical address    */
	u32 	ebi_virt_addr; /* EBI Virtual address   */
    u32		ebi_size;	   /* size of EBI*/
	u32	ebi_to_rtk_end;
};


/*============================================================================*/
/*                        EBI (External Bus Interface)                        */
/*============================================================================*/
#define NR_EBI                                                                3

#endif
