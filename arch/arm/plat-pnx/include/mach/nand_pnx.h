/**
 * @file:     nand_pnx.h
 *
 * @brief:    
 *
 * created:   03.02.2009 10:48:55
 *
 * @author:   Ludovic Barre (LBA), ludovic.barre@stn-wireless.com
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * changelog:
 *
 */

#ifndef  nand_pnx_H
#define  nand_pnx_H

struct pnx_nand_combo_info {
	int device_id;
	int manuf_id;
};

struct pnx_nand_timing_data {
	u32  timing_tac;
	u32  timing_tac_read;
};

struct pnx_nand_platform_data {
	struct        pnx_nand_timing_data *timing;
	unsigned int  *combo_index;
	struct        pnx_nand_combo_info *combo_info;
	struct  mtd_partition *parts;
	unsigned int  nr_parts;
	int usedma;
	int dma_data_ch;
	int dma_ecc_ch;
};

#endif   /* ----- #ifndef nand_pnx_H  ----- */
