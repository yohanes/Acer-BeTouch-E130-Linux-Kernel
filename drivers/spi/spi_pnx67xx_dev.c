/*
 * Driver for PNX67XX SPI Controllers
 *
 * Copyright (C) 2006 Atmel Corporation
 * Copyright (C) 2008 Elektrobit
 * Copyright (C) 2009 ST-Ericsson
 * Copyright (C) 2010 ST-Ericsson
 *
 * This driver implements spi controller driver for PNX67xx SoC. This code
 * is modified from atmel_spi.c by Haavard Skinnemoen <hskinnemoen@atmel.com>
 * and Philips Semiconductors source code.
 *
 * Contact: Sami Nurmenniemi <ext-sami.nurmenniemi@elektrobit.com>
 * Contact: Michel Jaouen <michel.jaouen@stericsson.com>
 *
 * To use this driver, platform resources must be set in
 * arch/arm/mach-pnx67xx/board-xxx.c like this:
 *
 * static struct resource someboard_spi1_resource = {
 * 		[0] = {
 *              .start          = SPI1_BASE,
 *              .end            = SPI1_BASE + 0x0FFF,
 *              .flags          = IORESOURCE_MEM,
 *      },
 *      [1] = {
 *              .start          = IRQ_SPI1,
 *              .end            = IRQ_SPI1,
 *              .flags          = IORESOURCE_IRQ,
 *      }
 * };
 * static u64 spi1DMAmask = ~(u32)0;
 * static struct platform_device someboard_spi1 = {
 * 	.name           = "pnx67xx_spi",
 * 	.id             = 1,
 *	.dev    = { x_mac
 *		.dma_mask           = &spi1DMAmask,
 *		.coherent_dma_mask  = 0xffffffff,
 *	},
 * 	.resource       = someboard_spi1_resource,
 * 	.num_resources  = 2,
 * };
 *
 * // If the bus is one-device bus, just use 0 as chip_select
 * static struct spi_board_info someboard_spi_board_info[] __initdata = {
 *	[0] = {
 *        .modalias       = "mydev_on_spi",
 *        .bus_num        = 1,
 *        .chip_select    = GPIO_A16,
 *        .max_speed_hz   = 12000000,
 *        },
 *	[1] = {
 *        .modalias       = "mydev2_on_spi",
 *        .bus_num        = 1,
 *        .chip_select    = GPIO_A17,
 *        .max_speed_hz   = 12000000,
 *        },
 * };
 *
 * static void __init xxx_init(void)
 * {
 * 		platform_device_register(&someboard_spi1);
 *		spi_register_board_info(someboard_spi_board_info,
 *                               ARRAY_SIZE(someboard_spi_board_info));
 * }
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/pm_qos_params.h>

#include <linux/io.h>
#include <mach/gpio.h>
#include <mach/dma.h>
#include <asm/dma.h>

#include "spi_pnx67xx.h"

#define PNX_SPI_CHIP_SELECTS	GPIO_COUNT
#define IRQ_PROCESS
#ifdef IRQ_PROCESS
static void pnx67xx_spi_transfer_done(struct pnx67xx_spi *pnx_spi);
#endif
/** @brief Get and Set up SPI mux pins
 * @param spi pnx67xx_spi structure
 */

static int pnx67xx_spi_mux_init(struct pnx67xx_spi *spi, const char *name)
{
	int err;

	err = gpio_request(spi->pdata->gpio_sclk, name);
	err |= gpio_request(spi->pdata->gpio_sdatin, name);
	err |= gpio_request(spi->pdata->gpio_sdatio, name);

	if (err)
		return -1;

	pnx_gpio_set_mode(spi->pdata->gpio_sclk, spi->pdata->gpio_mux_sclk);
	pnx_gpio_set_mode(spi->pdata->gpio_sdatin, spi->pdata->gpio_mux_sdatin);
	pnx_gpio_set_mode(spi->pdata->gpio_sdatio, spi->pdata->gpio_mux_sdatio);

	return 0;
}

/* @brief Set all SPI pins in GPIO mode and in INPUT direction to save power
*  @param spi pnx67xx_spi structure
*         param onoff : 1 put SPI pins in GPIO mode to save power
*                       0 put SPI pins in "SPI" mode
*/

static void pnx67xx_spi_mux_power_config(struct pnx67xx_spi *spi, int onoff)
{
	if (onoff) {
		/* set SPI pins in GPIO mode and INPUT in order to save power */
		gpio_direction_input(spi->pdata->gpio_sclk);
		gpio_direction_input(spi->pdata->gpio_sdatin);
		gpio_direction_input(spi->pdata->gpio_sdatio);
	} else {
		/* set all pins in SPI mode */
		pnx_gpio_set_mode(spi->pdata->gpio_sclk,
				  spi->pdata->gpio_mux_sclk);
		pnx_gpio_set_mode(spi->pdata->gpio_sdatin,
				  spi->pdata->gpio_mux_sdatin);
		pnx_gpio_set_mode(spi->pdata->gpio_sdatio,
				  spi->pdata->gpio_mux_sdatio);

		udelay(1000);
	}
}

static int pnx67xx_spi_mux_free(struct pnx67xx_spi *spi)
{
	gpio_free(spi->pdata->gpio_sclk);
	gpio_free(spi->pdata->gpio_sdatin);
	gpio_free(spi->pdata->gpio_sdatio);

	return 0;
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS (SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST)

/* Register access macros */

#define spi_readl(port, reg) \
		 __raw_readl((port)->regs + reg)
#define spi_writel(port, reg, value) \
		__raw_writel((value), (port)->regs + reg)
#define spi_readw(port, reg) \
		__raw_readw((port)->regs + reg)
#define spi_writew(port, reg, value) \
		 __raw_writew((value), (port)->regs + reg)

static inline void spi_write_frame(struct pnx67xx_spi *spi, u16 value)
{
	spi_writew(spi, SPI_ADR_OFFSET_FRM, value);
}

static inline void spi_write_stat(struct pnx67xx_spi *spi, u16 value)
{
	spi_writew(spi, SPI_ADR_OFFSET_STAT, value);
}

static inline void spi_write_global_register(struct pnx67xx_spi *spi,
					     u32 long_value)
{
	spi_writel(spi, SPI_ADR_OFFSET_GLOBAL, long_value);
}

/* basically at this point clocks on SPI  might be turned off, so
 * polling too long is a bad idea : try 10 times then carry on  */
static inline void spi_write_control_register(struct pnx67xx_spi *spi,
					      u32 long_value)
{
	int i = 0;
	spi_writel(spi, SPI_ADR_OFFSET_CON, long_value);
	while
		((long_value != spi_readl(spi, SPI_ADR_OFFSET_CON))
	       && (i++ < 10)
	    ) ;
}

static inline u32 spi_read_control_register(struct pnx67xx_spi *spi)
{
	return spi_readl(spi, SPI_ADR_OFFSET_CON);
}

static inline void spi_enable_generating_clock(struct pnx67xx_spi *spi)
{
	unsigned long control = spi_read_control_register(spi);
	control &= ~SPI_CON_SHIFT_OFF;
	spi_write_control_register(spi, control);
}

static inline void spi_stop_generating_clock(struct pnx67xx_spi *spi)
{
	unsigned long control = spi_read_control_register(spi);
	control |= SPI_CON_SHIFT_OFF;
	spi_write_control_register(spi, control);
}

void spi_set_ier(struct pnx67xx_spi *spi, int ie)
{
	unsigned long stat;
	stat = spi_readl(spi, SPI_ADR_OFFSET_IER);
	stat &= ~(SPI_IER_SPI_INTEOT | SPI_IER_SPI_INTTHR);
	stat |= (ie & (SPI_IER_SPI_INTEOT | SPI_IER_SPI_INTTHR));
	spi_writel(spi, SPI_ADR_OFFSET_IER, stat);

	/* wait for the interrupt enable to change */
	while
		(stat != spi_readl(spi, SPI_ADR_OFFSET_IER)) ;
}

static inline void spi_set_timer_ctrl(struct pnx67xx_spi *spi, long data)
{
	spi_writel(spi, SPI_ADR_OFFSET_TIMER_CTRL_REG, data);
}

/**
 * @brief enable spi ip
 *
 * @param spi
 */
static inline void spi_enable(struct pnx67xx_spi *spi)
{
	spi_write_global_register(spi, SPI_GLOBAL_SPI_ON);
}

/**
 * @brief disable spi ip
 *
 * @param spi
 */
static inline void spi_disable(struct pnx67xx_spi *spi)
{
	spi_write_global_register(spi, 0);
}

void spi_set_thr_mode_off(struct pnx67xx_spi *spi)
{
	unsigned long control = spi_read_control_register(spi);
	control &= ~SPI_CON_THR;
	spi_write_control_register(spi, control);
}

static inline int spi_read_stat(struct pnx67xx_spi *spi)
{
	return spi_readl(spi, SPI_ADR_OFFSET_STAT);
}

int spi_read_timer_stat(struct pnx67xx_spi *spi)
{
	return spi_readl(spi, SPI_ADR_OFFSET_TIMER_STATUS_REG);
}

static inline u16 spi_read_data(struct pnx67xx_spi *spi)
{
	return spi_readw(spi, SPI_ADR_OFFSET_DAT);
}

/** @brief Read data from SPI fifo
 *
 * @param spi pnx67xx spi data
 * @param buf Where to write data to
 * @param len How many bytes to read
 */
static inline void spi_read_buf(struct pnx67xx_spi *spi, u16 * buf, int len)
{
	int i;
	u8 *buf_tmp = (u8 *) buf;
	u8 factor = 0;

	factor = (spi->bits_per_word == 16 ? 2 : 1);
	if (spi->bits_per_word == 16) {
		for (i = 0; i < (len / factor); i++)
			*buf++ = spi_read_data(spi);
	} else
		for (i = 0; i < (len / factor); i++)
			*buf_tmp++ = (u8) spi_read_data(spi);

}

/** @brief Reset SPI controller
 *
 * Reset SPI controller
 *
 * @param spi PNX67xx spi data
 * @param leave_on 1 = leave controller on, 0 = turn it off
 */
static void pnx67xx_spi_hw_reset(struct pnx67xx_spi *spi, bool leave_on)
{
	u16 command = 0;

	if (leave_on)
		command = SPI_GLOBAL_SPI_ON;

	/* Set SPI controller on and reset it for 1ms */
	spi_write_global_register(spi, SPI_GLOBAL_BLRES_SPI + command);
	mdelay(1);
	spi_write_global_register(spi, command);
}

static inline void spi_write_data(struct pnx67xx_spi *spi, u16 data)
{
	spi_writew(spi, SPI_ADR_OFFSET_DAT, data);
}

/* ============================================================================
 * QOS & timer
 * ============================================================================
 */
/** @brief Set spi parent clock
 *
 * @param spi PNX67xx spi data
 */

enum t_SPI_pm_qos_status {
	PNXSPI_PM_QOS_DOWN,
	PNXSPI_PM_QOS_HALT,
	PNXSPI_PM_QOS_UP,
};

enum t_SPI_pm_qos_rate {
	PNXSPI_PM_QOS_STOP,
	PNXSPI_PM_QOS_XFER
};

static void pnx67xx_spi_pm_qos_up(struct pnx67xx_spi *spi)
{
	/* transfer will start */
	spi->pm_qos_rate = PNXSPI_PM_QOS_XFER;
	wmb();
	/* increase hclk2 if needed */
	if (spi->pm_qos_status == PNXSPI_PM_QOS_DOWN) {
		pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
					  (char *)spi->pdev->name, 104);
		pnx67xx_spi_mux_power_config(spi, 0);
		mod_timer(&(spi->pm_qos_timer), (jiffies + HZ / 10));
	}
	spi->pm_qos_status = PNXSPI_PM_QOS_UP;
}

/** @brief release spi parent clock
 *
 * @param spi PNX67xx spi data
 */
static void pnx67xx_spi_pm_qos_down(struct pnx67xx_spi *spi)
{
	pnx67xx_spi_mux_power_config(spi, 1);
	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
				  (char *)spi->pdev->name,
				  PM_QOS_DEFAULT_VALUE);
	spi->pm_qos_status = PNXSPI_PM_QOS_DOWN;
	spi->pm_qos_rate = PNXSPI_PM_QOS_STOP;
}

/** @brief check that spi parent clock is always required and release it if not
 *
 * @param spi PNX67xx spi data
 */
static void pnx67xx_spi_pm_qos_timeout(unsigned long arg)
{
	struct pnx67xx_spi *spi = (struct pnx67xx_spi *)arg;

	if (spi->pm_qos_rate == PNXSPI_PM_QOS_STOP) {
		if (spi->pm_qos_status == PNXSPI_PM_QOS_HALT) {
			pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT,
						  (char *)spi->pdev->name,
						  PM_QOS_DEFAULT_VALUE);
			spi->pm_qos_status = PNXSPI_PM_QOS_DOWN;
		} else {
			spi->pm_qos_status = PNXSPI_PM_QOS_HALT;
			mod_timer(&(spi->pm_qos_timer), (jiffies + HZ / 10));
		}
	} else
		mod_timer(&(spi->pm_qos_timer), (jiffies + HZ / 10));
}

/** @brief release spi parent clock
 *
 * @param spi PNX67xx spi data
 */
static void pnx67xx_spi_pm_qos_halt(struct pnx67xx_spi *spi)
{
	/* transfer is stopped */
	spi->pm_qos_rate = PNXSPI_PM_QOS_STOP;
}

/** @brief Write data to SPI fifo
 *
 * @param spi pnx67xx spi data
 * @param buf Where to read data from
 * @param len How many bytes to write
 */
static inline void spi_write_buf(struct pnx67xx_spi *spi, u16 * buf, int len)
{
	int i;
	u8 *buf_tmp = (u8 *) buf;
	u8 factor = 0;
	factor = (spi->bits_per_word == 16 ? 2 : 1);
	if (spi->bits_per_word == 16) {
		for (i = 0; i < (len / factor); i++)
			spi_write_data(spi, *buf++);
	} else {
		for (i = 0; i < (len / factor); i++)
			spi_write_data(spi, (u16) (*buf_tmp++));
	}
}

static int pnx67xx_spi_hw_init(struct pnx67xx_spi *spi)
{
	u32 control;

	pnx67xx_spi_hw_reset(spi, 1);

	/* Set SPI controller to work as master */
	/* TODO: LDINS should propably be used so that chip enable is no longer
	 * needed */
	control = SPI_CON_MS | SPI_CON_SPI_MODE0 | SPI_CON_RATE_13 |
	    SPI_CON_THR | SPI_CON_SPI_BIDIR;
	control |= (SPI_INITIAL_BITWIDTH - 1) << SPI_CON_BITNUM_SHIFT;
	printk(KERN_WARNING " SETTING CONTROL TO: %x\n", control);
	spi_write_control_register(spi, control);

	/* disable specific SPI peripheral interrupts,
	   enable general SPI  peripheral interrupts
	   through the SPI timer HW */
	/* in this mode SPI EOT is always used */
	spi_set_ier(spi, SPI_IER_SPI_INTEOT);
	spi_set_timer_ctrl(spi, SPI_TIMER_CTRL_REG_PIRQE);
	spi_set_thr_mode_off(spi);	/* THR is always set to low */

	return 0;
}
static inline void set_tx_stop_clk(struct pnx67xx_spi *pnx_spi)
{
	unsigned long control;
	/*control =  pnx_spi->control_val; */
	control = spi_read_control_register(pnx_spi);
	control |= SPI_CON_RxTx;
	control |= SPI_CON_SHIFT_OFF;
	spi_write_control_register(pnx_spi, control);
}

static inline void set_rx_stop_clk(struct pnx67xx_spi *pnx_spi)
{
	unsigned long control;
	/*control =  pnx_spi->control_val; */
	control = spi_read_control_register(pnx_spi);
	control &= ~SPI_CON_RxTx;
	control |= SPI_CON_SHIFT_OFF;
	spi_write_control_register(pnx_spi, control);
}

/** @brief Setup spi gpio
 *
 * Reserve given gpio and make it output
 *
 * @param gpio Number of gpio
 * @return 0 = success, -1 = failure
 */
static int pnx67xx_spi_setup_gpio(int gpio, int state, const char *name)
{
	if (gpio == NO_CHIP_SELECT)
		return 0;
	if (gpio_request(gpio, name) != 0)
		return -1;
	gpio_direction_output(gpio, state);
	return 0;
}

/** @brief Release spi gpio
 *
 * @param gpio Number of gpio to release
 */
static void pnx67xx_spi_release_gpio(int gpio)
{
	gpio_free(gpio);
}

#define SPI_BLOCK_CLOCK		104000000

/** @brief Setup spi clock
 *
 * @param spi PNX67xx spi data
 * @param clock Clock speed to set
 */
static void pnx67xx_spi_set_clock(struct pnx67xx_spi_controller_state *cs,
				  u32 clock)
{
	int factor;
	u32 spi_block_clock;
	u32 control_val = cs->control_val;
	u32 pollmax = 0;

	/* get hold of the adapter private data pointer */
#if defined(CONFIG_SPI_PNX_HIGHEST_CLOCK)
	&&(CONFIG_SPI_PNX_HIGHEST_CLOCK > 0)
	if (clock > CONFIG_SPI_PNX_HIGHEST_CLOCK) {
		printk(KERN_WARNING "SPI bitclock request %d exceeds limit %d\n",
		       clock, CONFIG_SPI_PNX_HIGHEST_CLOCK);
		clock = CONFIG_SPI_PNX_HIGHEST_CLOCK;
	}
#endif

#if 1
	spi_block_clock = SPI_BLOCK_CLOCK;
#else /* FIXME get clock from platform data ? */
	/* limit fastest transfer */
	MESSAGE("Setting clock to %d Hz\n", clock);
	if (platdat->clk)
		spi_block_clock = clk_get_rate(platdat->clk);
	else {
		printk(KERN_ERR "spi-pnx: Cannot locate core clock"
			 "for SPI so using 104MHz\n");
		spi_block_clock = SPI_BLOCK_CLOCK;
	}
#endif

	/* See 11.28.5.2 from PNX67xx datasheet
	 * Rate = pclk/(2*sclk)-1 */
	factor = spi_block_clock / clock;
	/* rounded inverse of adap->clk/((2* factor)+2) */

	/* just avoiding negative results */
	if (factor > 1)
		factor = (factor - 1) / 2;

	/* limit slowest transfer */
	if (factor > SPI_CON_RATE_MASK)
		factor = SPI_CON_RATE_MASK;

	factor &= SPI_CON_RATE_MASK;/* change to allow dynamic clock */
	/* Change the clock */
	control_val = control_val & ~SPI_CON_RATE_MASK;
	control_val |= factor;
	/* compute max packetsize for polling 20us */
	pollmax = clock * 20;
	pollmax = pollmax / 1000000;
	cs->pollsize_thread = pollmax / 8;
	/* polling in it is allowed up to 5 us */
	cs->pollsize_it = pollmax / 32;
	printk("Max Packet for Polling thread%d, it %d\n", cs->pollsize_thread,
	       cs->pollsize_it);
	cs->control_val = control_val;
}

/** @brief Setup spi mode
 *
 * @param spi PNX67xx spi data
 * @param mode Mode to set
 */
static void pnx67xx_spi_set_mode(struct pnx67xx_spi_controller_state *cs,
				 u16 mode)
{
	u32 control_val;

	/* get hold of the adapter private data pointer */
	if (mode & ~(SPI_MODE(3) | SPI_LSB_FIRST)) {
		printk(KERN_ERR "%s: called with invalid argument 0x%x\n",
		       __func__, mode);
		return;
	}

	control_val =
	    cs->control_val & ~(SPI_CON_SPI_MODE_MASK | SPI_CON_SPI_MSB);
	control_val |= (mode & 3) << SPI_CON_MODE_SHIFT;
	if (mode & SPI_LSB_FIRST)
		control_val |= SPI_CON_SPI_MSB;
	cs->control_val = control_val;
}

/** @brief Setup spi bitwidth
 *
 * @param spi PNX67xx spi data
 * @param bidwidth Bit width to set
 */
static void pnx67xx_spi_set_bitwidth(struct pnx67xx_spi_controller_state *cs,
				     u16 bitwidth)
{
	u32 control_val;

	if (bitwidth > 16) {
		printk(KERN_ERR
		       "%s: bitwidth %d greater than 16 not supported\n",
		       __func__, bitwidth);
		return;
	}
	if (bitwidth < 1) {
		printk(KERN_ERR "%s: Zero bitwidth is not allowed\n", __func__);
		return;
	}

	control_val = cs->control_val & ~SPI_CON_BITNUM_MASK;
	control_val |=
	    ((bitwidth - 1) << SPI_CON_BITNUM_SHIFT) & SPI_CON_BITNUM_MASK;
	cs->control_val = control_val;
}

/** @brief Activate chip select
 *
 * @param spi Kernel spi data
 */
static void cs_activate(struct spi_device *spi)
{
	unsigned active = spi->mode & SPI_CS_HIGH;

	if (spi->chip_select == NO_CHIP_SELECT)
		return;
	__gpio_set_value(spi->chip_select, active);
}

/** @brief Deactivate chip select
 *
 * @param spi Kernel spi data
 */
static void cs_deactivate(struct spi_device *spi)
{
	unsigned active = spi->mode & SPI_CS_HIGH;

	if (spi->chip_select == NO_CHIP_SELECT)
		return;

	__gpio_set_value(spi->chip_select, !active);
}

#define DMAU_Cx_SRC_ADDR(c)	\
(DMAU_BASE + DMAU_C0_SRC_ADDR_OFFSET + (c) * 0x20)
#define DMAU_Cx_CONF_ADDR(c)	(DMAU_Cx_SRC_ADDR(c) + 0x10)
#define DMAU_Cx_CONFIG(c)       DMAU_Cx_SRC_ADDR(c)
/** @brief Unmap SPI transfer from DMA capable memory
 *
 * @param pnx_spi Pointer to SPI data
 * @param xfer Transfer to map
 */
static void pnx67xx_spi_dma_unmap_xfer(struct pnx67xx_spi *pnx_spi,
				       struct spi_transfer *xfer)
{
	u8 factor = 0;
	factor = (pnx_spi->bits_per_word == 16 ? 2 : 1);

	if (xfer->len < (DMA_LEN_THRESHOLD * factor))
		return;

	if (xfer->tx_dma != INVALID_DMA_ADDRESS)
		dma_unmap_single(&pnx_spi->master->dev, xfer->tx_dma,
				 xfer->len, DMA_TO_DEVICE);
	else if (xfer->rx_dma != INVALID_DMA_ADDRESS)
		dma_unmap_single(&pnx_spi->master->dev, xfer->rx_dma,
				 xfer->len, DMA_FROM_DEVICE);
}

static int pnx67xx_spi_next_xfer(struct pnx67xx_spi *pnx_spi,
				 struct spi_message *msg);

/**
 * @brief Start message transfer
 *
 * @param pnx_spi
 */
static int pnx67xx_spi_next_message(struct pnx67xx_spi *pnx_spi)
{
	struct spi_message *msg;
	struct spi_device *spi;
	struct pnx67xx_spi_controller_state *cs;

	BUG_ON(pnx_spi->current_transfer);

	msg = list_entry(pnx_spi->queue.next, struct spi_message, queue);
	spi = msg->spi;
	cs = (struct pnx67xx_spi_controller_state *)spi->controller_state;
	msg->status = -EINPROGRESS;
	msg->actual_length = 0;
	/* select chip if it's not still active */
	if (pnx_spi->stay) {
		if (pnx_spi->stay != spi) {
			cs_deactivate(pnx_spi->stay);
			pnx_spi->pollsize_it = cs->pollsize_it;
			pnx_spi->pollsize_thread = cs->pollsize_thread;
			spi_write_control_register(pnx_spi, cs->control_val);
			/* pnx_spi->control_val=cs->control_val; */
			cs_activate(spi);
		}
		pnx_spi->stay = NULL;
	} else {
		pnx_spi->pollsize_it = cs->pollsize_it;
		pnx_spi->pollsize_thread = cs->pollsize_thread;
		spi_write_control_register(pnx_spi, cs->control_val);
		/* pnx_spi->control_val=cs->control_val; */
		cs_activate(spi);
	}
	return pnx67xx_spi_next_xfer(pnx_spi, msg);
}

/** @brief Function that gets called after there are no more bytes to
 * transfer in the message
 *
 * @param master SPI master
 * @param pnx_spi Pointer to SPI data
 * @param msg Transferred message
 * @param status Message status
 * @param stay Should the chip stay selected (1=yes)
 */
static int
pnx67xx_spi_msg_done(struct pnx67xx_spi *pnx_spi,
		     struct spi_message *msg, int status, int stay)
{
	unsigned long flags;
	if (!stay || status < 0)
		cs_deactivate(msg->spi);
	else
		pnx_spi->stay = msg->spi;
	spin_lock_irqsave(&pnx_spi->lock, flags);
	list_del(&msg->queue);
	spin_unlock_irqrestore(&pnx_spi->lock, flags);
	msg->status = status;
	/* message complete can do pnx67xx_spi_transfer
	 * in interruption can be called in mean time
	 * to queue a tranfer this message should not
	 * start the transfer due transfer state */
	BUG_ON(pnx_spi->transfer_state == NO_TRANSFER);
	msg->complete(msg->context);
	pnx_spi->current_transfer = NULL;
	/* continue if needed */
	spin_lock_irqsave(&pnx_spi->lock, flags);
	if (list_empty(&pnx_spi->queue) || pnx_spi->stopping) {
		/* end of transfert, disable clock and set SPI
		 * pin in GPIO mode NO TRANFER state and clock
		 * gating are critical in term of race condition
		 * they are protected by the spin lock */
		pnx_spi->transfer_state = NO_TRANSFER;
		clk_disable(pnx_spi->clk);
		pnx67xx_spi_pm_qos_halt(pnx_spi);
		spin_unlock_irqrestore(&pnx_spi->lock, flags);
		return 0;
	} else {
		spin_unlock_irqrestore(&pnx_spi->lock, flags);
		return pnx67xx_spi_next_message(pnx_spi);
	}
}

/** @brief DMA transfer complete
 *
 * Called by Kernel when dma transfer is complete
 * @param channel Number of channel that completed
 * @param cause DMA_TC_INT or DMA_ERR_INT
 * @param data Pointer to data
 */
static void spi_pnx_dma_callback(int channel, int cause, void *data)
{
	struct spi_master *master = (struct spi_master *)data;
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);

	/* test possible error */
	/* fix me a better handling must be done */
	if (cause & DMA_ERR_INT)
		printk(KERN_ERR "SPI dma call : DMA error \n");
	/* If SPI has already completed, schedule
	 * transfer complete tasklet
	 * clear condition relative to spi ip
	 * because this interrupt condition is always active */
	{
		u16 stat = spi_read_stat(pnx_spi);
		spi_write_stat(pnx_spi,
			       (stat | SPI_STAT_SPI_INTCLR) &
			       ~SPI_STAT_SPI_EOT);
	}
#ifdef IRQ_PROCESS
	pnx_spi->pollsize = &pnx_spi->pollsize_it;
	pnx67xx_spi_transfer_done(pnx_spi);
#else
	queue_work(pnx_spi->workqueue, &pnx_spi->xferdone);
#endif
}

/** @brief Request given DMA channel
 *
 * @param pnx_spi Pointer to SPI data
 * @return 0=success, <0 on failure
 */
static int pnx67xx_spi_request_dma_channel(struct spi_master *master)
{
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);
	pnx_spi->channel = pnx_request_channel("spi_pnx", -1,
					       spi_pnx_dma_callback,
					       (void *)master);
	if ((pnx_spi->channel == -EINVAL) || (pnx_spi->channel == -ENODEV)) {
		printk(KERN_WARNING "DMA channel allocation failed\n");
		return pnx_spi->channel;
	}
	printk(KERN_INFO "DMA channel for SPI: %d\n", pnx_spi->channel);
	return 0;
}

/** @brief Free given DMA channel
 *
 * @param spi Kernel spi data
 */
static void pnx67xx_spi_free_dma_channel(struct pnx67xx_spi *spi)
{
	pnx_free_channel(spi->channel);
}

/** @brief Enable DMA channel
 *
 * @param spi Kernel spi data
 */
static inline int pnx67xx_spi_dma_ch_enable(struct pnx67xx_spi *spi)
{
	spi->channel_enable_counter++;
	return pnx_dma_ch_enable(spi->channel);
}

/** @brief Disable DMA channel cleanly
 *
 * See section 11.9.6.6 on PNX67xx datasheet
 * @param dma_channel Channel to disable
 */
static inline void pnx67xx_spi_channel_clean_disable(int dma_channel)
{
	u32 reg;
	int count = 0;

	reg = __raw_readl(DMAU_Cx_CONFIG(dma_channel));
	reg &= ~1;
	__raw_writel(reg, DMAU_Cx_CONFIG(dma_channel));
	reg &= ~0x40001;
	__raw_writel(reg, DMAU_Cx_CONFIG(dma_channel));
	while (__raw_readl(DMAU_Cx_CONFIG(dma_channel)) & 0x20000) {
		count++;
		if (count > 1000)
			break;
	}
}

/** @brief Disable DMA channel
 *
 * @param spi Kernel spi data
 */
static inline int pnx67xx_spi_dma_ch_disable(struct pnx67xx_spi *spi)
{
	if (spi->channel_enable_counter <= 0) {
		dev_err(&spi->pdev->dev,
			"Trying to disable unenabled channel,\
			would cause DMAU to "
			"be disabled\n");
		return -EINVAL;
	}
	spi->channel_enable_counter--;
	return pnx_dma_ch_disable(spi->channel);
}

/**
 * @brief build dma configuration for tx transaction
 *
 * @param pnx_spi
 */
static void pnx67xx_build_dma_tx(struct pnx67xx_spi *pnx_spi)
{
	struct pnx_dma_ch_config ch_cfg;
	struct pnx_dma_ch_ctrl ch_ctrl;
	int width = (pnx_spi->bits_per_word == 8 ? WIDTH_BYTE : WIDTH_HWORD);
	u32 iostart = pnx_spi->phys_addr + SPI_ADR_OFFSET_DAT;
	int err;
	if (pnx_spi->txdone)
		return;
	pnx_spi->txdone = 1;
	memset(&pnx_spi->cfgtx, 0, sizeof(struct pnx_dma_config));
	pnx_spi->cfgtx.src_addr = 0;
	pnx_spi->cfgtx.dest_addr = (u32) (iostart);
	/* Must be _DMA in master mode - see 11.28.5.6 in PNX67xx manual */
	ch_cfg.flow_cntrl = FC_MEM2PER_DMA;
	ch_cfg.dest_per = PNX_SPI_PER(pnx_spi->pdev->id);
	ch_cfg.src_per = 0;
	ch_ctrl.di = 0;
	ch_ctrl.si = 1;
	ch_ctrl.dwidth = width;
	ch_ctrl.swidth = width;

	/*       - performing a DMA AHB1 to AHB2 (and the reverse)
	 *       has a higher performance
	 *     than acting on the same AHB layer (AHB2 in our case).
	 *     But there's a bug SWIFT_PR_CR 8964. Today, multi-layers DMA
	 *     transfers are 4 times worse than mono layer.
	 *     Today, this PR will not be solved for PRISM3A.
	 */
	ch_ctrl.src_ahb1 = 1;
	ch_ctrl.dest_ahb1 = 1;
	ch_ctrl.tr_size = 0;
	/* even ITC is not meaning full there */
	ch_cfg.itc = 0;
	ch_cfg.halt = 0;
	ch_cfg.active = 1;
	ch_cfg.lock = 0;

	ch_cfg.ie = 0;
	ch_ctrl.tc_mask = 1;
	ch_ctrl.cacheable = 0;
	ch_ctrl.bufferable = 0;
	ch_ctrl.priv_mode = 1;
	ch_ctrl.dbsize = 1;
	ch_ctrl.sbsize = 1;
	err = pnx_dma_pack_config(&ch_cfg, &pnx_spi->cfgtx.ch_cfg);
	if (err < 0)
		goto out;
	pnx_dma_pack_control(&ch_ctrl, &pnx_spi->cfgtx.ch_ctrl);
	if (err < 0)
		if (err != -E2BIG)
			goto out;
	return;
out:
	dev_err(&pnx_spi->pdev->dev, "config spi incorrect \n");

	return;
}

/**
 * @brief build dma configuration for rx transaction
 *
 * @param pnx_spi
 */
static void pnx67xx_build_dma_rx(struct pnx67xx_spi *pnx_spi)
{
	struct pnx_dma_ch_config ch_cfg;
	struct pnx_dma_ch_ctrl ch_ctrl;
	int width = (pnx_spi->bits_per_word == 8 ? WIDTH_BYTE : WIDTH_HWORD);
	u32 iostart = pnx_spi->phys_addr + SPI_ADR_OFFSET_DAT;
	int err;
	if (pnx_spi->rxdone)
		return;
	pnx_spi->rxdone = 1;
	memset(&pnx_spi->cfgrx, 0, sizeof(struct pnx_dma_config));
	pnx_spi->cfgrx.dest_addr = 0;
	pnx_spi->cfgrx.src_addr = (u32) (iostart);
	ch_cfg.flow_cntrl = FC_PER2MEM_DMA;
	ch_cfg.itc = 1;
	ch_cfg.src_per = PNX_SPI_PER(pnx_spi->pdev->id);
	ch_cfg.dest_per = 0;
	ch_ctrl.di = 1;
	ch_ctrl.si = 0;
	ch_ctrl.dwidth = width;
	ch_ctrl.swidth = width;
	ch_cfg.halt = 0;
	ch_cfg.active = 1;
	ch_cfg.lock = 0;

	ch_cfg.ie = 0;
	ch_ctrl.tc_mask = 1;
	ch_ctrl.cacheable = 0;
	ch_ctrl.bufferable = 0;
	ch_ctrl.priv_mode = 1;
	ch_ctrl.dbsize = 1;
	ch_ctrl.sbsize = 1;
	ch_ctrl.src_ahb1 = 1;
	ch_ctrl.dest_ahb1 = 1;
	ch_ctrl.tr_size = 0;
	err = pnx_dma_pack_config(&ch_cfg, &pnx_spi->cfgrx.ch_cfg);
	if (err < 0)
		goto out;
	err = pnx_dma_pack_control(&ch_ctrl, &pnx_spi->cfgrx.ch_ctrl);
	if (err < 0)
		if (err != -E2BIG)
			goto out;
	return;
out:
	dev_err(&pnx_spi->pdev->dev, "config spi incorrect \n");
	return;
}

static inline int pnx67xx_spi_tx_dma(struct pnx67xx_spi *pnx_spi, int addr,
				     int len)
{
	int err;
	int dma_channel = pnx_spi->channel;
	struct pnx_dma_config *cfg;
	cfg = &pnx_spi->cfgtx;
	cfg->src_addr = addr;
	cfg->ch_ctrl &= ~DMA_TR_SIZE_MAX;
	cfg->ch_ctrl |= len & DMA_TR_SIZE_MAX;
	err = pnx_config_channel(dma_channel, cfg);
	return err;
}

static inline int pnx67xx_spi_rx_dma(struct pnx67xx_spi *pnx_spi, int addr,
				     int len)
{
	int err;
	int dma_channel = pnx_spi->channel;
	struct pnx_dma_config *cfg;
	cfg = &pnx_spi->cfgrx;
	cfg->dest_addr = addr;
	cfg->ch_ctrl &= ~DMA_TR_SIZE_MAX;
	cfg->ch_ctrl |= len & DMA_TR_SIZE_MAX;
	err = pnx_config_channel(dma_channel, cfg);
	return err;
}

void printregs(int channel)
{
	unsigned long addr;
	for (addr = DMAU_INT_STATUS_REG; addr <= DMAU_SYNC_REG; addr += 4)
		printk(KERN_WARNING "%x: %x\n", (unsigned int)addr,
		       __raw_readl(addr));
	for (addr = DMAU_Cx_SRC_ADDR(channel);
	     addr <= DMAU_Cx_CONF_ADDR(channel); addr += 4)
		printk(KERN_WARNING "%x: %x\n", (unsigned int)addr,
		       __raw_readl(addr));

}

void printspiregs(void)
{
	unsigned long addr;
	printk(KERN_WARNING "SPI REGS:\n");
	for (addr = SPI1_GLOBAL_REG; addr <= SPI1_STAT_REG; addr += 4)
		printk(KERN_WARNING "SPI A: %x: %x\n", (unsigned int)addr,
		       __raw_readl(addr));
/*	for( addr = SPI1_DAT_REG; addr <= SPI1_DAT_MASK_REG; addr+=4 )
		printk( KERN_WARNING "SPI B: %x: %x\n",
		(unsigned int)addr, __raw_readl(addr) );*/
	for (addr = SPI1_MASK_REG; addr <= SPI1_ADDR_REG; addr += 4)
		printk(KERN_WARNING "SPI C: %x: %x\n", (unsigned int)addr,
		       __raw_readl(addr));
	for (addr = SPI1_TIMER_CTRL_REG; addr <= SPI1_TIMER_STATUS_REG;
	     addr += 4)
		printk(KERN_WARNING "SPI D: %x: %x\n", (unsigned int)addr,
		       __raw_readl(addr));

}

/** @brief Initiate DMA transfer
 *
 * @param pnx_spi Pointer to spi data
 * @param tx_dma Tx buffer (dma mapped)
 * @param rx_dma Rx buffer (dma mapped)
 * @param len How many bytes to transfer
 */
static int pnx67xx_do_dma_transfer(struct pnx67xx_spi *pnx_spi,
				   dma_addr_t tx_dma, dma_addr_t rx_dma,
				   u32 len)
{
	int ret = 0, buffer;
	int frm = (pnx_spi->bits_per_word == 8 ? len : len / 2);
	/* SPI interrupt must be disabled now */
	/* check spi fifo is empty */
	pnx_spi->doing_dma_transfer = 1;
#ifdef SPI_DEBUG
	if (!(spi_read_stat(pnx_spi) & SPI_STAT_SPI_BE))
		printk(KERN_ERR "SPI FIFO not empty\n");

	/* ************************** */
	if (frm <= SPI_FRM_MAX)
		spi_write_frame(pnx_spi, frm);
	else {
		printk(KERN_ERR "Too Big size\n");
		return 0;
	}
#else
	spi_write_frame(pnx_spi, frm);
#endif
	spi_disable(pnx_spi);
	/* If Tx or Rx is not specified, it points to dma_buffer */
	if ((tx_dma != INVALID_DMA_ADDRESS)) {
		pnx_spi->txmode = SPI_TRANSMIT;
		buffer = (int)tx_dma;
		set_tx_stop_clk(pnx_spi);
		pnx67xx_spi_tx_dma(pnx_spi, tx_dma, len);
		ret += pnx67xx_spi_dma_ch_enable(pnx_spi);
		spi_enable(pnx_spi);
		enable_irq(pnx_spi->irq);

	} else if ((rx_dma != INVALID_DMA_ADDRESS)) {
		u16 temp;
		pnx_spi->txmode = SPI_RECEIVE;
		buffer = (int)rx_dma;
		set_rx_stop_clk(pnx_spi);
		pnx67xx_spi_rx_dma(pnx_spi, rx_dma, len);
		ret += pnx67xx_spi_dma_ch_enable(pnx_spi);
		spi_enable(pnx_spi);
		temp = spi_read_data(pnx_spi);
	} else {
		printk(KERN_ERR "Unknown mode\n");
		return -1;
	}
	return 0;
}

static int pnx67xx_do_poll_transfer(struct pnx67xx_spi *pnx_spi, u16 *tx_buf,
				    u16 *rx_buf, u32 len)
{
	u16 stat;
	int loop = 0;
	int frm = (pnx_spi->bits_per_word == 8 ? len : len / 2);
	pnx_spi->doing_dma_transfer = 0;
	BUG_ON(pnx_spi->current_transfer == NULL);
	spi_write_frame(pnx_spi, frm);
#ifdef SPI_DEBUG
	if (!(spi_read_stat(pnx_spi) & SPI_STAT_SPI_BE))
		printk(KERN_ERR "SPI FIFO not empty\n");
#endif
	if (tx_buf) {
		pnx_spi->txmode = SPI_TRANSMIT;
		set_tx_stop_clk(pnx_spi);
		spi_write_buf(pnx_spi, tx_buf, len);
	} else {
		u16 temp;
		pnx_spi->txmode = SPI_RECEIVE;
		set_rx_stop_clk(pnx_spi);
		temp = spi_read_data(pnx_spi);
	}
	if (len > *(pnx_spi->pollsize))
		goto hw_transfer;
	while (!(spi_read_stat(pnx_spi) & SPI_STAT_SPI_EOT)) {
		loop++;
		/* 1us at 416 Mhz = 416 instruction */
		if (loop > (20 * 416)) {
#ifdef SPI_DEBUG
			printk(KERN_ERR "SPI poll timeout \n");
#endif
			if (pnx_spi->current_transfer == NULL)
				return 0;
			else
				goto hw_transfer;
		}
	}
	if (rx_buf)
		spi_read_buf(pnx_spi, rx_buf, len);

	stat = spi_read_stat(pnx_spi);
#ifdef SPI_DEBUG
	if (!stat & SPI_STAT_SPI_BE)
		printk(KERN_ERR "SPI FIFO not empty\n");
#endif
	/* clear stat bit */
	spi_write_stat(pnx_spi,
		       (stat | SPI_STAT_SPI_INTCLR) & ~SPI_STAT_SPI_EOT);
	return 1;
hw_transfer:
	enable_irq(pnx_spi->irq);
	return 0;
}

/** @brief Submit dma transfer
 *
 * @param master Spi master data
 * @param msg Message to transfer
 */
static int pnx67xx_spi_next_xfer(struct pnx67xx_spi *pnx_spi,
				 struct spi_message *msg)
{
	struct spi_transfer *xfer;
	u32 len;
	dma_addr_t tx_dma, rx_dma;
	int ret = 0;
	xfer = pnx_spi->current_transfer;
	if (xfer)
		xfer = list_entry(xfer->transfer_list.next,
				  struct spi_transfer, transfer_list);
	else
		xfer = list_entry(msg->transfers.next,
				  struct spi_transfer, transfer_list);
	pnx_spi->current_transfer = xfer;
	tx_dma = xfer->tx_dma;
	rx_dma = xfer->rx_dma;
	len = xfer->len;

	/* FIX ME */
	/* this is linked to pnx_spi it can work only if
	 * devices have the same bits_per_word */
	if (pnx_spi->bits_per_word > 8) {
		/* instead of % 2 */
		if (len & 1)
			len = (len >> 1) + 1;
		else
			len >>= 1;

	}
	if (len < DMA_LEN_THRESHOLD)
		ret =
		    pnx67xx_do_poll_transfer(pnx_spi, (u16 *) xfer->tx_buf,
					     (u16 *) xfer->rx_buf, len);
	else
		ret = pnx67xx_do_dma_transfer(pnx_spi, tx_dma, rx_dma, len);

	return ret;
}

/** @brief Transmit is done
 *
 * This is run after both SPI and DMA have completed
 * @param data Not used
 */
static void pnx67xx_spi_transfer_done(struct pnx67xx_spi *pnx_spi)
{
	struct spi_message *msg;
	struct spi_transfer *xfer;
done:
	xfer = pnx_spi->current_transfer;
	msg = list_entry(pnx_spi->queue.next, struct spi_message, queue);
#ifdef SPI_DEBUG
	if (msg == NULL) {
		dev_err(&pnx_spi->pdev->dev, "NULL message\n");
		return;
	}
	if (xfer == NULL) {
		dev_err(&pnx_spi->pdev->dev, "NULL transfer\n");
		if (pnx_spi->doing_dma_transfer)
			pnx67xx_spi_dma_ch_disable(pnx_spi);
		pnx_spi->doing_dma_transfer = 0;
		return;
	}
#endif
	/* stopped dma actvity in any cases */
	if (pnx_spi->doing_dma_transfer)
		pnx67xx_spi_dma_ch_disable(pnx_spi);
	pnx_spi->doing_dma_transfer = 0;
	msg->actual_length += xfer->len;
	spi_stop_generating_clock(pnx_spi);

	/* Unmap dma mapped transfers */
	if (!msg->is_dma_mapped)
		pnx67xx_spi_dma_unmap_xfer(pnx_spi, xfer);

	/* FIXME: udelay in irq is unfriendly */
#if 0
	if (xfer->delay_usecs)
		udelay(xfer->delay_usecs);
#endif
	/* If all the transfers in the message have been sent */
	if (msg->transfers.prev == &xfer->transfer_list) {
		/* report completed message */
		if (pnx67xx_spi_msg_done(pnx_spi, msg, 0, !xfer->cs_change))
			goto done;
	} else {
		if (xfer->cs_change) {
			cs_deactivate(msg->spi);
			udelay(1);
			cs_activate(msg->spi);
		}
		if (pnx67xx_spi_next_xfer(pnx_spi, msg))
			goto done;
	}
	return;
}

static void pnx67xx_spi_work_transfer_done(struct work_struct *work)
{
	struct pnx67xx_spi *pnx_spi =
	    container_of(work, struct pnx67xx_spi, xferdone);
#ifdef IRQ_PROCESS
	pnx_spi->pollsize = &pnx_spi->pollsize_thread;
#endif
	pnx67xx_spi_transfer_done(pnx_spi);
}

/** @brief Interrupt after SPI transfer is complete
 *
 * @param irq IRQ number
 * @param dev_id Internal data
 */
static irqreturn_t pnx67xx_spi_interrupt(int irq, void *dev_id)
{
	struct spi_master *master = dev_id;
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);
	u16 stat;

	/* Read interrupt source */
	stat = spi_read_stat(pnx_spi);
#ifdef SPI_DEBUG
	if (!stat & SPI_STAT_SPI_BE)
		printk(KERN_ERR "SPI FIFO not empty\n");
#endif
	/* Acknowledge interrupt */
	spi_write_stat(pnx_spi,
		       (stat | SPI_STAT_SPI_INTCLR) & ~SPI_STAT_SPI_EOT);
	/* If we are on HW read, */
#if 1
	if ((!pnx_spi->doing_dma_transfer) && (pnx_spi->txmode == SPI_RECEIVE))
		spi_read_buf(pnx_spi, pnx_spi->current_transfer->rx_buf,
			     pnx_spi->current_transfer->len);
	disable_irq(pnx_spi->irq);
#else
	if (!pnx_spi->doing_dma_transfer)
		printk(KERN_ERR "SPI interrupt useless\n");
	else
		disable_irq(pnx_spi->irq);
#endif
#ifdef IRQ_PROCESS
	pnx_spi->pollsize = &pnx_spi->pollsize_it;
	pnx67xx_spi_transfer_done(pnx_spi);
#else
	queue_work(pnx_spi->workqueue, &pnx_spi->xferdone);
#endif
	return IRQ_HANDLED;
}

/** @brief Map SPI transfer to DMA capable memory
 *
 * @param pnx_spi Pointer to SPI data
 * @param xfer Transfer to map
 */
static int pnx67xx_spi_dma_map_xfer(struct pnx67xx_spi *pnx_spi,
				    struct spi_transfer *xfer)
{
	struct device *dev = &pnx_spi->pdev->dev;
	u8 factor = 0;

	factor = (pnx_spi->bits_per_word == 16 ? 2 : 1);

	if (xfer->len < (DMA_LEN_THRESHOLD * factor))
		return 0;

	xfer->tx_dma = xfer->rx_dma = INVALID_DMA_ADDRESS;
	if (xfer->tx_buf) {
		void *tx = (void *)xfer->tx_buf;
		xfer->tx_dma = dma_map_single(dev,
					      (void *)tx, xfer->len,
					      DMA_TO_DEVICE);
		if (dma_mapping_error(dev, xfer->tx_dma)) {
			printk(KERN_ERR "SPI TX DMA map error\n");
			return -ENOMEM;
		}
	} else if (xfer->rx_buf) {
		xfer->rx_dma = dma_map_single(dev,
					      (void *)xfer->rx_buf, xfer->len,
					      DMA_FROM_DEVICE);
		if (dma_mapping_error(dev, xfer->rx_dma)) {
			printk(KERN_ERR "SPI RX DMA map error\n");
			return -ENOMEM;
		}
	}
	return 0;
}

#ifdef SPI_DEBUG
/** @brief Split transfer to largest possible size
 *
 * FIXME: USE LLI TO SPEED THINGS UP!
 *
 * @param spi Spi device
 * @param xfer Transfer to inspect
 * @return 0 = success
 */
static int pnx67xx_split_too_large_transfers(struct pnx67xx_spi *pnx_spi,
					     struct spi_transfer *xfer)
{
	if (xfer->len > DMA_TR_SIZE_MAX) {
		/* LLI implementation */
		return -EINVAL;
	}
	if (pnx_spi->bits_per_word == 8) {
		/* no control */
		return 0;
	} else if (pnx_spi->bits_per_word == 16) {
		if ((int)(xfer->rx_buf) & 1)
			return -EINVAL;
		if ((int)(xfer->tx_buf) & 1)
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}
#endif
/** @brief Called by kernel when a transfer is wanted
 *
 * @param spi Spi device
 * @param msg Message to tranmit
 * @return 0 = success
 */
static int pnx67xx_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct pnx67xx_spi *pnx_spi;
	struct spi_transfer *xfer;
#ifdef SPI_DEBUG
	int ret = 0;
#endif
	unsigned long flags;
	pnx_spi = spi_master_get_devdata(spi->master);
	if (unlikely(list_empty(&msg->transfers)
		     || !spi->max_speed_hz))
		return -EINVAL;

	if (pnx_spi->stopping)
		return -ESHUTDOWN;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!(xfer->tx_buf || xfer->rx_buf)) {
			dev_err(&spi->dev, "missing rx or tx buf\n");
			return -EINVAL;
		}

		/* FIXME implement these protocol options!! */
		if (xfer->bits_per_word || xfer->speed_hz) {
			dev_dbg(&spi->dev, "no protocol options yet\n");
			return -ENOPROTOOPT;
		}
		/* check only one transfer */
		if ((xfer->rx_buf) && (xfer->tx_buf)) {
			dev_err(&spi->dev, "rx and  tx buf not supported\n");
			return -EINVAL;
		}
#ifdef SPI_DEBUG
		/* Split transfers to max size according to
		 *  rx /tx and 8 or 16 bitwidth
		 *  and check alignement according bits_per_word */
		ret = pnx67xx_split_too_large_transfers(pnx_spi, xfer);
		if (ret != 0) {
			dev_err(&spi->dev,
				"transaction too large not supported\n");
			return ret;
		}
#endif
		/*
		 * DMA map early,
		 * this is done only if transaction
		 * size is matching minimum DMA transaction
		 * This implementation supports only contiguous memory mapping
		 */
		if (!msg->is_dma_mapped) {
			if (pnx67xx_spi_dma_map_xfer(pnx_spi, xfer) < 0)
				return -ENOMEM;
		}
	}
	/* protect the pnx_spi->queue against multiple modification */
	spin_lock_irqsave(&pnx_spi->lock, flags);
	list_add_tail(&msg->queue, &pnx_spi->queue);

	if (pnx_spi->transfer_state == NO_TRANSFER) {
		pnx_spi->transfer_state = TRANSFERING;
#ifdef IRQ_PROCESS
		pnx_spi->pollsize = &pnx_spi->pollsize_thread;
#endif
		pnx67xx_spi_pm_qos_up(pnx_spi);
		clk_enable(pnx_spi->clk);
		spin_unlock_irqrestore(&pnx_spi->lock, flags);
		if (pnx67xx_spi_next_message(pnx_spi))
			pnx67xx_spi_transfer_done(pnx_spi);
	} else
		spin_unlock_irqrestore(&pnx_spi->lock, flags);
	return 0;
}

/** @brief Check setup parameters
 *
 * @param spi Spi device whose parameters to check
 * @return 0 = success
 */
static int pnx67xx_spi_check_setup_params(struct spi_device *spi)
{
	struct pnx67xx_spi *pnx_spi;
	pnx_spi = spi_master_get_devdata(spi->master);

	if (pnx_spi->stopping)
		return -ESHUTDOWN;

	/* Check chip select is ok */
	if ((spi->chip_select != NO_CHIP_SELECT)
	    && (spi->chip_select > spi->master->num_chipselect)) {
		dev_err(&spi->dev,
			"setup: invalid chipselect %u (%u defined)\n",
			spi->chip_select, spi->master->num_chipselect);
		return -EINVAL;
	}

	if (spi->bits_per_word == 0)
		spi->bits_per_word = 8;
	if (spi->bits_per_word < 8 || spi->bits_per_word > 16) {
		dev_err(&spi->dev,
			"setup: invalid bits_per_word %u (8 to 16)\n",
			spi->bits_per_word);
		return -EINVAL;
	}

	if (spi->mode & ~MODEBITS) {
		dev_err(&spi->dev, "setup: unsupported mode bits %x\n",
			spi->mode & ~MODEBITS);
		return -EINVAL;
	}

	return 0;
}

/** @brief Setup spi device
 *
 * This function is called by kernel when spi needs to change mode or clock
 *
 * @param pdev Platform device
 * @return 0=success
 */
static int pnx67xx_spi_setup(struct spi_device *spi)
{
	int ret;
	struct pnx67xx_spi_controller_state *cs;
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(spi->master);
	ret = pnx67xx_spi_check_setup_params(spi);
	if (ret != 0)
		return ret;

	/* If nobody has done setup yet, set up gpio */
	if (!spi->controller_state) {
		cs = kzalloc(sizeof(struct pnx67xx_spi_controller_state),
			     GFP_KERNEL);
		if (!cs)
			return -ENOMEM;
		spi->controller_state = (u32 *) cs;
		if (pnx67xx_spi_setup_gpio
		    (spi->chip_select, !(spi->mode & SPI_CS_HIGH),
		     pnx_spi->pdev->name) != 0) {
			kfree(cs);
			return -ENOMEM;
		}
	} else
		cs = spi->controller_state;
	/* right the same initial value  */
	cs->control_val = SPI_CON_MS | SPI_CON_SPI_MODE0 | SPI_CON_RATE_13 |
	    SPI_CON_THR | SPI_CON_SPI_BIDIR;
	cs->control_val |= (SPI_INITIAL_BITWIDTH - 1) << SPI_CON_BITNUM_SHIFT;
	pnx_spi->bits_per_word = spi->bits_per_word;
	pnx67xx_spi_set_clock(cs, spi->max_speed_hz);
	pnx67xx_spi_set_mode(cs, spi->mode);
	pnx67xx_spi_set_bitwidth(cs, spi->bits_per_word);
	/* compute dma configuration */
	pnx67xx_build_dma_rx(pnx_spi);
	pnx67xx_build_dma_tx(pnx_spi);

	return 0;
}

/** @brief Cleanup spi device
 *
 * This function is called by kernel when spi is about to be removed
 *
 * @param pdev Platform device
 * @return 0=success
 */
static void pnx67xx_spi_cleanup(struct spi_device *spi)
{
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(spi->master);
	unsigned gpio = (unsigned)spi->chip_select;
	unsigned long flags;

	if (!spi->controller_state)
		return;

	kfree(spi->controller_state);

	spin_lock_irqsave(&pnx_spi->lock, flags);
	if (pnx_spi->stay == spi) {
		pnx_spi->stay = NULL;
		cs_deactivate(spi);
	}
	spin_unlock_irqrestore(&pnx_spi->lock, flags);

	pnx67xx_spi_release_gpio(gpio);
}

/** @brief Get platform resources
 *
 * @param pdev Platform device
 * @param spi PNX67xx spi data
 * @return 0=success
 */
static int __init pnx67xx_spi_get_resources(struct platform_device *pdev,
					    struct pnx67xx_spi *spi)
{
	struct resource *regs;
	spi->regs = NULL;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs)
		return -ENXIO;

	spi->phys_addr = regs->start;
	spi->regs = ioremap(regs->start, (regs->end - regs->start) + 1);
	if (!spi->regs)
		return -ENOMEM;

	spi->irq = platform_get_irq(pdev, 0);
	if (spi->irq < 0)
		return spi->irq;

	spi->clk = clk_get(&pdev->dev, PNX_SPI_CLOCK(pdev->id));
	if (IS_ERR(spi->clk))
		return PTR_ERR(spi->clk);

	dev_info(&pdev->dev, "PNX67xx SPI Controller at 0x%08lx (irq %d)\n",
		 (unsigned long)regs->start, spi->irq);

	return 0;
}

/** @brief Set up spi_master structure and set it to driver data
 * @param pdev Platform device
 * @param master spi_master structure to set up
 */
static void __init pnx67xx_spi_setup_master(struct platform_device *pdev,
					    struct spi_master *master)
{
	master->bus_num = pdev->id;
	master->num_chipselect = PNX_SPI_CHIP_SELECTS;
	master->setup = pnx67xx_spi_setup;
	master->transfer = pnx67xx_spi_transfer;
	master->cleanup = pnx67xx_spi_cleanup;
	platform_set_drvdata(pdev, master);
}

/** @brief Probe platform device
 *
 * This function is called by kernel when platform device is probed
 * ( Once for every bus listed in board-xxx.c )
 *
 * @param pdev Platform device
 * @return 0=success
 */
static int pnx67xx_spi_probe(struct platform_device *pdev)
{
	int ret;
	struct spi_master *master;
	struct pnx67xx_spi *spi;
	dev_info(&pdev->dev, "Probing for PNX67xx SPI Controller\n");

	/* Allocate SPI master */
	master = spi_alloc_master(&pdev->dev, sizeof *spi);
	if (!master)
		return -ENOMEM;

	/* Set up data for master */
	pnx67xx_spi_setup_master(pdev, master);
	spi = spi_master_get_devdata(master);
	spi->pdev = pdev;
	spi->master = master;
	spi->channel_enable_counter = 0;

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "SPI Platform data not initialized\n");
		return -EFAULT;
	} else
		spi->pdata = pdev->dev.platform_data;
	/* create the workqueue with the same name */
	/* spi->workqueue= create_workqueue(pdev->name); */
	spi->workqueue = __create_workqueue(pdev->name, 1, 0, 1);
	INIT_WORK(&spi->xferdone, pnx67xx_spi_work_transfer_done);

	/* Read platform resources */
	ret = pnx67xx_spi_get_resources(pdev, spi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Invalid platform resources (%d)\n", ret);
		goto out_free_master;
	}
	spi->transfer_state = NO_TRANSFER;
	spi->current_transfer = NULL;
	/* the only purpose of this spin lock is
	 * to share access to  msg queue */
	spin_lock_init(&spi->lock);
	spi->lock_count = 0;
	INIT_LIST_HEAD(&spi->queue);

	ret = request_irq(spi->irq, pnx67xx_spi_interrupt, 0,
			  pdev->dev.bus_id, master);
	if (ret) {
		dev_err(&pdev->dev, "Request IRQ failed\n");
		ret = -ENOMEM;
		goto out_free_master;
	}
	disable_irq(spi->irq);

	init_timer(&(spi->pm_qos_timer));
	spi->pm_qos_timer.function = &pnx67xx_spi_pm_qos_timeout;
	spi->pm_qos_timer.data = (unsigned long)spi;

	pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *)pdev->name,
			       104);

	/* Initialize the hardware */
	clk_enable(spi->clk);

	if (pnx67xx_spi_mux_init(spi, pdev->name)) {
		pnx67xx_spi_mux_free(spi);
		goto out_reset_hw;
	}

	pnx67xx_spi_hw_init(spi);

	/* Get DMA channel */
	if (pnx67xx_spi_request_dma_channel(master) != 0) {
		dev_err(&pdev->dev, "DMA channel request failed\n");
		ret = -ENOMEM;
		goto out_reset_hw;
	}

	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "Register master failed\n");
		goto out_free_dma;
	}
	dev_info(&pdev->dev, "PNX67xx SPI master installed\n");

	/* disable SPI clock and set SPI pins in
	 * GPIO mode in order to save power */
	clk_disable(spi->clk);
	pnx67xx_spi_mux_power_config(spi, 1);
	pnx67xx_spi_pm_qos_down(spi);

	return 0;

out_free_dma:
	pnx67xx_spi_free_dma_channel(spi);

out_reset_hw:
	pnx67xx_spi_hw_reset(spi, 0);
	if (!(IS_ERR(spi->clk)))
		clk_disable(spi->clk);
	free_irq(spi->irq, master);
out_free_master:
	if (!(IS_ERR(spi->clk)))
		clk_put(spi->clk);
	if (spi->regs)
		iounmap(spi->regs);
	spi_master_put(master);
	return ret;
}

static int __exit pnx67xx_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);
	struct spi_message *msg;

	/* reset the hardware and block queue progress */
	spin_lock(&pnx_spi->lock);
	pnx_spi->stopping = 1;
	pnx67xx_spi_hw_reset(pnx_spi, 0);
	spin_unlock_irq(&pnx_spi->lock);

	/* Terminate remaining queued transfers */
	list_for_each_entry(msg, &pnx_spi->queue, queue) {
		/* REVISIT unmapping the dma is a NOP on ARM and AVR32
		 * but we shouldn't depend on that...
		 */
		msg->status = -ESHUTDOWN;
		msg->complete(msg->context);
	}
	/* disable SPI clock and set SPI pins in GPIO
	 * mode in order to save power */
	clk_disable(pnx_spi->clk);
	pnx67xx_spi_mux_power_config(pnx_spi, 1);
	pnx67xx_spi_pm_qos_down(pnx_spi);

	pm_qos_remove_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *)pdev->name);
	del_timer(&(pnx_spi->pm_qos_timer));

	clk_put(pnx_spi->clk);
	free_irq(pnx_spi->irq, master);
	iounmap(pnx_spi->regs);

	pnx67xx_spi_free_dma_channel(pnx_spi);

	spi_unregister_master(master);

	return 0;
}

#ifdef	CONFIG_PM

static int pnx67xx_spi_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);

//	clk_disable(pnx_spi->clk);
	return 0;
}

static int pnx67xx_spi_resume(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct pnx67xx_spi *pnx_spi = spi_master_get_devdata(master);

//	clk_enable(pnx_spi->clk);
	return 0;
}

#else
#define	pnx67xx_spi_suspend	NULL
#define	pnx67xx_spi_resume	NULL
#endif

static struct platform_driver pnx67xx_spi_driver1 = {
	.driver = {
		   .name = "pnx67xx_spi1",
		   .owner = THIS_MODULE,
		   },
	.probe = pnx67xx_spi_probe,
	.remove = __exit_p(pnx67xx_spi_remove),

	.suspend = pnx67xx_spi_suspend,
	.resume = pnx67xx_spi_resume,
};

static struct platform_driver pnx67xx_spi_driver2 = {
	.driver = {
		   .name = "pnx67xx_spi2",
		   .owner = THIS_MODULE,
		   },
	.probe = pnx67xx_spi_probe,
	.remove = __exit_p(pnx67xx_spi_remove),

	.suspend = pnx67xx_spi_suspend,
	.resume = pnx67xx_spi_resume,
};

static int __init pnx67xx_spi_init(void)
{
	int ret;
	ret = platform_driver_register(&pnx67xx_spi_driver1);
	if (ret)
		return ret;
	ret = platform_driver_register(&pnx67xx_spi_driver2);
	if (ret)
		return ret;
	return 0;
}

module_init(pnx67xx_spi_init);

static void __exit pnx67xx_spi_exit(void)
{
	platform_driver_unregister(&pnx67xx_spi_driver1);
	platform_driver_unregister(&pnx67xx_spi_driver2);

}

module_exit(pnx67xx_spi_exit);

MODULE_DESCRIPTION("PNX67xx SPI controller driver");
MODULE_AUTHOR("Sami Nurmenniemi <ext-sami.nurmenniemi@elektrobit.com>");
MODULE_LICENSE("GPL");
