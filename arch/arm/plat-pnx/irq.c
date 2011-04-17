/*
 *  linux/arch/arm/plat-pnx/irq.c
 *
 *  Copyright (C) 2007 ST-Ericsson
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/irqs.h>		/* PNX specific constants, types */

#undef PNX_IRQ_MONITOR

#define INTC_REQUESTx_PENDING_MASK       ( 1 << 31 )
#define INTC_REQUESTx_SET_SWINT_MASK     ( 1 << 30 )
#define INTC_REQUESTx_CLR_SWINT_MASK     ( 1 << 29 )
#define INTC_REQUESTx_WE_PRIORITY_MASK   ( 1 << 28 )
#define INTC_REQUESTx_WE_TARGET_MASK     ( 1 << 27 )
#define INTC_REQUESTx_WE_ENABLE_MASK     ( 1 << 26 )
#define INTC_REQUESTx_WE_ACTIVE_LOW_MASK ( 1 << 25 )
#define INTC_REQUESTx_ACTIVE_LOW_MASK    ( 1 << 17 )
#define INTC_REQUESTx_ENABLE_MASK        ( 1 << 16 )
#define INTC_REQUESTx_TARGET_MASK        ( 1 <<  8 )

#define INTC_IID_USB_LP_INT 13

static void pnx_irq_mask(uint32_t irqno)
    {
	if (irqno > 0 || irqno < IRQ_COUNT) {
	unsigned long flags;
	uint32_t a, v;

	hw_raw_local_irq_save ( flags );

	a = INTC_REQUESTx ( irqno );
	v = readl ( a );
	v &= ~INTC_REQUESTx_ENABLE_MASK;
	v |= INTC_REQUESTx_WE_ENABLE_MASK;
	v |= INTC_REQUESTx_CLR_SWINT_MASK;
	writel ( v, a );

	hw_raw_local_irq_restore ( flags );
    }
}

static void pnx_irq_unmask(uint32_t irqno)
    {
	if (irqno > 0 || irqno < IRQ_COUNT) {
	unsigned long flags;
	uint32_t a, v;

	hw_raw_local_irq_save ( flags );

	a = INTC_REQUESTx(irqno);
	v = readl ( a );
	v |= INTC_REQUESTx_ENABLE_MASK;
	v |= INTC_REQUESTx_WE_ENABLE_MASK;
	v |= INTC_REQUESTx_CLR_SWINT_MASK;
	writel ( v, a );

	hw_raw_local_irq_restore ( flags );
    }
}

static int pnx_irq_type(unsigned irqno, unsigned type)
{
	int  l;
	uint32_t reg = INTC_REQUESTx ( irqno );

	if (irqno > 0 || irqno < IRQ_COUNT) {
		if (type == IRQF_TRIGGER_LOW) {
			l = (INTC_REQUESTx_ACTIVE_LOW_MASK
			     | INTC_REQUESTx_WE_ACTIVE_LOW_MASK);
		} else if (type == IRQF_TRIGGER_HIGH) {
			l = (INTC_REQUESTx_WE_ACTIVE_LOW_MASK);
		} else {
			goto bad;
		}

		__raw_writel(l, reg);
		return 0;

	}

bad:
	return -EINVAL;
}

int pnx_irq_enabled(uint32_t irqno)
    {
	if (irqno > 0 || irqno < IRQ_COUNT) {
	uint32_t a = INTC_REQUESTx ( irqno );
	uint32_t v = readl ( a );

	return v & INTC_REQUESTx_ENABLE_MASK;
	} else
	return 0;
}

#ifdef CONFIG_NKERNEL
static void pnx_irq_ack(uint32_t irqno)
{
}
#endif

static struct irq_chip pnx_irq_chip = {
#ifdef CONFIG_NKERNEL
    .ack    = pnx_irq_ack,
#else
    .ack    = pnx_irq_mask,
#endif
    .mask   = pnx_irq_mask,              /* disable irqs */
    .unmask = pnx_irq_unmask,            /* enable irqs */
	.set_type	= pnx_irq_type,
};

#ifdef CONFIG_NKERNEL

static void pnx_irq_init(uint32_t irqno)
    {
	if (irqno > 0 && irqno < IRQ_COUNT) {
	unsigned long flags;

	hw_raw_local_irq_save ( flags );
/* LPA do nothing because already done by osware with connect operation */
#if 0
      uint32_t v, a;
	v = 15; /* highest priority */
	v |= INTC_REQUESTx_WE_ACTIVE_LOW_MASK;
	v |= INTC_REQUESTx_WE_TARGET_MASK;
	v |= INTC_REQUESTx_WE_PRIORITY_MASK;
	v |= INTC_REQUESTx_WE_ENABLE_MASK;
	v |= INTC_REQUESTx_CLR_SWINT_MASK;

	a = INTC_REQUESTx ( irqno );
	writel ( v, a );
#endif

	set_irq_chip    ( irqno, &pnx_irq_chip );
	set_irq_handler ( irqno, handle_level_irq );
	set_irq_flags   ( irqno, IRQF_VALID | IRQF_PROBE );

	hw_raw_local_irq_restore ( flags );
    }
	/* Unset auto enable flag for USB LP IRQ */
    set_irq_flags(INTC_IID_USB_LP_INT, IRQF_VALID | IRQF_NOAUTOEN);

}

#include <nk/nkern.h>
#include <asm/atomic.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>

int ___ffs(u32 mask)
{
    u32 bit;
    mask = mask & -mask;
    asm("clz   %0, %1": "=r" (bit): "r" (mask));
    return bit - 31;
#ifdef OLD
asm("rsb	%0, %1, #0 ;" "and	%0, %1, %0 ;"
	"clz	%0, %0; " "rsb	%0, %0, #31" : "=&r"(bit) : "r"(mask));
    return bit;
#endif
}

#define OLDFFS(mask) ({ int r = mask & ~mask; \
		asm("clz   %0, %1" : "=r" (r) : "r" (r)); \
		31 - r ; })

#define FFS(A) __ffs(A)

/* ----------------------- start of sys bus driver --------------------- */

#define MAX_OS			4
#define MAX_DEVICE		80

#define DEVICE_STATE_UNASSIGNED	0
#define DEVICE_STATE_ASSIGNED	1 /* assignement is not taken into account */
#define DEVICE_STATE_INACTIVE	2		/* assigned to an os and ready */
#define DEVICE_STATE_ACTIVE	3		/* assigned and active */
struct SysBusDevice {
    uint32_t	addr;
    uint16_t	size;
    uint8_t	state;
    uint8_t	owner;
    uint8_t	next;
    uint8_t	irqs[3];
};

typedef void (*NkSysBusEventHdl) (int dev, struct SysBusDevice *sys);

struct NkPNXSysBus {

    /*!
     * Generic device structure
     */
    NkDevDesc		dev;

    NkSysBusEventHdl	event_hdl[MAX_OS];
    nku8_f		xirqs[MAX_OS];
    nku32_f		subscribed_events[MAX_OS][(MAX_DEVICE + 31) / 32 ];
    nku32_f		pending_events[MAX_OS]   [(MAX_DEVICE + 31) / 32 ];

    /*! list of device provided to linux  */
	struct SysBusDevice devices[MAX_DEVICE];

};

static struct NkPNXSysBus *bus;
static int		los;

#ifdef __KERNEL__
#define PRINT	printnk
#else
#define PRINT	printnk
#endif

#define warn	PRINT

#ifdef DBG_PNX_SYS_BUS
static int	sysbus_tracecontrol = 0xf;
#define _PRINT(LEVEL, ...) \
	if ((1 << (LEVEL)) & sysbus_tracecontrol) \
		PRINT(__VA_ARGS__)

#define trace(LEVEL, ...)   \
	_PRINT(LEVEL, "vuart" __VA_ARGS__)

#define prolog(A, ...)						\
	_PRINT(1, "%d>%s" A, los, __func__,  __VA_ARGS__)

#define epilog(A, ...)						\
	_PRINT(1, "%d<%s" A, los, __func__,  __VA_ARGS__)

#define xintrprolog(A, ...)						\
	_PRINT(3, "%d>%s" A, los, __func__,  __VA_ARGS__)

#define xintrepilog(A, ...)						\
	_PRINT(3, "%d<%s" A, los, __func__,  __VA_ARGS__)
#else
#define prolog(...)
#define epilog(...)
#define xintrprolog(...)
#define xintrepilog(...)
#define trace(...)
#define trace(...)
#endif

/* cross interrupt handler */
void event_hdl(void *_bus, NkXIrq irq)
{
    int		i;
	struct NkPNXSysBus *bus = (struct NkPNXSysBus *) _bus;

    prolog("(%p)\n", _bus);
    /* notify all unmasked device */
    for (i = 0 ; i < ( (MAX_DEVICE + 31) / 32 ) ; i++) {
	nku32_f mask;

	nku32_f*	subscribed =  &bus->subscribed_events[los][i];
	nku32_f*	pending =  &bus->pending_events[los][i];

	mask = *subscribed & *pending;

	while ( mask ) {
	    int		j;
	    j = FFS(mask);

	    /* mask event for the duration of the call */
	    *subscribed &= ~( 1 << j );

	    /* remove it from pending event */
	    *pending &= ~(1 << j);

	    /* invoke the handler */
			bus->event_hdl[los] (j + i * 32,
					     &bus->devices[j + i * 32]);

	    /* renable the event */
	    *subscribed |= ( 1 << j );

	    /* loop over the device */
	    mask = (*pending) & (*subscribed);
	}
    }
}

static void
sysbus_device_set_state(struct NkPNXSysBus *bus, nku32_f device, nku8_f state)
{
    int		ros = ( los == 1 ? 2 : 1 ) ;
    nku32_f	mask = 1 << (device & 0x1f);
    nku32_f	idx = device >> 5;

    nku32_f*	subscribed =  &bus->subscribed_events[ros][idx];
    nku32_f*	pending =  &bus->pending_events[ros][idx];

    bus->devices[device].state = state;

    *pending |= mask;
	if (*subscribed & mask)
		if (bus->xirqs[ros])
	    nkops.nk_xirq_trigger(bus->xirqs[ros], ros);

    subscribed =  &bus->subscribed_events[los][idx];
    if ( *subscribed & mask ) {
	*subscribed &= ~mask;
	bus->event_hdl[los](device, &bus->devices[device]);
	*subscribed |= mask;
    } else {
	pending =  &bus->pending_events[los][idx];
        *pending   |= mask;
    }
}

void sysbus_device_monitor(int device)
{
    nku32_f	mask = 1 << (device & 0x1f);
    nku32_f	idx = device >> 5;
    nku32_f*	subscribed =  &bus->subscribed_events[los][idx];
    nku32_f*	pending =  &bus->pending_events[los][idx];

    if (*pending & mask) {
	*pending &= ~mask;
	bus->event_hdl[los](device, &bus->devices[device]);
    }
    *subscribed |=  mask;
}

struct NkPNXSysBus *sysbus_create(NkSysBusEventHdl hdl)
{
    NkPhAddr		pdev = 0;
    NkDevDesc*		vdev;
    prolog("(%p)\n", hdl);

    /*
     * lookup device and see if it already exists
     */
    pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_SYSBUS, pdev) ;
    if (pdev) {
		bus = (struct NkPNXSysBus *) nkops.nk_ptov(pdev);
	goto	attach_intr;
    }
    /*
     * no match found .
     * perform initialization of descriptor + communicaton buffer
     */
	pdev = nkops.nk_dev_alloc(sizeof(struct NkPNXSysBus));
	if (!pdev)
	goto alloc_failure;

	bus = (struct NkPNXSysBus *) nkops.nk_ptov(pdev);

	memset(bus, 0, sizeof(struct NkPNXSysBus));
    /*
     * initialize the virtual device and add it to the data base
     */
    vdev = &bus->dev;
    vdev->class_id   = NK_DEV_CLASS_GEN;
    vdev->dev_id     = NK_DEV_ID_SYSBUS;
    vdev->dev_header = pdev + sizeof(NkDevDesc);

    nkops.nk_dev_add(pdev);

attach_intr:
    /*
     * init local os id
     */
    los  = nkops.nk_id_get();
    if (bus->xirqs[los] == 0 ) {
	int		lirq;
	/*
	 * get idx
	 */
	lirq = nkops.nk_xirq_alloc(1);
		if (lirq == 0)
	    return 0;

	bus->xirqs[los] = lirq;
	bus->event_hdl[los] = hdl;

	nkops.nk_xirq_attach(lirq, event_hdl, bus);

    }

    /*
     * we should register to sysconfig xirq to reinitialize peer driver
     * if it goes down.
     */

    epilog("() %p \n", bus);
    return bus;

#ifdef LATER
rem_dev:
	/* nkops.nk_dev_rem(pdev); */
free_dev:
	/* nkops.nk_dev_free(pdev); */
#endif
alloc_failure:
    warn("sysbus creation failure\n");
    return 0;
}

void sysbus_assign_device(int device, int owner)
{
    bus->devices[device].owner = owner;
    sysbus_device_set_state(bus, device, DEVICE_STATE_ASSIGNED);
}

/* ----------------------- end of sys bus driver --------------------- */

static void	(*evthdl[MAX_DEVICE])(int, int);

static void device_event_hdl(int device, struct SysBusDevice *dev)
{
    if ( dev->owner == los && dev->state == DEVICE_STATE_ASSIGNED )
		pnx_irq_init(device);

    if (evthdl[device])
		evthdl[device]( device, dev->state);

}

void pnx_device_monitor(int device, void (*hdl)(int, int))
{
	if (device < MAX_DEVICE)
	evthdl[device] = hdl;
    }

void pnx_init_irq()
{
    int		i;
	if (sysbus_create(device_event_hdl) == 0)
	printnk ("should bug on \n");

	for (i = 0; i < MAX_DEVICE; i++)
	sysbus_device_monitor(i);

}

#else /* CONFIG_NKERNEL */

#ifdef PNX_IRQ_MONITOR
/*
 * Enable hardware debug port for debugging purposes (irq monitoring)
 *  - presume: GPIO and FPGA setup done by bootloader
 *  - HDP configuration: IRQ set 3 (MUXvalue=23):
 *                       HDP0=/IRQ;
 *                       HDP2=SCTU1-IRQ;
 *                       HDP8=UART1-IRQ;
 *
 * This function seems based on outdated value regarding PNX on Wave D.
 */
static void pnx_hdp_init(void)
{
    struct clk * clk;
    uint32_t a, v;

    /* Enable HDP.
     */
    a = SCON_SYSCON0_REG;
    v = readl ( a );
    v |=  ( 1 << 0 );
    v |= ( 23 << 1 );
    writel ( v, a );

    /* Clock the GPIO device.
     */
    clk = clk_get ( "GPIO" );
    clk_enable ( clk );
    clk_put ( clk );
}
#endif

#if defined(CONFIG_ARCH_PNX67XX)

static const uint32_t pnx_irq_config[] = {
	0,                             /* GHOST      0 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* EXTINT1    1 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* EXTINT2    2 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* EXTINT3    3 */
	0,                             /* RFRD       4 */
	0,                             /* MTU        5 */
	0,                             /* IIS        6 */
	0,                             /* USB        7 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* I2C2       8 */
	0,                             /* TVO        9 */
	0,                             /* 3G_WUP     10 */
	0,                             /* 3G_CALINT  11 */
	0,                             /* 3G_FRAME_IT 12 */
	0,                             /* GPADCINT    13 */
	0,                             /* ARM9_COMMTX 14 */
	0,                             /* ARM9_COMMRX 15 */
	0,                             /* KBS        16 */
	0,                             /* SCTU2      17 */
	0,                             /* SCTU1      18 */
	0,                             /* PIO1       19 */
	0,                             /* PIO2       20 */
	0,                             /* FINT0      21 */
	0,                             /* FINT1      22 */
	0,                             /* UART2      23 */
	0,                             /* UART1      24 */
	0,                             /* SPI2       25 */
	0,                             /* SPI1       26 */
	0,                             /* FCI        27 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* I2C1       28 */
	0,                             /* DMAU       29 */
	0,                             /* USIM       30 */
	0,                             /* RESERVED31 31 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* MSI        32 */
	0,                             /* JDI        33 */
	0,                             /* JDU        34 */
	0,                             /* NFI        35 */
	0,                             /* IPP        36 */
	0,                             /* VDC        37 */
	0,                             /* VEC        38 */
	0,                             /* VDE        39 */
	INTC_REQUESTx_ACTIVE_LOW_MASK, /* CAM        40 */
	0,                             /* ETB_ACQ    41 */
	0,                             /* ETB_FULL   42 */
	0,                             /* DIGRF_TX   43 */
	0,                             /* DIGRF_RX   44 */
	0,                             /* RESERVED45 45 */
	0,                             /* FIR_FI     46 */
	0,                             /* FIR        47 */
	0,                             /* PDCU       48 */
	0,                             /* MC2SC0     49 */
	0,                             /* MC2SC1     50 */
	0,                             /* MC2SC2     51 */
	0,                             /* MC2SC3     52 */
	0,                             /* MC2SC4     53 */
	0,                             /* MC2SC5     54 */
	0,                             /* MC2SC6     55 */
	0,                             /* MC2SC7     56 */
	0,                             /* Reserved   57 */
	0,                             /* Reserved   58 */
	0,                             /* Reserved   59 */
	0,                             /* Reserved   60 */
	0,                             /* Reserved   61 */
	0,                             /* Reserved   62 */
	0,                             /* Reserved   63 */
	0,                             /* Reserved   64 */
};

#endif

static const uint32_t pnx_irq_irq_priority;
static const uint32_t pnx_irq_fiq_priority;

void __init pnx_init_irq ( void )
{
	unsigned int irqno;

    /* Clock interrupt controller.
     */
    struct clk * clk = clk_get ( NULL, "INTC" );
    clk_enable ( clk );
    clk_put ( clk );

	/* The address of the vector table INTCdata.asm is assumed to be aligned
     * to a 2KB boundary.  Thus the register access value will be padded with
	 * zeroes, which is conforming to the 'READ_ONLY' attributes of the LS
	 * 11 bits. We are using the same table for both IRQ and FIQ.
     */
    writel ( 0, INTC_VECTOR_FIQ_REG ); /* no vector IRQ */
    writel ( 0, INTC_VECTOR_IRQ_REG ); /* no vector FIQ */

    writel ( pnx_irq_irq_priority, INTC_PRIOMASK_IRQ_REG );
    writel ( pnx_irq_fiq_priority, INTC_PRIOMASK_FIQ_REG );

    /* Initialize the individual interrupt sources.
     */
	for (irqno = 1; irqno < IRQ_COUNT; irqno += 1) {
	uint32_t a = INTC_REQUESTx ( irqno );
	uint32_t v = pnx_irq_config[irqno];

	v |= 4;
	v |= INTC_REQUESTx_WE_ACTIVE_LOW_MASK;
	v |= INTC_REQUESTx_WE_TARGET_MASK;
	v |= INTC_REQUESTx_WE_PRIORITY_MASK;
	v |= INTC_REQUESTx_WE_ENABLE_MASK;
	v |= INTC_REQUESTx_CLR_SWINT_MASK;

	writel ( v, a );

	set_irq_chip    ( irqno, &pnx_irq_chip );
	set_irq_handler ( irqno, handle_level_irq );
	set_irq_flags   ( irqno, IRQF_VALID | IRQF_PROBE );
    }

#ifdef PNX_IRQ_MONITOR
    pnx_hdp_init();
    pnx_monitor_irq_enter(0);
    pnx_monitor_irq_exit(0);
#endif
}

#define GPIO_EOI 3

void pnx_monitor_irq_enter ( unsigned int irqno )
{
	if (irqno == IRQ_SCTU1) {
	uint32_t a = GPIOA_OR_REG;
	uint32_t v = readl ( a );
	v |= 1 << GPIO_EOI;
	writel ( v, a );
    }
}

void pnx_monitor_irq_exit ( unsigned int irqno )
{
	if (irqno == IRQ_SCTU1) {
	uint32_t a = GPIOA_OR_REG;
	uint32_t v = readl ( a );
	v &= ~( 1 << GPIO_EOI );
	writel ( v, a );
    }
}

#endif /* CONFIG_NKERNEL */
