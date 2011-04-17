/*
 *  linux/arch/arm/plat-pnx/gpio.c
 *
 * Support functions for PNX GPIO
 *
 * Copyright (C) 2010 ST-Ericsson
 * Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/sysdev.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <asm/mach/irq.h>
#include <mach/scon.h>
#include <mach/extint.h>

/*
 * PN5220 GPIO/MUX registers
 * defined in asm/arch/registers.h
 */

#define PNX_GPIO_PINS_OFFSET	0
#define PNX_GPIO_OUTPUT_OFFSET	4
#define PNX_GPIO_DIR_OFFSET	8

#define PNX_MUX2_OFFSET		4

extern struct pnx_scon_config pnx_scon_init_config[SCON_REGISTER_NB];
extern struct pnx_extint_config pnx_extint_init_config[];
extern unsigned int pnx_modem_extint_nb;
extern struct pnx_gpio_config pnx_gpio_init_config[];
extern unsigned int pnx_modem_gpio_reserved[];
extern unsigned int pnx_modem_gpio_reserved_nb;
extern u32 gpio_to_configure;

extern struct gpio_bank *pnx_gpio_bank;
static struct gpio_bank *gpio_bank_desc;
static int gpio_bank_count;

static inline struct gpio_bank *get_gpio_bank(int gpio)
{
	/* 32 GPIOs per bank */
	return &(gpio_bank_desc[gpio >> 5]);
}

static inline int get_gpio_index(int gpio)
{
	/* TODO LPA add cpu test for future !!! */
	return gpio & 0x1f;
}

static int check_gpio(int gpio)
{
	int retval = ((unsigned int)gpio) < PNX_GPIO_COUNT;
	if (unlikely(!retval)) {
		printk(KERN_ERR "pnx-gpio: invalid GPIO %d\n", gpio);
		dump_stack();
	}
	return retval;
}

static inline int gpio_is_requested(struct gpio_bank *bank, unsigned long mask)
{
	return bank->reserved_map & mask;
}

static int check_gpio_requested(struct gpio_bank *bank, int index)
{
	int retval = gpio_is_requested(bank, 1 << index);
	if (unlikely(!retval)) {
		char c = 'A' + (bank - get_gpio_bank(0));
		printk(KERN_ERR "pnx-gpio: GPIO %c%d is not requested yet\n",
		       c, index);
		dump_stack();
	}
	return retval;
}

static int check_gpio_unrequested(struct gpio_bank *bank, int index)
{
	int retval = !gpio_is_requested(bank, 1 << index);
	if (unlikely(!retval)) {
		char c = 'A' + (bank - get_gpio_bank(0));
		printk(KERN_ERR "pnx-gpio: GPIO %c%d is already requested\n",
		       c, index);
		dump_stack();
	}
	return retval;
}

static int check_gpio_irq(int gpio_irq)
{
	int retval = ((unsigned int) gpio_irq) < NR_EXTINT;
	if (unlikely(!retval)) {
		printk(KERN_ERR "pnx-gpio: invalid GPIO-IRQ %d\n", gpio_irq);
		dump_stack();
	}
	return retval;
}

static void _set_gpio_direction(struct gpio_bank *bank, int gpio, int is_input)
{
	void __iomem *reg = bank->gpio_base;
	u32 l;

	/* select direction register */
	reg += PNX_GPIO_DIR_OFFSET;

	/* in register 0 = input, 1 = output */
	l = __raw_readl(reg);
	if (is_input)
		l &= ~(1 << gpio);
	else
		l |= (1 << gpio);
	__raw_writel(l, reg);
}

int pnx_gpio_set_direction(int gpio, int is_input)
{
	unsigned long flags, index;
	struct gpio_bank *bank;

	if (!check_gpio(gpio))
		return -EINVAL;

	bank = get_gpio_bank(gpio);
	index = get_gpio_index(gpio);
	if (!check_gpio_requested(bank, index))
		return -EINVAL;

	spin_lock_irqsave(&bank->lock, flags);
	_set_gpio_direction(bank, index, is_input);
	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}
EXPORT_SYMBOL(pnx_gpio_set_direction);

static void _set_gpio_mode(struct gpio_bank *bank, int gpio, int mode)
{
	void __iomem *reg = bank->mux_base;
	unsigned long flags;
	unsigned long l;

	/* select direction register */
	if(gpio >= 16) {
		reg += PNX_MUX2_OFFSET;
		gpio -= 16;
	}

	hw_raw_local_irq_save(flags);
	/* apply mux mode */
	/* width 2 bit */
	l = __raw_readl(reg);
	l &= ~(3 << (gpio * 2));
	l |= (mode << (gpio * 2));
	__raw_writel(l, reg);
	hw_raw_local_irq_restore(flags);
}

int pnx_gpio_set_mode(int gpio, int mode)
{
	struct gpio_bank *bank;
	int index;

	if (!check_gpio(gpio))
		return -EINVAL;

	bank = get_gpio_bank(gpio);
	index = get_gpio_index(gpio);
	if (!check_gpio_requested(bank, index))
		return -EINVAL;

	spin_lock(&bank->lock);
	_set_gpio_mode(bank, get_gpio_index(gpio), mode);
	spin_unlock(&bank->lock);

	return 0;
}
EXPORT_SYMBOL(pnx_gpio_set_mode);

int pnx_gpio_set_mode_gpio(int gpio)
{
	int muxmode = gpio >= GPIO_B0 ? GPIO_MODE_MUX1 : GPIO_MODE_MUX0;
	return pnx_gpio_set_mode(gpio, muxmode);
}
EXPORT_SYMBOL(pnx_gpio_set_mode_gpio);

static void _write_gpio_pin(struct gpio_bank *bank, int gpio, int gpio_value)
{
	void __iomem *reg = bank->gpio_base;
	unsigned long flags;
	unsigned long l = 0;

	reg += PNX_GPIO_OUTPUT_OFFSET;
	hw_raw_local_irq_save(flags);
	l = __raw_readl(reg);
	if (gpio_value)
		l |= 1 << gpio;
	else
		l &= ~(1 << gpio);
	__raw_writel(l, reg);
	hw_raw_local_irq_restore(flags);

}

static int pnx_gpio_to_extint(int gpio)
{
	int extint_idx;

	for (extint_idx =0; extint_idx < NR_EXTINT ; extint_idx ++)
		if (extint_to_gpio[extint_idx] == gpio)
			return extint_idx;

	return -1;
}

int pnx_gpio_write_pin(int gpio, int gpio_value)
{
	struct gpio_bank *bank;
	unsigned long index;

	if (!check_gpio(gpio))
		return -EINVAL;

	bank = get_gpio_bank(gpio);
	index = get_gpio_index(gpio);
	if (!check_gpio_requested(bank, index))
		return -EINVAL;

	spin_lock(&bank->lock);
	_write_gpio_pin(bank, index, gpio_value);
	spin_unlock(&bank->lock);

	return 0;
}
EXPORT_SYMBOL(pnx_gpio_write_pin);

int pnx_gpio_read_pin(int gpio)
{
	struct gpio_bank *bank;
	void __iomem *reg;
	u32 l = 0;
	int irq, index;

	if (!check_gpio(gpio))
		return -EINVAL;

	bank = get_gpio_bank(gpio);
	index = get_gpio_index(gpio);
	if (!check_gpio_requested(bank, index))
		return -EINVAL;

	/* check if the GPIO is used as extint */
	irq = pnx_gpio_to_extint(gpio);
	if (irq >= 0) {
		/* and if it's an alternate internal signal */
		/* (cf PNX67xx datasheet table 444)*/
		reg = (void __iomem*) EXTINT_CFGx(irq);
		l = __raw_readl(reg);
		if (l & EXTINT_SEL_ALTERNATE) {
			reg = (void __iomem*) EXTINT_SIGNAL_REG;
			return (__raw_readl(reg) & (1 << irq)) !=0;
		}
	}

	reg = bank->gpio_base;
	reg += PNX_GPIO_PINS_OFFSET;
	return (__raw_readl(reg) & (1 << index)) != 0;
}
EXPORT_SYMBOL(pnx_gpio_read_pin);

static int _pnx_gpio_request(struct gpio_bank *bank, int index)
{
	int retval = 0;
	unsigned long mask = 1 << index;
	unsigned long flags;

	spin_lock_irqsave(&bank->lock, flags);
	if (unlikely(!check_gpio_unrequested(bank, index)))
		retval = -EINVAL;
 	else
		bank->reserved_map |= mask;
	spin_unlock_irqrestore(&bank->lock, flags);

	return retval;
}

int pnx_gpio_acquire(struct gpio_chip *chip, unsigned offset)
{
	struct gpio_bank *bank = container_of(chip, struct gpio_bank, chip);

	return _pnx_gpio_request(bank, offset);
}

int pnx_gpio_request(int gpio)
{
	int index;
	struct gpio_bank *bank;

	if (!check_gpio(gpio))
		return -EINVAL;

	index = get_gpio_index(gpio);
	bank = get_gpio_bank(gpio);

	return _pnx_gpio_request(bank, index);
}
EXPORT_SYMBOL(pnx_gpio_request);

static void _pnx_gpio_free(struct gpio_bank *bank, int index)
{
	unsigned long mask = 1 << index;
	unsigned long flags;

	spin_lock_irqsave(&bank->lock, flags);
	if (likely(check_gpio_requested(bank, index)))
		bank->reserved_map &= ~mask;
	spin_unlock_irqrestore(&bank->lock, flags);
}

void pnx_gpio_release(struct gpio_chip *chip, unsigned offset)
{
	struct gpio_bank *bank = container_of(chip, struct gpio_bank, chip);

	return _pnx_gpio_free(bank, offset);
}

void pnx_gpio_free(int gpio)
{
	int index;
	struct gpio_bank *bank;

	if (!check_gpio(gpio))
		return;

	index = get_gpio_index(gpio);
	bank = get_gpio_bank(gpio);
	_pnx_gpio_free(bank, index);
}
EXPORT_SYMBOL(pnx_gpio_free);

/* New GPIO_GENERIC interface */

static int gpio_input(struct gpio_chip *chip, unsigned offset)
{
	struct gpio_bank *bank;
	unsigned long flags;

	bank = container_of(chip, struct gpio_bank, chip);
	spin_lock_irqsave(&bank->lock, flags);
	_set_gpio_direction(bank, offset, 1);
	spin_unlock_irqrestore(&bank->lock, flags);
	return 0;
}

static int gpio_get(struct gpio_chip *chip, unsigned offset)
{
	return pnx_gpio_read_pin(chip->base + offset);
}

static int gpio_output(struct gpio_chip *chip, unsigned offset, int value)
{
	struct gpio_bank *bank;
	unsigned long flags;

	bank = container_of(chip, struct gpio_bank, chip);
	spin_lock_irqsave(&bank->lock, flags);
	_write_gpio_pin(bank, offset, value);
	_set_gpio_direction(bank, offset, 0);
	spin_unlock_irqrestore(&bank->lock, flags);
	return 0;
}

static void gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct gpio_bank *bank;
	unsigned long flags;

	bank = container_of(chip, struct gpio_bank, chip);
	spin_lock_irqsave(&bank->lock, flags);
	_write_gpio_pin(bank, offset, value);
	spin_unlock_irqrestore(&bank->lock, flags);
}

static int gpio_2irq(struct gpio_chip *chip, unsigned offset)
{
	return pnx_gpio_to_extint(chip->base + offset);
}

/*
 * PNX EXTINT : only EXTINT 3 is managed by Linux
 * We need to unmask the GPIO bank interrupt as soon as possible to
 * avoid missing GPIO interrupts for other lines in the bank.
 * Then we need to mask-read-clear-unmask the triggered GPIO lines
 * in the bank to avoid missing nested interrupts for a GPIO line.
 * If we wait to unmask individual GPIO lines in the bank after the
 * line's interrupt handler has been run, we may miss some nested
 * interrupts.
 */
static void gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned long isr;
	unsigned long flags;
	unsigned int gpio_irq;

	/* LPA TBD */
	desc->chip->ack(irq);

	/* read status */
	hw_raw_local_irq_save ( flags );
	isr = __raw_readl(EXTINT_STATUS_REG) & __raw_readl(EXTINT_ENABLE3_REG);
	/* clear IRQ source(s)*/
	__raw_writel(~isr, EXTINT_STATUS_REG);
	hw_raw_local_irq_restore ( flags );

	gpio_irq = IRQ_COUNT;
	for (; isr != 0; isr >>= 1, gpio_irq++) {
		struct irq_desc *d;
		if (!(isr & 1))
			continue;
		d = irq_desc + gpio_irq;
#ifdef CONFIG_DEBUG_EXTINT
		printk(KERN_ERR "got something from EXTINT#%i line\n",
		       gpio_irq - IRQ_COUNT);
#endif
		desc_handle_irq(gpio_irq, d);
	}

#ifdef CONFIG_NKERNEL
	/*
	 * interrupt forwarding routine of osware masks INTC_IID_EXT2
	 * so we have to unmask it after irq handling
	 */
	hw_raw_local_irq_save ( flags );
	__raw_writel((1<<26 | 1<<16), INTC_REQUESTx(IRQ_EXTINT3));
	hw_raw_local_irq_restore ( flags );
#endif
}

static void gpio_ack_irq(unsigned int irq)
{
	unsigned int gpio_irq = irq - IRQ_COUNT;
	unsigned long flags;
	hw_raw_local_irq_save ( flags );
	__raw_writel( ~(1<< gpio_irq), EXTINT_STATUS_REG);
	hw_raw_local_irq_restore ( flags );
}

static void gpio_mask_irq(unsigned int irq)
{
	unsigned int gpio_irq = irq - IRQ_COUNT;
	__raw_writel(__raw_readl(EXTINT_ENABLE3_REG) & ~(1<< gpio_irq),
		     EXTINT_ENABLE3_REG);
}

static void gpio_unmask_irq(unsigned int irq)
{
	unsigned int gpio_irq = irq - IRQ_COUNT;
	__raw_writel(__raw_readl(EXTINT_ENABLE3_REG) | (1<< gpio_irq),
		     EXTINT_ENABLE3_REG);
}

int pnx_gpio_clear_irq(unsigned int irq)
{
	unsigned int gpio_irq;
	unsigned long flags;
	gpio_irq = irq - IRQ_COUNT;
	if (!check_gpio_irq(gpio_irq))
		return -EINVAL;
	hw_raw_local_irq_save ( flags );
	__raw_writel( ~(1<< gpio_irq), EXTINT_STATUS_REG);
	hw_raw_local_irq_restore ( flags );
	return 0;
}
EXPORT_SYMBOL(pnx_gpio_clear_irq);

int pnx_gpio_set_irq_debounce(int irq, int cycles)
{
	int gpio;
	struct gpio_bank *bank;
	void __iomem *reg;
	int mode;
	u32 l = 0;

	gpio = EXTINT_TO_GPIO(irq);
	irq -= IRQ_COUNT;
	if (!check_gpio_irq(irq))
		goto err;

	bank = get_gpio_bank(gpio);
	if (!check_gpio_requested(bank, get_gpio_index(gpio)))
		return -EINVAL;

	reg = (void __iomem*) EXTINT_CFGx(irq);
	l = __raw_readl(reg);

	mode=l&(3<<EXTINT_MODE_SHIFT);
	if ( mode==EXTINT_MODE_BYPASS )
		goto err;

	/* clear mode and set streching to debounce */
	if ( mode==EXTINT_MODE_STRETCHING ){
		l &= ~(3 << EXTINT_MODE_SHIFT);
		l |= EXTINT_MODE_DEBOUNCE;
	}
	/* clear and set the debounce field */
	l &= ~(7<<EXTINT_DEBOUNCE_SHIFT);
	l |= ((cycles & 0x7) << EXTINT_DEBOUNCE_SHIFT);
	__raw_writel(l,reg);

	return 0;
err:
	return -EINVAL;
}
EXPORT_SYMBOL(pnx_gpio_set_irq_debounce);

int pnx_gpio_set_irq_selection(int irq, int selection)
{
	int gpio, index;
	struct gpio_bank *bank;
	u32 l = 0;
	void __iomem *reg;

	gpio = EXTINT_TO_GPIO(irq);
	irq -= IRQ_COUNT;
	if (!check_gpio_irq(irq))
		return -EINVAL;

	bank = get_gpio_bank(gpio);
	index = get_gpio_index(gpio);
	if (!check_gpio_requested(bank, index))
		return -EINVAL;

	reg = (void __iomem*) EXTINT_CFGx(irq);
	l = __raw_readl(reg);

	if (selection == EXTINT_SEL_ALTERNATE)
		l |= EXTINT_SEL_ALTERNATE;
	else
		l &= ~EXTINT_SEL_ALTERNATE;
	__raw_writel(l,reg);

	return  0;
}
EXPORT_SYMBOL(pnx_gpio_set_irq_selection);

/*
 * -the level type set the bypass mode
 * -and by default the edge type select the stretching mode.
 * if you would a debounce you must defined your nb
 * cycle with pnx_set_gpio_debounce
 */
static int _set_gpio_triggering(int gpio_irq, int trigger)
{
	void __iomem *reg = (void __iomem*) EXTINT_CFGx(gpio_irq);
	u32 l = 0;

	l = __raw_readl(reg);
	l &= ~(3 << 6 | 1<<2);

	if (trigger == IRQ_TYPE_LEVEL_LOW)
		l |= EXTINT_POL_NEGATIVE;
	else if (trigger == IRQ_TYPE_LEVEL_HIGH)
		l |= EXTINT_POL_POSITIVE;
	else if(trigger == IRQ_TYPE_EDGE_RISING)
		l |= ( EXTINT_MODE_STRETCHING | EXTINT_POL_POSITIVE);
	else if (trigger == IRQ_TYPE_EDGE_FALLING)
		l |= (EXTINT_MODE_STRETCHING | EXTINT_POL_NEGATIVE);
	else if (trigger == IRQ_TYPE_EDGE_BOTH)
		l |= EXTINT_MODE_DUAL_EDGE;
	else
		goto err;

	__raw_writel(l, reg);

	return 0;
err:
	return -EINVAL;
}

static int gpio_irq_type(unsigned irq, unsigned type)
{
	unsigned gpio_irq;
	int retval;

	gpio_irq = irq - IRQ_COUNT;

	if (!check_gpio_irq(gpio_irq))
		return -EINVAL;

	if (type & (IRQF_TRIGGER_PROBE))
		return -EINVAL;

	retval = _set_gpio_triggering( gpio_irq, type);
	return retval;
}

static struct irq_chip gpio_irq_chip = {
	.ack		= gpio_ack_irq,
	.disable    = gpio_mask_irq,
	.enable     = gpio_unmask_irq,
	.mask		= gpio_mask_irq,
	.unmask		= gpio_unmask_irq,
	.set_type	= gpio_irq_type,
	/*.set_wake	= gpio_wake_enable,*/
};

static int initialized;

static int __init pnx_gpio_probe(struct platform_device *pdev)
{
	int i,j;
	int gpio = 0;
	struct gpio_bank *bank;
	struct gpio_data *data = pdev->dev.platform_data;
	unsigned long flags;

	initialized = 1;

	printk(KERN_INFO "PNX GPIO\n");
	gpio_bank_desc = data->gpio_bank_desc;
	gpio_bank_count = data->nb_banks;

	for (i = 0; i < gpio_bank_count; i++) {
		int gpio_count = 32; /* 32 GPIO per bank */
		bank = &gpio_bank_desc[i];
		bank->reserved_map = 0; /* must always be initialized */
		spin_lock_init(&bank->lock);

		/* check if bank is managed by PNX GPIO driver */
		if ((bank->gpio_base != 0) && (bank->mux_base != 0)) {
		bank->chip.request = pnx_gpio_acquire;
		bank->chip.free = pnx_gpio_release;
		bank->chip.direction_input = gpio_input;
		bank->chip.get = gpio_get;
		bank->chip.direction_output = gpio_output;
		bank->chip.set = gpio_set;
		bank->chip.to_irq = gpio_2irq;
		bank->chip.label = "gpio";
		bank->chip.base = gpio;

		bank->chip.ngpio = gpio_count;

		gpiochip_add(&bank->chip);
		}
		gpio += gpio_count;
	}

#ifdef CONFIG_MODEM_BLACK_BOX
	/* set init value */
	printk(KERN_INFO "PNX GPIO initialize SCON\n");

	/* configure MUX and PAD settings */
	for (i = 0; i< SCON_REGISTER_NB; i++)
		__raw_writel(pnx_scon_init_config[i].scon_reg_value,
			     pnx_scon_init_config[i].scon_reg_addr);

	/* configure GPIO direction and value */
	for (i=0; i < gpio_to_configure; i++) {
		int index;

		bank = get_gpio_bank(pnx_gpio_init_config[i].gpio);
		index = get_gpio_index(pnx_gpio_init_config[i].gpio);
		_set_gpio_direction(bank, index, pnx_gpio_init_config[i].dir);
		_write_gpio_pin(bank, index, pnx_gpio_init_config[i].value);
	}

	/* reserve GPIO used by Modem */
	for (i = 0; i < pnx_modem_gpio_reserved_nb; i++) {
		int index;

		bank = get_gpio_bank(pnx_modem_gpio_reserved[i]);
		index = get_gpio_index(pnx_modem_gpio_reserved[i]);
		bank->reserved_map |= (1 << index);
	}

	/* configure EXTINT used by modem */
	for (i = 0; i< pnx_modem_extint_nb; i++)
		__raw_writel(pnx_extint_init_config[i].reg_value,
			     pnx_extint_init_config[i].reg_addr);

	printk(KERN_INFO "PNX GPIO Driver\n");
#endif

	/* for extint */
	for (j = IRQ_COUNT; j < IRQ_COUNT + NR_EXTINT; j++) {
		set_irq_chip(j, &gpio_irq_chip);
		set_irq_handler(j, handle_simple_irq);
		set_irq_flags(j, IRQF_VALID);
	}

	hw_raw_local_irq_save ( flags );
	/* mask all EXT IRQ sources before registring handler */
	/* read status */
	j = __raw_readl(EXTINT_STATUS_REG) & __raw_readl(EXTINT_ENABLE3_REG);
	/* clear IRQ source(s)*/
	__raw_writel(j, EXTINT_STATUS_REG);

	__raw_writel(0, EXTINT_ENABLE3_REG);

	/* set irq in low level */
	set_irq_type(IRQ_EXTINT3, IRQF_TRIGGER_LOW);

	/* chained GPIO-IRQ on EXTINT3 */
	set_irq_chained_handler(IRQ_EXTINT3, gpio_irq_handler);
	hw_raw_local_irq_restore ( flags );

	return 0;
}

static struct platform_driver pnx_gpio_driver = {
	.probe		= pnx_gpio_probe,
	.remove		= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= "pnx-gpio",
	},
};

static struct sysdev_class pnx_gpio_sysclass = {
	.name		= "gpio",
	.suspend	= 0, /*pnx_gpio_suspend,*/
	.resume		= 0, /*pnx_gpio_resume,*/
};

static struct sys_device pnx_gpio_device = {
	.id		= 0,
	.cls		= &pnx_gpio_sysclass,
};

/*
 * This may get called early from board specific init
 * for boards that have interrupts routed via FPGA.
 */
int pnx_gpio_init(void)
{
	if (!initialized)
		return platform_driver_register(&pnx_gpio_driver);
	else
		return 0;
}

static int __init pnx_gpio_sysinit(void)
{
	int ret = 0;

	if (!initialized)
		ret = pnx_gpio_init();

	if (ret == 0) {
		ret = sysdev_class_register(&pnx_gpio_sysclass);
		if (ret == 0)
			ret = sysdev_register(&pnx_gpio_device);
	}

	return ret;
}

arch_initcall(pnx_gpio_sysinit);
