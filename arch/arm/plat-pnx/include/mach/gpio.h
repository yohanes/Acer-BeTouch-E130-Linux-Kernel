/*
 * linux/arch/arm/plat-pnx/include/mach/gpio.h
 *
 * PNX GPIO handling defines and functions
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __ASM_ARCH_PNX_GPIO_H
#define __ASM_ARCH_PNX_GPIO_H

#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/gpio.h>

#include <linux/errno.h>
#include <asm-generic/gpio.h>

/* GPIO bank description */
struct gpio_bank {
	void __iomem *gpio_base;
	void __iomem *mux_base;
	u16 irq;
	u16 virtual_irq_start;
	int method;
	u32 reserved_map;
	u32 suspend_wakeup;
	u32 saved_wakeup;
	spinlock_t lock;
	struct gpio_chip chip;
};


struct gpio_data {
	u32 nb_banks;
	struct gpio_bank *gpio_bank_desc;
};


/* GPIO Init configuration */

struct pnx_gpio_config {
	u32 gpio;
	u32 dir;
	u32 value;
};


/* list of GPIO */
enum PNX_GPIO_LIST {
	GPIO_A0 = 0,
	GPIO_A1,
	GPIO_A2,
	GPIO_A3,
	GPIO_A4,
	GPIO_A5,
	GPIO_A6,
	GPIO_A7,
	GPIO_A8,
	GPIO_A9,
	GPIO_A10,
	GPIO_A11,
	GPIO_A12,
	GPIO_A13,
	GPIO_A14,
	GPIO_A15,
	GPIO_A16,
	GPIO_A17,
	GPIO_A18,
	GPIO_A19,
	GPIO_A20,
	GPIO_A21,
	GPIO_A22,
	GPIO_A23,
	GPIO_A24,
	GPIO_A25,
	GPIO_A26,
	GPIO_A27,
	GPIO_A28,
	GPIO_A29,
	GPIO_A30,
	GPIO_A31,
	GPIO_B0,
	GPIO_B1,
	GPIO_B2,
	GPIO_B3,
	GPIO_B4,
	GPIO_B5,
	GPIO_B6,
	GPIO_B7,
	GPIO_B8,
	GPIO_B9,
	GPIO_B10,
	GPIO_B11,
	GPIO_B12,
	GPIO_B13,
	GPIO_B14,
	GPIO_B15,
	GPIO_B16,
	GPIO_B17,
	GPIO_B18,
	GPIO_B19,
	GPIO_B20,
	GPIO_B21,
	GPIO_B22,
	GPIO_B23,
	GPIO_B24,
	GPIO_B25,
	GPIO_B26,
	GPIO_B27,
	GPIO_B28,
	GPIO_B29,
	GPIO_B30,
	GPIO_B31,
	GPIO_C0,
	GPIO_C1,
	GPIO_C2,
	GPIO_C3,
	GPIO_C4,
	GPIO_C5,
	GPIO_C6,
	GPIO_C7,
	GPIO_C8,
	GPIO_C9,
	GPIO_C10,
	GPIO_C11,
	GPIO_C12,
	GPIO_C13,
	GPIO_C14,
	GPIO_C15,
	GPIO_C16,
	GPIO_C17,
	GPIO_C18,
	GPIO_C19,
	GPIO_C20,
	GPIO_C21,
	GPIO_C22,
	GPIO_C23,
	GPIO_C24,
	GPIO_C25,
	GPIO_C26,
	GPIO_C27,
	GPIO_C28,
	GPIO_C29,
	GPIO_C30,
	GPIO_C31,
	GPIO_D0,
	GPIO_D1,
	GPIO_D2,
	GPIO_D3,
	GPIO_D4,
	GPIO_D5,
	GPIO_D6,
	GPIO_D7,
	GPIO_D8,
	GPIO_D9,
	GPIO_D10,
	GPIO_D11,
	GPIO_D12,
	GPIO_D13,
	GPIO_D14,
	GPIO_D15,
	GPIO_D16,
	GPIO_D17,
	GPIO_D18,
	GPIO_D19,
	GPIO_D20,
	GPIO_D21,
	GPIO_D22,
	GPIO_D23,
	GPIO_D24,
	GPIO_D25,
	GPIO_D26,
	GPIO_D27,
	GPIO_D28,
	GPIO_D29,
	GPIO_D30,
	GPIO_D31,
	GPIO_E0,
	GPIO_E1,
	GPIO_E2,
	GPIO_E3,
	GPIO_E4,
	GPIO_E5,
	GPIO_E6,
	GPIO_E7,
	GPIO_E8,
	GPIO_E9,
	GPIO_E10,
	GPIO_E11,
	GPIO_E12,
	GPIO_E13,
	GPIO_E14,
	GPIO_E15,
	GPIO_E16,
	GPIO_E17,
	GPIO_E18,
	GPIO_E19,
	GPIO_E20,
	GPIO_E21,
	GPIO_E22,
	GPIO_E23,
	GPIO_E24,
	GPIO_E25,
	GPIO_E26,
	GPIO_E27,
	GPIO_E28,
	GPIO_E29,
	GPIO_E30,
	GPIO_E31,
    GPIO_F0,
	GPIO_F1,
	GPIO_F2,
	GPIO_F3,
	GPIO_F4,
	GPIO_F5,
	GPIO_F6,
	GPIO_F7,
	GPIO_F8,
	GPIO_F9,
	GPIO_F10,
	GPIO_F11,
	GPIO_F12,
	GPIO_F13,
	GPIO_F14,
	GPIO_F15,
	GPIO_F16,
	GPIO_F17,
	GPIO_F18,
	GPIO_F19,
	GPIO_F20,
	GPIO_F21,
	GPIO_F22,
	GPIO_F23,
	GPIO_F24,
	GPIO_F25,
	GPIO_F26,
	GPIO_F27,
	GPIO_F28,
	GPIO_F29,
	GPIO_F30,
	GPIO_F31,
	PNX_GPIO_COUNT,
};

enum PMU_GPIO_LIST {
	PMU_GPIO1 = PNX_GPIO_COUNT,
	PMU_GPIO2,
	PMU_GPIO3,
	PMU_GPIO4,
	GPIO_COUNT,
};

enum PNX_GPIO_MODE {
	GPIO_MODE_MUX0 = 0,
	GPIO_MODE_MUX1,
	GPIO_MODE_MUX2,
	GPIO_MODE_MUX3
};


#define GPIO_DIR_INPUT  1
#define GPIO_DIR_OUTPUT 0


extern int  pnx_gpio_init(void);	/* Call from board init only */
extern int  pnx_gpio_request(int gpio);
extern void pnx_gpio_free(int gpio);
extern int  pnx_gpio_set_mode(int gpio, int mode);
extern int  pnx_gpio_set_mode_gpio(int gpio);
extern int  pnx_gpio_set_direction(int gpio, int is_input);
extern int  pnx_gpio_write_pin(int gpio, int gpio_value);
extern int  pnx_gpio_read_pin(int gpio);
extern int  pnx_gpio_set_irq_debounce(int irq, int cycles);
extern int  pnx_gpio_set_irq_selection(int irq, int selection);
extern int  pnx_gpio_clear_irq(unsigned int irq);
extern int  pnx_gpio_acquire(struct gpio_chip *chip, unsigned offset);
extern void pnx_gpio_release(struct gpio_chip *chip, unsigned offset);


/*-------------------------------------------------------------------------*/

/* Wrappers for "new style" GPIO calls, using the new infrastructure
 * which lets us plug in FPGA, I2C, and other implementations.
 * *
 * The original PNX-specfic calls should eventually be removed.
 */


static inline int gpio_get_value(unsigned gpio)
{
	return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned gpio, int value)
{
	__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned gpio)
{
	return __gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned gpio)
{
	return __gpio_to_irq(gpio);
}

static inline int irq_to_gpio(unsigned irq)
{
	return EXTINT_TO_GPIO(irq);
}



#endif
