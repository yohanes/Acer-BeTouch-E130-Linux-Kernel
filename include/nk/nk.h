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
 * #ident  "@(#)nk.h 1.31     05/09/21 Jaluna"
 *
 * Contributor(s):
 *   Eric Lescouet <eric@lescouet.org> - Jaluna SA
 *   Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
 *
 ****************************************************************
 */

#ifndef	_NK_NK_H
#define	_NK_NK_H

    /*
     * NanoKernel DDI class name
     */
#define NK_CLASS "nanokernel"

    /*
     * Version number of the NanoKernel device driver interface.
     */
typedef enum {
    NK_VERSION_INITIAL = 0,	/* initial version */
	/*
	 * In version 1 the following methods have been added:
	 *    nk_cpu_id_get() - returns the current NK CPU ID
	 *    nk_xvex_post() - post a virtual exception to a remote CPU
	 *    nk_local_xirq_post() - post a local cross interrrupt
	 *    nk_xirq_attach_masked() - attach "masked" cross IRQ handler
	 */
    NK_VERSION_1,		/* SMP version */
        /* In version 2 the following method have been added:
	 * nk_scheduler() - set/get secondary fair-share scheduling parameters
	 */
    NK_VERSION_2		/* scheduler parameters */
} NkVersion;

    /*
     * Shared device descriptor
     * These descriptors are used to build the NanoKernel repository
     */
typedef nku32_f NkDevClass;
typedef nku32_f NkDevId;

typedef struct NkDevDesc {
    NkPhAddr	next;		/* linked list of devices      */
    NkDevClass	class_id;	/* class of device             */
    NkDevId	dev_id;		/* kind of device within class */
    NkPhAddr	class_header;	/* class  specific header      */
    NkPhAddr	dev_header;	/* device specific header      */
    NkOsId	dev_lock;	/* may be used as the global spin lock */
    NkOsId      dev_owner;	/* device owner */
} NkDevDesc;

    /*
     * space allocators used for various memory and I/O spaces.
     * (RAM, bus memory or I/O spaces)
     */
typedef nku32_f NkSpcTag;

#define NK_SPC_FREE		0x00000001 /* Available space */
#define NK_SPC_ALLOCATED	0x00000002 /* Allocated space */
#define NK_SPC_RESERVED		0x00000004 /* Reserved  space */
#define NK_SPC_NONEXISTENT	0x00000010 /* Hole in space   */

#define NK_SPC_STATE(tag)	(tag & 0xffff)
#define NK_SPC_OWNER(tag)	(((tag) >> 16) & (NK_OS_LIMIT-1))
#define NK_SPC_TAG(s, id)	((((id) & (NK_OS_LIMIT-1)) << 16) | s)

#define NK_SPC_MAX_CHUNK 128	/* maximum chunk number per descriptor */

    /*
     * 32 bits space allocator
     */
typedef struct NkSpcChunk_32 {
    nku32_f  start;
    nku32_f  size;
    NkSpcTag tag;
} NkSpcChunk_32;

typedef struct NkSpcDesc_32 {
    nku32_f		maxChunks;
    nku32_f		numChunks;
    NkSpcChunk_32	chunk[NK_SPC_MAX_CHUNK];
} NkSpcDesc_32;

extern void    nk_spc_init_32 (NkSpcDesc_32* desc);
extern void    nk_spc_tag_32  (NkSpcDesc_32* desc,
			       nku32_f       addr,
			       nku32_f       size,
			       NkSpcTag      tag);
extern void    nk_spc_free_32 (NkSpcDesc_32* desc,
			       nku32_f       addr,
			       nku32_f       size);
extern int     nk_spc_alloc_any_32 (NkSpcDesc_32* desc,
				    nku32_f*      addr,
				    nku32_f       size,
				    nku32_f       align,
				    NkSpcTag       tag);
extern int     nk_spc_alloc_within_range_32 (NkSpcDesc_32* desc,
					     nku32_f*      addr,
					     nku32_f       size,
					     nku32_f       align,
					     nku32_f       lo,
					     nku32_f       hi,
					     NkSpcTag      owner);
extern int     nk_spc_alloc_32 (NkSpcDesc_32* desc,
				nku32_f       addr,
				nku32_f       size,
				NkSpcTag      owner);
extern void    nk_spc_release_32 (NkSpcDesc_32* desc, NkSpcTag tag);
extern nku32_f nk_spc_size_32 (NkSpcDesc_32* desc);
extern void    nk_spc_dump_32 (NkSpcDesc_32* desc);

    /*
     * 64 bits space allocator
     */
typedef struct NkSpcChunk_64 {
    nku64_f  start;
    nku64_f  size;
    NkSpcTag tag;
} NkSpcChunk_64;

typedef struct NkSpcDesc_64 {
    nku32_f		maxChunks;
    nku32_f		numChunks;
    NkSpcChunk_64	chunk[NK_SPC_MAX_CHUNK];
} NkSpcDesc_64;

extern void    nk_spc_init_64 (NkSpcDesc_64* desc);
extern void    nk_spc_tag_64  (NkSpcDesc_64* desc,
			       nku64_f       addr,
			       nku64_f       size,
			       NkSpcTag      tag);
extern void    nk_spc_free_64 (NkSpcDesc_64* desc,
			       nku64_f       addr,
			       nku64_f       size);
extern int     nk_spc_alloc_any_64 (NkSpcDesc_64* desc,
				    nku64_f*      addr,
				    nku64_f       size,
				    nku64_f       align,
				    NkSpcTag       tag);
extern int     nk_spc_alloc_within_range_64 (NkSpcDesc_64* desc,
					     nku64_f*      addr,
					     nku64_f       size,
					     nku64_f       align,
					     nku64_f       lo,
					     nku64_f       hi,
					     NkSpcTag      owner);
extern int     nk_spc_alloc_64 (NkSpcDesc_64* desc,
				nku64_f       addr,
				nku64_f       size,
				NkSpcTag      owner);
extern void    nk_spc_release_64 (NkSpcDesc_64* desc, NkSpcTag tag);
extern nku64_f nk_spc_size_64 (NkSpcDesc_64* desc);
extern void    nk_spc_dump_64 (NkSpcDesc_64* desc);

    /*
     * The NanoKernel hardware interrupts provide a common shared
     * representation of the hardware interrupts, between the different
     * operating systems.
     * In other words, each system kernel maps its own interrupt
     * representation to/from the NanoKernel one.
     */
typedef nku32_f NkHwIrq;

    /*
     * Maximum number of HW irq per virtualized device (PIC)
     */
#define NK_HW_IRQ_LIMIT	32


    /*
     * NanoKernel cross interrupt handlers.
     *
     * The <cookie> argument is an opaque given by the client when calling
     * irq_attach(). It is passed back when called.
     *
     * May be called from hardware interrupt context
     */
typedef void (*NkXIrqHandler) (void* cookie, NkXIrq xirq);
    /*
     * Identifiers of xirq handler's attachement
     */
typedef void* NkXIrqId;

#define	NK_XIRQ_SYSCONF	0	/* system (re-)condifuration cross interrupt */
#define	NK_XIRQ_FREE	1	/* first free cross interrupt */

    /*
     * NanoKernel generic DDI operations
     */
typedef struct NkDevOps {

    NkVersion nk_version;	/* DDI version */

            /*
	     * Lookup first device of given class, into NanoKernel repository
	     *
	     * Return value:
	     *
	     * if <curr_dev> is zero the first intance of a device of class
	     * <dev_class> is returned. Otherwise <curr_dev> must be an
	     * address returned by a previous call to dev_lookup_xxx().
	     * The next device of class <dev_class>, starting from <curr_dev>
	     * is returned in that case.
	     *
	     * NULL is returned, if no device of the required class is found.
	     */
        NkPhAddr
    (*nk_dev_lookup_by_class) (NkDevClass dev_class, NkPhAddr curr_dev);
            /*
	     * Lookup first device of given type, into NanoKernel repository.
	     *
	     * Return value:
	     *
	     * if <curr_dev> is zero the first intance of a device of type
	     * <dev_type> is returned. Otherwise <curr_dev> must be an
	     * address returned by a previous call to dev_lookup_xxx().
	     * The next device of class <dev_type>, starting from <curr_dev>
	     * is returned in that case.
	     *
	     * NULL is returned, if no device of the required type is found.
	     */
        NkPhAddr
    (*nk_dev_lookup_by_type) (NkDevId dev_id, NkPhAddr curr_dev);

            /*
	     * Allocates a contiguous memory block from NanoKernel repository
	     */
        NkPhAddr
    (*nk_dev_alloc) (NkPhSize size);

            /*
	     * Add a new device to NanoKernel repository.
	     *
	     * <dev> is a physical address previously returned by
	     * dev_alloc(). It must points to a completed NkDevDesc structure.
	     */
        void
    (*nk_dev_add) (NkPhAddr dev);

            /*
	     * Get self operating system ID
	     */
        NkOsId
    (*nk_id_get) (void);

            /*
	     * Get last running operating system ID
	     * (eg highest possible value for an ID)
	     */
        NkOsId
    (*nk_last_id_get) (void);

            /*
	     * Get a mask of started operating system's ID
	     * -> bit <n> is '1' if system with NkOsId <n> is started.
	     * -> bit <n> is '0' if system with NkOsId <n> is stopped.
	     */
        NkOsMask
    (*nk_running_ids_get) (void);

            /*
	     * Convert a physical address into a virtual one.
	     */
        void*
    (*nk_ptov) (NkPhAddr paddr);

            /*
	     * Convert a virtual address into a physical one.
	     */
        NkPhAddr
    (*nk_vtop) (void* vaddr);

        /*
	 * Get the "virtualized exception" mask value to be used to
	 * post a given hardware interrupt.
	 */
        nku32_f
    (*nk_vex_mask) (NkHwIrq hirq);

        /*
	 * Get physical address to be used by another system to post a
	 * "virtualized exception" to current system
	 * (eg: where to write the mask returned from vex_mask_get()).
	 */
        NkPhAddr
    (*nk_vex_addr) (NkPhAddr dev);

        /*
	 * Allocates required number of contiguous unused cross-interrupts.
	 *
	 * Return the number of the first interrupt of allocated range,
	 * or 0 if not enough xirq are available.
	 */
        NkXIrq
    (*nk_xirq_alloc) (NkXIrq nb);

        /*
	 * Attach a handler to a given NanoKernel cross-interrupt.
	 * (0 returned on failure)
	 * Must be called from base level.
	 * The handler is called with ONLY masked cross interrupt source.
	 */
        NkXIrqId
    (*nk_xirq_attach) (NkXIrq        xirq,
		       NkXIrqHandler hdl,
		       void*         cookie);

        /*
	 * Mask a given cross-interrupt
	 */
        void
    (*nk_xirq_mask) (NkXIrq xirq);

        /*
	 * Unmask a given cross-interrupt
	 */
        void
    (*nk_xirq_unmask) (NkXIrq xirq);

        /*
	 * Detach a handler (previously attached with irq_attach())
	 *
	 * Must be called from base level
	 */
        void
    (*nk_xirq_detach) (NkXIrqId id);

        /*
	 * Trigger a cross-interrupt to a given operating system.
	 *
	 * Must be called from base level
	 */
        void
    (*nk_xirq_trigger) (NkXIrq xirq, NkOsId osId);

        /*
	 * Get high priority bit set in bit mask.
	 */
        nku32_f
    (*nk_mask2bit) (nku32_f mask);

        /*
	 * Insert a bit of given priority into a mask.
	 */
        nku32_f
    (*nk_bit2mask) (nku32_f bit);

        /*
	 * Atomic operation to clear bits within a bit field.
	 *
	 * The following logical operation: *mask &= ~clear
	 * is performed atomically.
	 */
        void
    (*nk_atomic_clear) (volatile nku32_f* mask, nku32_f clear);

        /*
	 * Atomic operation to clear bits within a bit field.
	 *
	 * The following logical operation: *mask &= ~clear
	 * is performed atomically.
	 *
	 * Returns 0 if and only if the result is zero.
	 */
        nku32_f
    (*nk_clear_and_test) (volatile nku32_f* mask, nku32_f clear);

        /*
	 * Atomic operation to set bits within a bit field.
	 *
	 * The following logical operation: *mask |= set
	 * is performed atomically
	 */
        void
    (*nk_atomic_set) (volatile nku32_f* mask, nku32_f set);

        /*
	 * Atomic operation to substract value to a given memory location.
	 *
	 * The following logical operation: *ptr -= val
	 * is performed atomically.
	 */
        void
    (*nk_atomic_sub) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Atomic operation to substract value to a given memory location.
	 *
	 * The following logical operation: *ptr -= val
	 * is performed atomically.
	 *
	 * Returns 0 if and only if the result is zero.
	 */
        nku32_f
    (*nk_sub_and_test) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Atomic operation to add value to a given memory location.
	 *
	 * The following logical operation: *ptr += val
	 * is performed atomically
	 */
        void
    (*nk_atomic_add) (volatile nku32_f* ptr, nku32_f val);

        /*
	 * Map a physical address range of "shared" memory
	 * into supervisor space (RAM from other systems).
	 *
	 * On error, NULL is returned.
	 */
        void*
    (*nk_mem_map) (NkPhAddr paddr, NkPhSize size);
        /*
	 * Unmap memory previously mapped using nk_mem_map.
	 */
        void
    (*nk_mem_unmap) (void* vaddr, NkPhAddr paddr, NkPhSize size);

	/*
	 * Get NK CPU ID.
	 */
	NkCpuId
    (*nk_cpu_id_get) (void);

        /*
	 * Post a virtual exception (VEX) to a given operating system running
	 * on a given CPU.
	 * Must be called from base level
	 */
	void
    (*nk_xvex_post) (NkCpuId cpuid, NkOsId osid, NkVex vex);

    /*
	 * Post a cross-interrupt to a given operating system running
	 * on the same CPU.
	 * Must be called from base level
	 */
        void
    (*nk_local_xirq_post) (NkXIrq xirq, NkOsId osId);

        /*
	 * Attach a handler to a given NanoKernel cross-interrupt.
	 * (0 returned on failure)
	 * Must be called from base level.
	 * The handler is called with ALL CPU interrupts masked.
	 */
        NkXIrqId
    (*nk_xirq_attach_masked) (NkXIrq        xirq,
		              NkXIrqHandler hdl,
		              void*         cookie);

	/*
	 * Get scheduling paramteres of a given operating system
	 */
	void
    (*nk_scheduler) (NkOsId osId, NkSchedParams* oldp,
                     NkSchedParams* newp);

} NkDevOps;

#endif
