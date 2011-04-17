/*
 ****************************************************************
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)nkdev.h 1.52     06/02/28 Jaluna"
 *
 * Contributor(s):
 *   Eric Lescouet <eric@lescouet.org> - Jaluna SA
 *   Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
 *
 ****************************************************************
 */

#ifndef	_NK_NKDEV_H
#define	_NK_NKDEV_H

    /*
     * Shared device's class
     */
#define NK_DEV_CLASS_GEN	0	/* Generic (non-hardware) devices */
#define NK_DEV_CLASS_PCI	1	/* PCI bus devices */
#define NK_DEV_CLASS_PCI64	2	/* PCI-64 bus devices */
#define NK_DEV_CLASS_ISA	3	/* ISA bus devices */
#define NK_DEV_CLASS_VME	4	/* VME bus devices */
#define NK_DEV_CLASS_QUICC	5	/* QUICC bus devices */

    /*
     * Shared device's type
     */
#define NK_DEV_ID_NKIO		0x4e4b494f	/* "NKIO" - reserved by NK */
#define NK_DEV_ID_SYSBUS	0x53595342	/* "SYSB" */
#define NK_DEV_ID_QUICCBUS	0x51434342	/* "QCCB" */
#define NK_DEV_ID_PCI_BRIDGE	0x50434942	/* "PCIB" */
#define NK_DEV_ID_PIC		0x50494320	/* "PIC " */
#define NK_DEV_ID_TICK		0x5449434b	/* "TICK" */
#define NK_DEV_ID_RTC		0x50544320	/* "RTC " */
#define NK_DEV_ID_IOBUS		0x494f4220	/* "IOB " */
#define NK_DEV_ID_APIC		0x41504943	/* "APIC" */
#define NK_DEV_ID_LTICK		0x4c544943	/* "LTIC" */
#define NK_DEV_ID_RING		0x50494e47	/* "RING" */
#define NK_DEV_ID_FB		0x46422020	/* "FB  " */
#define NK_DEV_ID_SHM		0x53484d20	/* "SHM " */
#define NK_DEV_ID_WDT		0x57445420	/* "WDT " */
#define NK_DEV_ID_SIO		0x53494f20	/* "SIO " */
#define NK_DEV_ID_VGA		0x56474120	/* "VGA " */
#define NK_DEV_ID_UART		0x55415254	/* "UART" */
#define NK_DEV_ID_PERF		0x50455246	/* "PERF" */
#define NK_DEV_ID_SIB       0x53494220  /* "SIB " */

typedef struct NkPciHeader {
    nku16_f vend_id;	/* PCI vendor ID */
    nku16_f dev_id;	/* PCI vendor ID */
} NkPciHeader;

/*
 * ---------------------------------------------------------------------
 * Specific data for "PCI bus bridge" type of device
 * (NK_DEV_TYPE_PCI_BRIDGE)
 */

#define NK_PCI_BUS_BITS		8	/* bus number uses 8 bits */
#define	NK_PCI_BUS_MASK		((1 << NK_PCI_BUS_BITS) - 1)

#define NK_PCI_DEV_BITS		5	/* dev number uses 5 bits */
#define	NK_PCI_DEV_MASK		((1 << NK_PCI_DEV_BITS) - 1)

#define NK_PCI_FUN_BITS		3	/* func number uses 3 bits */
#define	NK_PCI_FUN_MASK		((1 << NK_PCI_FUN_BITS) - 1)

#define NK_PCI_OS_ID_BITS	5	/* OS IDs in [0..31] */
#define NK_PCI_OS_ID_MASK	((1 << NK_PCI_OS_ID_BITS) - 1)

#define	NK_PCI_CONF_SLOT(bus, dev, fun) \
    (((bus) << (NK_PCI_DEV_BITS + NK_PCI_FUN_BITS)) | \
     ((dev) << (NK_PCI_FUN_BITS)) | (fun))

#define	NK_PCI_CONF_ADDR(bus, dev, fun) \
    (NK_PCI_CONF_SLOT(bus, dev, fun) * NK_PCI_OS_ID_BITS)

#define	NK_PCI_CONF_BYTE(confaddr) ((confaddr) >> 3)
#define	NK_PCI_CONF_BIT(confaddr)  ((confaddr) & 0x7)
#define NK_PCI_CONF_SIZE(lastbus) \
    (NK_PCI_CONF_BYTE(NK_PCI_CONF_ADDR(lastbus, \
				       NK_PCI_DEV_MASK, NK_PCI_FUN_MASK)) \
     + 1)

    /*
     * Configuration space allocator.
     * Bit string giving, foreach (bus,dev,function), the ID of the OS owning
     * the PCI device.
     */
typedef nku8_f* NkPciConfSpace;

typedef nku32_f NkPciAddr;

typedef struct NkPciRange {
    NkPciAddr start;	/* PCI start address */
    NkPciAddr end;	/* PCI end address */
    NkPhAddr  base;	/* Translated CPU physical address of start */
} NkPciRange;

#define	NK_DEV_PCI_MEM_FIRST 1
#define	NK_DEV_PCI_MEM_LAST  3

    /*
     * Data for PCI bridge shared device type
     */
typedef struct NkDevPciBridge {
        /*
	 * Pointer to parent bus descriptor (self for PHB)
	 */
    NkPhAddr		parent;
        /*
	 * Local to secondary PCI bus mappings
	 * (4 windows max)
	 */
    NkPciRange     	pci [NK_DEV_PCI_MEM_LAST+1];
        /*
	 * Secondary PCI to local bus mapping (DMA)
	 * (1 window max)
	 */
    NkPciAddr		ram_base;
        /*
	 * Configuration space allocator
	 */
    nku32_f		indirect;	/* boolean: config. access */
    nku32_f		ind_addr;	/* address register offset */
    nku32_f		ind_data;	/* data    register offset */
    NkPhAddr		conf_space;	/* NkPciConfSpace physical addr.*/
    nku32_f		conf_lastbus;	/* max. bus available in conf_space */
        /*
	 * sub-buses available for that PCI bus
	 */
    nku32_f		bus_start;
    nku32_f		bus_end;
        /*
	 * PCI I/O and memory space allocators
	 */
    NkSpcDesc_32	io_space;	/* I/O space allocator    */
    NkSpcDesc_32	mem_space;	/* Memory space allocator */
} NkDevPciBridge;

    /*
     * Get ID of the OS owning a given PCI device configuration header
     */
    static inline NkOsId
nkPciConfGetOwner (NkPciConfSpace space, int bus, int dev, int fun)
{
    nku32_f addr  = NK_PCI_CONF_ADDR(bus, dev, fun);
    nku32_f index = NK_PCI_CONF_BYTE(addr);
    nku32_f shift = NK_PCI_CONF_BIT(addr);
    return((space[index] >> shift) & NK_PCI_OS_ID_MASK);
}
    /*
     * Set owner of a given PCI device configuration header
     */
    static inline void
nkPciConfSetOwner (NkPciConfSpace space,
		   int bus, int dev, int fun, NkOsId osId)
{
    nku32_f addr  = NK_PCI_CONF_ADDR(bus, dev, fun);
    nku32_f index = NK_PCI_CONF_BYTE(addr);
    nku32_f shift = NK_PCI_CONF_BIT(addr);
    space[index]  = ((space[index] & ~(NK_PCI_OS_ID_MASK << shift)) |
		     (osId << shift));
}

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized Motorola "QUICC" bus bridge device
 * (NK_DEV_ID_QUICCBUS)
 *
 * QUICC bus is virtualized by the primary kernel for all secondary kernels.
 *
 * The primary kernel initializes the QUICC virtual bridge setting up
 * various frequencies, the internal map physical address (immr),
 * the shared resources allocators (imap, brgs...) and the "cpcr_xirq" field
 * with the cross IRQ assigned to the CPCR update requests.
 *
 * When a secondary kernel wants to update the CPCR register (to send a CPM
 * command), it should follow the protocol below:
 * 	1) atomically clear down its kernel mask bit in "cpcr_update";
 * 	2) write new CPCR register value to the corresponding "cpcr[id]" field;
 * 	3) atomically raise up its kernel mask bit in the "cpcr_update" field;
 *	4) post the "cpcr_xirq" cross interrupt to the primary kernel;
 *	5) poll the "cpcr_update" field until the kernel mask bit is cleared
 *         down
 *	6) read updated CPCR value from "cpcr[id]" field
 * Note that the last item is optional and it is only needed if the secondary
 * kernel wants to get back updated result from the CPM processor.
 */

#define NK_QUICC_BRG_LIMIT	8
#define NK_QUICC_PORT_LIMIT	4

#define NK_QUICC_RESERVED	0x80
#define NK_QUICC_OS_ID(byte)	((byte) & ~NK_QUICC_RESERVED)

typedef struct NkDevQuiccBus {
    nku32_f		cpm_hz;				/* CPM clock */
    nku32_f		pit_hz;				/* PIT clock */
    nku32_f		brg_hz;				/* BRG clock */
    NkPhAddr		immr;				/* IMAP base */
    nku8_f		brg[NK_QUICC_BRG_LIMIT];	/* BRGs allocator */
    nku8_f		iopin[NK_QUICC_PORT_LIMIT][32]; /* I/O pin allocator */
    NkSpcDesc_32	imap;				/* IMAP allocator */
    volatile nku32_f	cpcr[NK_OS_LIMIT];		/* virtualized CPCR */
    volatile NkOsMask	cpcr_update;			/* cpcr update mask */
    NkXIrq		cpcr_xirq;			/* cpcr update xirq */
} NkDevQuiccBus;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized generic I/O (e.g. ISA) bus bridge device
 * (NK_DEV_ID_IOBUS)
 */

typedef struct NkDevIoBus {
    NkSpcDesc_32 io;			/* I/O space allocator */
} NkDevIoBus;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized "Programmable Interrupt Controller" type of
 * device (NK_DEV_ID_PIC).
 */

typedef nku32_f NkDevIrqMask;	/* IRQ mask for virtual PIC */

    /*
     * Per interrupt line info
     */
typedef struct NkDevPicIrq {
    NkDevIrqMask      irq_mask;   /* bit mask corresponding to that IRQ */
    volatile NkOsMask os_enabled; /* one bit set per attached OS */
    volatile NkOsMask os_pending; /* one bit set per pending OS */
    nku32_f	      vex_mask;   /* mask to use for posting the VEX */
    NkXIrq            xirq;	  /* cross interrupt used for acknowledgment */
    NkHwIrq	      hwirq;	  /* HW (real) intr. number.
				   * May be used to allocate a virtualized
				   * interrupt and to translate between
				   * virtual and real (HW) irq numbers.
				   */
} NkDevPicIrq;

    /*
     * Per Operating system info
     */
typedef struct NkDevPicOs {
    NkOsMask              os_mask;     /* bit mask corresponding to this OS */
    volatile NkDevIrqMask irq_pending; /* IRQs pending for that OS */
    NkPhAddr              vex_addr;    /* address to use for posting a VEX */
    NkCpuId		  vex_cpu;     /* destination CPU */
} NkDevPicOs;

    /*
     * Each NkDevPic virtualized device represents one or more cascaded
     * hardware interrupt controllers.
     * Like hardware PIC they may be cascaded (via next link) to
     * virtualize various hardware configurations.
     * One device can handle at most NK_HW_IRQ_LIMIT IRQs.
     */
typedef struct NkDevPic {
    NkHwIrq     irq_base;		/* system-wide number of dev IRQ 0 */
    NkHwIrq     irq_limit;		/* number of IRQ used in that dev  */
    NkXIrq	xirq;			/* cross interrupt used for config */
    NkDevPicIrq irq[NK_HW_IRQ_LIMIT];	/* Per IRQ info                    */
    NkDevPicOs  os[NK_OS_LIMIT];	/* Per OS  info                    */
} NkDevPic;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized "Tick" device (NK_DEV_ID_TICK).
 *
 * The TICK device is provided by the primary kernel for one or several
 * secondary kernels. Initially the "freq" field is set to zero by the
 * primary kernel indicating that the device is free.
 *
 * First secondary kernel which wants to use this device sets up the
 * "freq" field to the required frequency. It also sets up the corresponding
 * "xirq" field which will be used by the primary kernel to post the TICK
 * cross interrupt. Finally, the secondary kernel raises up the corresponding
 * bit in the "enabled" field and triggers the "xirq[NK_OS_PRIM]" cross
 * interrupt to the primary kernel in order to activate the TICK device.
 *
 * Other secondary kernels may use the same TICK device if its frequency
 * is convenient. Actions taken by such a secondary kernel are similar
 * to discribed above except the "freq" field is not altered.
 *
 * In order to disactivate the TICK device, a secondary kernel has to
 * clear down the corresponding bit in the "enabled" field and to trigger
 * the "xirq[NK_OS_PRIM]" cross interrupt to the primary kernel.
 *
 * Each time, the primary kernel posts the TICK cross interrupt to a
 * secondary kernel, the "counter" field is incremented.
 * So, the secondary kernel is able to detect the number of missed TICK
 * interrupts and take them into account to adjust the time. Note that
 * the "counter" field is read-only for the secondary kernels.
 */

typedef nku32_f NkTickFreq;		/* TICK device frequency in Hz */
typedef nku32_f NkTickCounter;		/* TICK counter */
typedef nku32_f NkTickPeriod;		/* TICK period in nanoseconds */

    /*
     * Data for the Tick virtualized device
     */
typedef struct NkDevTick {
    volatile NkOsMask	   enabled;	      /* connected kernels */
    NkTickFreq	           freq;	      /* TICK frequency in Hz */
    NkTickPeriod           period;	      /* TICK period in nanosecs */
    volatile NkTickCounter counter;	      /* counter of TICKs */
    NkXIrq                 xirq[NK_OS_LIMIT]; /* TICK cross IRQs */
    volatile nku64_f       last_stamp;	      /* time stamp of last tick */
/*#ifdef CONFIG_POWER_SAVING*/
    volatile unsigned char atomic;
    /* this is the physical address of the register */
    /* this means that we need to use phystovirt primitive */
    /* phys_to_virt in order to convert the access before reading*/
    volatile unsigned short *qbc;
/*#endif*/
    volatile NkTickCounter silent[NK_OS_LIMIT];
    volatile NkTickCounter delta[NK_OS_LIMIT];
} NkDevTick;

    /*
     * Data for the local Tick virtualized device
     */
typedef struct NkDevLTick {
    NkCpuId   cpuid;	/* CPU ID */
    NkDevTick tick;	/* virtual tick */
} NkDevLTick;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized "WATCHDOG" device (NK_DEV_ID_WDT).
 *
 * The WATCHDOG device is provided by the primary kernel for one or several
 * secondary kernels. Initially the "period" field is set to zero by the
 * primary kernel indicating that the device is free.
 *
 * First secondary kernel which wants to use this device sets up the
 * "period" field to the required period (in milliseconds).
 * It also sets up the corresponding "xirq" field which will be used by
 * secondary kernels to post the (re)configuration cross interrupt.
 * Finally, the secondary kernel raises up the corresponding
 * bit in the "enabled" field and triggers the "xirq" cross
 * interrupt to the primary kernel in order to activate the WATCHDOG device.
 *
 * Other secondary kernels may use the same WATCHDOG device if its period
 * is convenient. Actions taken by such a secondary kernel are similar
 * to discribed above except the "period" field is not altered.
 *
 * In order to disactivate the WATCHDOG device, a secondary kernel has to
 * clear down the corresponding bit in the "enabled" field and to trigger
 * the "xirq" cross interrupt to the primary kernel.
 *
 * The "pat" field is used by secondary kernels to periodically pat
 * the virtual watchdog.
 * If a secondary kernel doesn't pat the virtual watchdog during the
 * watchdog period, the faulting kernel is restarted by the primary kernel.
 * The primary kernel also reset pat bits at each watchdog period.
 */

typedef nku32_f NkWdtPeriod;		      /* WDT period in milliseconds */
typedef nku32_f NkWdtVex;		      /* WDT virtual exception */

    /*
     * Data for the Wdt virtualized device
     */
typedef struct NkDevWdt {
    volatile NkOsMask	  enabled;	      /* connected kernels */
    volatile NkOsMask     pat;                /* one bit set per attached OS */
    NkWdtPeriod           period;	      /* WDT period in millisecs */
    NkXIrq                xirq;               /* WDT cross IRQ */
    NkPhAddr              vex_addr[NK_OS_LIMIT];  /* OS vex addresses */
    NkWdtVex              vex_mask[NK_OS_LIMIT];  /* OS vex masks */
} NkDevWdt;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized "RTC" device (NK_DEV_ID_RTC).
 *
 * The RTC is provided by the primary kernel for all secondary kernels.
 *
 * The primary kernel initializes the RTC virtual device setting up
 * the "utime[NK_OS_PRIM]" field to the current universal time,
 * the "stamp" field to zero and the "xirq" field the cross IRQ assigned
 * to the RTC update requests. Each time the primary kernel updates
 * the current universal time in the "utime[NK_OS_PRIM]" field, the "stamp"
 * field is incremented.
 *
 * When a secondary kernel needs to know the current universal time, it
 * should follow the "query" protocol below:
 * 	1) read the "stamp" filed;
 *	2) post the "xirq" cross interrupt to the primary kernel;
 *	3) poll the "stamp" field until it is changed;
 * 	4) read the current universal time from the "utime[NK_OS_PRIM]" field.
 *
 * When a secondary kernel wants to update the universal time, it should
 * follow the "update" protocol below:
 * 	1) atomically clear down the kernel mask bit in the "update" field;
 * 	2) write new universal time to the corresponding "utime[id]" field;
 * 	3) atomically raise up the kernel mask bit in the "update" field;
 *	4) post the "xirq" cross interrupt to the primary kernel;
 *	5) poll the "update" filed until the kernel mask bit is cleared down
 * Note that the last item is optional and it is only needed if the secondary
 * kernel wants to be synchronized with the universal time update made by the
 * primary kernel.
 */

typedef struct NkUnivTime {		/* Universal Time since Jan 1, 1970 */
    nku32_f secs;			/* seconds */
    nku32_f nsecs;			/* nano-seconds */
} NkUnivTime;

typedef nku32_f NkTimeStamp;		/* time stamp */

    /*
     * Data for the RTC virtualized device
     */
typedef struct NkDevRTC {
    volatile NkTimeStamp stamp;		     /* primary utime update stamp */
    volatile NkUnivTime  utime[NK_OS_LIMIT]; /* universal times */
    volatile NkOsMask	 update;	     /* update mask */
    NkXIrq	         xirq;		     /* update cross interrupt */
} NkDevRTC;

/*
 * ---------------------------------------------------------------------
 * Specific data for "Inter-System Communication bridge" type of device
 * (NK_DEV_ID_SYSBUS)
 */

    /*
     * Data for per OS SYSCOM pseudo bridge device
     */
typedef struct NkDevSysBridge {
    NkPhAddr      pmem_start;	/* exported memory region start address */
    NkPhSize      pmem_size;    /* exported memory region size */
    NkXIrq        xirq_base;	/* cross interrupt base */
    NkXIrq        xirq_limit;	/* number of cross interrupts */
    NkTimeStamp   stamp;	/* time stamp */
} NkDevSysBridge;

    /*
     * Data for SYSCOM pseudo bus type
     */
typedef struct NkDevSysBus {
    volatile NkOsMask connected;	   /* connected bridges */
    NkDevSysBridge    bridge[NK_OS_LIMIT]; /* per OS bridge instances */
} NkDevSysBus;

/*
 * ---------------------------------------------------------------------
 * Specific data for I/O range used by Nano-Kernel (NK_DEV_ID_NKIO)
 */

typedef struct NkDevNkIO {
    nku32_f	base;	/* I/O range base address */
    nku32_f	size;	/* I/O range size */
} NkDevNkIO;

/*
 * ---------------------------------------------------------------------
 * Specific data for virtualized "Advanced Programmable Interrupt Controller"
 * type of device (NK_DEV_ID_APIC).
 */

#define	NK_APIC_PIN_LIMIT 32

    /*
     * Per interrupt pine info
     */
typedef struct NkDevApicPin {
    nku32_f irq;	 /* IRQ routing */
    nku32_f vector; 	 /* IRQ vector */
    nku32_f level;	 /* 0/1 -> edge/level */
    NkOsId  owner;	 /* pin owner */
} NkDevApicPin;

    /*
     * Each NkDevPic virtualized device represents one I/O APIC.
     * One device can handle at most NK_APIC_PIN_LIMIT pins.
     */
typedef struct NkDevApic {
    nku32_f	 id;				/* APIC ID */
    nku32_f 	 npins;				/* number of pins */
    NkDevApicPin pins[NK_APIC_PIN_LIMIT]; 	/* per pin info */
} NkDevApic;

/*
 * ---------------------------------------------------------------------
 * Specific data for a communication ring device (NK_DEV_ID_RING).
 *
 * The ring device is provided by a consumer OS for a given
 * producer OS. The decriptor fields are initialized as follows.
 *
 * The "pid" field is normally set to the consumer OS ID.
 * When the ring consumer is unknown, the "pid" field is set to zero.
 * In this case, the "pid" field will be set by the producer at connection
 * time.
 *
 * The "type" field specifies the ring type. It is typically set to
 * a 4 characters ASCII string (e.g., "DISK" for a remote disk ring).
 *
 * The "cxirq" field specifies the XIRQ number used for consumer notifications.
 *
 * Initially the "pxirq" field is set to zero by the
 * consumer indicating that the ring device is disconnected.
 * Once the producer is connected to the ring, the "pxirq" field
 * is set to a cross IRQ number which should be used for the producer
 * notification.
 *
 * The "dsize" field specifies the ring descriptor size.
 *
 * The "imask" field specifies the ring index mask. Note that the ring
 * indexes ("iresp" and "ireq") are free run. They must always be
 * bitwise anded with "imask" when used in order to clear up most
 * significant bits.
 *
 * The "iresp" field specifies the first descriptor index available for
 * a consumer response. This field is updated by the consumer once the
 * response decriptor is filled in. Typically, the consumer sends
 * the "pxirq" cross interrupt when "iresp" index is updated.
 *
 * The "ireq" field specifies the first descriptor index available for
 * a producer request. This field is updated by the producer once the
 * response decriptor is filled in. Typically, the producer sends
 * the "cxirq" cross interrupt when "ireq" index is updated.
 *
 * The "ibase" field specifies the ring physical address.
 * Note that the ring is always contiguous in the physical space.
 */

typedef struct NkDevRing {
    NkOsId	pid;		/* producer OS ID */
    nku32_f	type;		/* ring type */
    nku32_f	cxirq;		/* consumer XIRQ */
    nku32_f	pxirq;		/* producer XIRQ */
    nku32_f	dsize;		/* ring descriptor size */
    nku32_f	imask;		/* ring index mask */
    nku32_f 	iresp;		/* consumer response ring index */
    nku32_f	ireq;		/* producer request ring index */
    NkPhAddr	base;		/* ring physical base address */
} NkDevRing;

/*
 * ---------------------------------------------------------------------
 * Specific data for (video) frame buffer (NK_DEV_ID_FB)
 */

typedef struct NkDevFb {
    NkPhAddr    start;          /* start of framebuffer memory */
    NkPhSize    size;           /* size of framebuffer memory */
    nku32_f     bpp;            /* bits per pixel */
    nku32_f     width;          /* screen width */
    nku32_f     height;         /* screen height */
} NkDevFb;

/*
 * ---------------------------------------------------------------------
 * Specific data for shared memory (NK_DEV_ID_SHM)
 */
typedef struct NkDevShm {
    NkPhAddr    start;
    NkPhSize    size;
} NkDevShm;

/*
 * ---------------------------------------------------------------------
 * Specific data for a virtual serial I/O device (NK_DEV_ID_SIO).
 *
 * The virtual serial I/O device is provided by a backend driver
 * (typically running in the primary OS) toward a frontend driver
 * (typically running in a secondary OS).
 *
 * The "port" field specifies the hardware port assosiated to
 * the serial I/O device. This field is backend driver specific
 * and it is never used by a frontend driver.
 *
 * The "type" field specifies the serial I/O controller type.
 * Types are inherited from Linux serio.h.
 *
 * The "fid" field is normally set by the backend driver to specify the
 * frontend OS ID. When the frontend OS ID is unknown, the "fid" field
 * is set to zero. In this case, the "fid" field will be set by the
 * frontend driver at connection time.
 *
 * The "fxirq" field specifies the XIRQ number used for frontend notifications.
 *
 * The "bxirq" field specifies the XIRQ number used for backend notifications.
 *
 * The "iring" and "oring" structures specifiy the input  and
 * output circular buffers respectively.
 *
 * The "start" field specifies the first character index available for
 * consumer. This field is updated by the consumer once the character
 * is read. Typically, the consumer sends a cross interrupt when the
 * "start" index is updated.
 *
 * The "limit" field specifies the first character index available for
 * producer request. This field is updated by the producer once a
 * character is put in the ring. Typically, the producer sends
 * a cross interrupt when the "limit" index is updated.
 *
 * The "ring" array keeps characters sent from producer to consumer.
 */

#define	NK_DEV_SIO_RING_SIZE	64
#define	NK_DEV_SIO_RING_MASK	(NK_DEV_SIO_RING_SIZE-1)
#define	NK_DEV_SIO_IDX(x)	((x) & NK_DEV_SIO_RING_MASK)

typedef struct NkDevSioRing {
    nku32_f     start;				/* ring start index */
    nku32_f     limit;				/* ring limit index */
    nku8_f      ring[NK_DEV_SIO_RING_SIZE];	/* circular buffer */
} NkDevSioRing;

typedef struct NkDevSio {
    nku32_f    	 port;		/* device port (backend specific) */
    nku32_f    	 type;		/* device type */
    NkOsId	 fid;		/* frontend OS ID */
    nku32_f	 fxirq;		/* frontend XIRQ */
    nku32_f	 bxirq;		/* backend XIRQ */
    NkDevSioRing iring;		/* input ring */
    NkDevSioRing oring;		/* output ring */
} NkDevSio;

#define NK_DEV_SIO_TYPE			0xff000000UL
#define NK_DEV_SIO_TYPE_XT		0x00000000UL
#define NK_DEV_SIO_TYPE_8042		0x01000000UL
#define NK_DEV_SIO_TYPE_RS232		0x02000000UL
#define NK_DEV_SIO_TYPE_HIL_MLC		0x03000000UL
#define NK_DEV_SIO_TYPE_PS_PSTHRU	0x05000000UL
#define NK_DEV_SIO_TYPE_8042_XL  	0x06000000UL

/*
 * ---------------------------------------------------------------------
 * Specific data for Virtual VGA device
 * (NK_DEV_ID_VGA)
 */

    /*
     * VGA registers
     */
#define NK_DEV_VGA_CRT_MAX   	0x19 /* number of CRT regs */
#define NK_DEV_VGA_ATT_MAX   	0x15 /* number of attribute regs */
#define NK_DEV_VGA_GFX_MAX   	0x09 /* number of graphics regs */
#define NK_DEV_VGA_SEQ_MAX   	0x05 /* number of sequencer regs */
#define NK_DEV_VGA_MIS_MAX   	0x01 /* number of misc output regs */

#define	NK_DEV_VGA_NCOLORS	256  /* number of VGA colors */
#define	NK_DEV_VGA_PAL(x)	((x)*3)

typedef struct NkDevVgaRegs {
    nku8_f crt[NK_DEV_VGA_CRT_MAX];    /* CRT regs */
    nku8_f att[NK_DEV_VGA_ATT_MAX];    /* atribute regs */
    nku8_f gfx[NK_DEV_VGA_GFX_MAX];    /* graphics regs */
    nku8_f seq[NK_DEV_VGA_SEQ_MAX];    /* sequencer regs */
    nku8_f mis[NK_DEV_VGA_MIS_MAX];    /* misc output regs */
    nku8_f pal[NK_DEV_VGA_NCOLORS*3];  /* palette */
} NkDevVgaRegs;

    /*
     * VGA memory planes
     */
#define	NK_DEV_VGA_PLANE_SIZE	 0x10000	/* 64K */
#define	NK_DEV_VGA_PLANE_MAX	 4		/* four planes */
#define	NK_DEV_VGA_PLANE_BASE(x) (NK_DEV_VGA_PLANE_SIZE * (x))
#define	NK_DEV_VGA_MEM_SIZE	 (NK_DEV_VGA_PLANE_SIZE * NK_DEV_VGA_PLANE_MAX)

    /*
     * State of VGA controller (registers & video memory)
     */
typedef struct NkDevVgaState {
    NkPhAddr      regs;		/* VGA registers */
    NkPhAddr      mem;		/* VGA memory planes */
} NkDevVgaState;

    /*
     * Data for Virtual VGA device
     */
typedef struct NkDevVga {
    NkOsId        owner;		 /* VGA owner */
    NkOsMask      enabled;		 /* enabled guest VGA devices */
    NkOsMask      connected;	 	 /* connected guest VGA devices */
    NkDevVgaState vgas[NK_OS_LIMIT]; 	 /* per guest OS VGA instances */
} NkDevVga;

#endif
