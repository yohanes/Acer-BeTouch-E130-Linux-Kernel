/*
 * linux/arch/arm/plat-pnx/pnx_ebi.c
 *
 * Description:  
 *
 * Created:      02.02.2010 15:27:57
 * Author:       jean-philippe foures (JFO), jean-philippe.foures-nonst@stericsson.com
 * Copyright (C) 2010 ST-Ericsson
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
 */

#include <linux/kernel.h>

#include <linux/types.h>
#include <linux/init.h>

#include <asm/io.h>

#ifdef CONFIG_EBI_BUS
#include <mach/ebi.h>

extern struct pnx_ebi_config pnx_ebi_init_tab[NR_EBI];


static int __init hyperion_ebi_init(void)
{
	int i;
	/* Set init value */
	printk(KERN_INFO "HYPERION EBI initialization\n");

	/* Configure EBI registers */
	for(i = 0; i < NR_EBI; i++)
	{
		/* Write Main config register */
		__raw_writel(pnx_ebi_init_tab[i].ebi_mainCfg.ebi_reg_value,
			     pnx_ebi_init_tab[i].ebi_mainCfg.ebi_reg_addr);
		/* Write Read config register */
		__raw_writel(pnx_ebi_init_tab[i].ebi_readCfg.ebi_reg_value,
			     pnx_ebi_init_tab[i].ebi_readCfg.ebi_reg_addr);
		/* Write write config register */
		__raw_writel(pnx_ebi_init_tab[i].ebi_writeCfg.ebi_reg_value,
			     pnx_ebi_init_tab[i].ebi_writeCfg.ebi_reg_addr);
		/* Write burst config register */
		__raw_writel(pnx_ebi_init_tab[i].ebi_burstCfg.ebi_reg_value,
			     pnx_ebi_init_tab[i].ebi_burstCfg.ebi_reg_addr);

	}
	printk(KERN_INFO "HYPERION EBI initialization done\n");


	return (0);
}

arch_initcall(hyperion_ebi_init);

#endif

