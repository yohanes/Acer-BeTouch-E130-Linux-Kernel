/*
 *  linux/arch/arm/plat-pnx/include/mach/keypad.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ASMARM_ARCH_PNX_KEYPAD_H
#define ASMARM_ARCH_PNX_KEYPAD_H

#define ACER_L1_CHANGED

struct pnx_kbd_platform_data {
	int rows;    /* 1 <= rows <= 8 */
	int cols;    /* 1 <= cols <= rows */
	int *keymap;
	unsigned int keymapsize;
	unsigned int dbounce_delay;  /* in ms */
	/* unsigned int rep:1; FIXME not yet managed */

/* ACER Jen chang, 2010/04/11, IssueKeys:AU21.B-1, Modify GPIO setting for K2/K3 PR2 { */
#if (defined ACER_L1_CHANGED)
	int led1_gpio;
	int led1_pin_mux;
	int led2_gpio;
	int led2_pin_mux;
	int led3_gpio;
	int led3_pin_mux;
	int led4_gpio;
	int led4_pin_mux;
#endif
/* } ACER Jen Chang, 2010/04/11*/
};

#define KEY(col, row, val) (((col) << 28) | ((row) << 24) | (val))

#endif

