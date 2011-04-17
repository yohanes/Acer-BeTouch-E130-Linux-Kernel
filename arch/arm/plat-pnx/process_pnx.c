/*
 * process_pnx.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author:  E. Vigier <emeric.vigier@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include "process_pnx.h"

#ifdef CONFIG_VMON_MODULE
unsigned int vmon_l2m_idle_switch;
EXPORT_SYMBOL(vmon_l2m_idle_switch);

u64 vmon_stamp(void)
{
	return pnx_rtke_read();
}
EXPORT_SYMBOL(vmon_stamp);
#endif


