/*
 *  linux/arch/arm/plat-pnx/include/mach/irqs.h
 *
 *  Copyright (C) 2010 ST-Ericsson
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
 */

#ifndef __ASM_ARCH_PNX_IRQS_H
#define __ASM_ARCH_PNX_IRQS_H

#if defined(CONFIG_ARCH_PNX67XX)
#define IRQ_COUNT      65
#endif

/* external IRQ definition EXTINT */
#define IRQ_EXTINT(num)       (IRQ_COUNT+(num))
#define EXTINT_NUM(irq)       ((irq)-IRQ_COUNT)

#define NR_EXTINT             24

#define NR_IRQS		(IRQ_COUNT+NR_EXTINT)


extern unsigned char extint_to_gpio[NR_EXTINT];

#define EXTINT_TO_GPIO(gpio_irq)      extint_to_gpio[gpio_irq-IRQ_COUNT]

#ifndef CONFIG_NKERNEL
#define hw_raw_local_irq_save       raw_local_irq_save 
#define hw_raw_local_irq_restore    raw_local_irq_restore
#endif

#ifndef __ASSEMBLY__
void /*__init */ pnx_init_irq(void);
void pnx_monitor_irq_enter(unsigned int irq);
void pnx_monitor_irq_exit(unsigned int irq);
#endif /* __ASSEMBLY__ */

#endif /* __ASM_ARCH_PNX_IRQS_H */
