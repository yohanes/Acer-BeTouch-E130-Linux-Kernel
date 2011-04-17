/*
 ****************************************************************
 *
 * Component = Nano-Kernel Device Driver Interface (NK DDI)
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)ddi.c 1.76     06/03/13 Jaluna"
 *
 * Contributor(s):
 *	Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
 *	Guennadi Maslov <guennadi.maslov@jaluna.com>
 *      Chi Dat Truong <chidat.truong@jaluna.com>
 *
 ****************************************************************
 */

/*
 * This driver provides the generic NKDDI services to NK specific drivers
 * when the Linux kernel runs on top of the nano-kernel and it plays
 * ether primary or secondary role.
 */

#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <nk/nkern.h>

#include <asm/nkern.h>

#include <asm/page.h>
#include <asm/system.h>
#include <asm/hardirq.h>
#include <asm/io.h>
#include <asm/irq_regs.h>

/*
 * This driver provides the generic NKDDI to NK specific drivers
 */
#define DTRACE(FORMAT, ...) /* printk("%s" FORMAT, __func__ ,## __VA_ARGS__) */


#define	NKCTX()	os_ctx

/*
 * All shared between OS'es memory is accessed through
 * compatible one-to-one mapping
 */
#define	PTOV(x)	__va(x)
#define	VTOP(x)	__pa(x)

#define	VDEV(x)	((NkDevDesc*)PTOV(x))

typedef void (*NkXIrqHandler_f) (void*, NkXIrq);

typedef struct XIrqCall {
    struct XIrqCall* next;
    NkXIrqHandler_f  hdl;
    void*	     cookie;
    NkXIrq	     xirq;
    unsigned int     masked;
} XIrqCall;

typedef struct NkDev {
    NkXIrqMask*  xirqs;				   /* XIRQs table */
    NkXIrqMask*  xpending;			   /* pending XIRQs */
    spinlock_t   xirq_b_lock;			   /* base lock */
    spinlock_t   xirq_i_lock;			   /* interrupt lock */
    int		 xirq_free;	 		   /* first free cross IRQ */
    NkOsMask*    running;			   /* mask of running OSes */
    NkXIrqMask	 xirq_xmask;		 	   /* XIRQ enable mask  */
    unsigned int xirq_xcount[NK_XIRQ_LIMIT];	   /* XIRQ enable count  */
    XIrqCall*    xirq_call[NK_XIRQ_LIMIT];	   /* cross IRQ calls */
    unsigned int xirq_masked[NK_XIRQ_LIMIT];	   /* masked flags */
} NkDev;

static NkDev nkdev;

    static void
nk_panic(void)
{
    for(;;){ /* endless loop */}
}

    /*
     * We make provision for some statically allocated XIrqCall data.
     * These data are taken from a static pool and allow to attach
     * xirq in the kernel init. path, before the kmem cache is initialized
     * (kmalloc not usable yet).
     *
     * XIRQ_STATIC_CALLS defines the size of the XIrqCall static pool
     * (number of xirq to be attached before kmalloc() is inited)
     */
#define XIRQ_STATIC_CALLS	8

static XIrqCall  _xcalls_pool[XIRQ_STATIC_CALLS];
static XIrqCall* _xcalls_free;

#define	IS_XIRQ_CALL_STATIC(x) \
	(((_xcalls_pool <= (x)) && ((x) < (_xcalls_pool + XIRQ_STATIC_CALLS))))

    static XIrqCall*
_xirq_call_alloc (void)
{
    XIrqCall* call;

    spin_lock(&(nkdev.xirq_b_lock));

    call = _xcalls_free;
    if (call) {
	_xcalls_free = call->next;
    }

    spin_unlock(&(nkdev.xirq_b_lock));

    if (!call) {
        call = (XIrqCall*)kmalloc(sizeof(XIrqCall), GFP_KERNEL);
    }

    return call;
}

    static void
_xirq_call_free (XIrqCall* call)
{
    if (IS_XIRQ_CALL_STATIC(call)) {
        spin_lock(&(nkdev.xirq_b_lock));
	call->next   = _xcalls_free;
	_xcalls_free = call;
        spin_unlock(&(nkdev.xirq_b_lock));
	return;
    }
    kfree(call);
}

    static void
_xirq_call_init (void)
{
    int i;
    _xcalls_free = 0;
    for (i = 0; i < XIRQ_STATIC_CALLS; i++) {
	_xirq_call_free(_xcalls_pool + i);
    }
}

/*
 * Notification call-back
 */

static void (*_dev_notify)(void) = 0;

    int
nk_dev_notification_register (void (*handler) (void))
{
    if (_dev_notify) {
	return 0;
    }
    _dev_notify = handler;
    return 1;
}

    static void
nk_dev_notification (void)
{
    if (_dev_notify) {
        _dev_notify();
    }
}

/*
 * Interface methods...
 */

    /*
     * Get the "virtualized exception" mask value to be used to
     * post a processor exception.
     */
    static nku32_f
nk_vex_mask (NkHwIrq irq)
{
    return (NK_VEX_RUN | NK_VEX_IRQ);
}

    /*
     * Get physical address to be used by another system to post a
     * "virtualized exception" to current system
     * (where to write the mask returned from vexcep_mask()).
     */
    static NkPhAddr
nk_vex_addr (NkPhAddr dev)
{
    return VTOP(&(NKCTX()->pending));
}

    /*
     * Lookup first device of given class, into NanoKernel repository
     *
     * Return value:
     *
     * if <cdev> is zero the first intance of a device of class
     * <dclass> is returned. Otherwise <cdev> must be an
     * address returned by a previous call to dev_lookup_xxx().
     * The next device of class <dclass>, starting from <cdev>
     * is returned in that case.
     *
     * 0 is returned, if no device of the required class is found.
     */
    static NkPhAddr
nk_dev_lookup_by_class (NkDevClass cid, NkPhAddr cdev)
{
    cdev = (cdev ? VDEV(cdev)->next : NKCTX()->dev_info);
    while (cdev && (VDEV(cdev)->class_id != cid)) {
	cdev = VDEV(cdev)->next;
    }
    return cdev;
}

    /*
     * Lookup first device of given type, into NanoKernel repository.
     *
     * Return value:
     *
     * if <cdev> is zero the first intance of a device of type
     * <did> is returned. Otherwise <cdev> must be an
     * address returned by a previous call to dev_lookup_xxx().
     * The next device of type <did>, starting from <cdev>
     * is returned in that case.
     *
     * NULL is returned, if no device of the required type is found.
     */
    static NkPhAddr
nk_dev_lookup_by_type (NkDevId did, NkPhAddr cdev)
{
    cdev = (cdev ? VDEV(cdev)->next : NKCTX()->dev_info);
    while (cdev && (VDEV(cdev)->dev_id != did)) {
	cdev = VDEV(cdev)->next;
    }
    return cdev;
}

#ifdef CONFIG_NKERNEL_PRIMARY
    /*
     * Allocates a contiguous memory block from NanoKernel repository
     */
    static NkPhAddr
nk_p_dev_alloc (NkPhSize size)		// primary kernel
{
    return NKCTX()->dev_alloc(NKCTX(), size);
}
#endif

    static NkPhAddr
nk_s_dev_alloc (NkPhSize size)		// secondary kernel
{
    return 0;				// not supported
}

    /*
     * Add a new device to NanoKernel repository.
     * <dev> is a physical address previously returned by
     * dev_alloc(). It must points to a completed NkDevDesc structure.
     */
    static void
nk_dev_add (NkPhAddr dev)
{
    NkDevDesc*    head;
    NkDevDesc*    vdev;
    NkPhAddr      pdev;
    unsigned long flags;

    pdev = NKCTX()->dev_info;

    if (!pdev) {
	printnk("NKDDI: device list is empty!\n");
	nk_panic();
    }

    VDEV(dev)->next = 0;

    head = VDEV(pdev);

    __NK_HARD_LOCK_IRQ_SAVE(&(head->dev_lock), flags);

    do {
	vdev = VDEV(pdev);
        pdev = vdev->next;
    } while (pdev);

    vdev->next = dev;

    __NK_HARD_UNLOCK_IRQ_RESTORE(&(head->dev_lock), flags);

    nk_dev_notification();
}

    /*
     * Get self OS ID
     */
    static NkOsId
nk_id_get (void)
{
    return NKCTX()->id;
}

    /*
     * Get last OS ID (i.e current highest OS ID value)
     */
    static NkOsId
nk_last_id_get (void)
{
    return NKCTX()->lastid;
}

    /*
     * Get a mask of started operating system's ID
     * -> bit <n> is '1' if system with NkOsId <n> is running.
     * -> bit <n> is '0' if system with NkOsId <n> is stopped.
     */
    static NkOsMask
nk_running_ids_get (void)
{
    return *(nkdev.running);
}

    /*
     * Convert a physical address to a virtual one.
     */
    static void*
nk_ptov (NkPhAddr addr)
{
    return PTOV(addr);
}

    /*
     * Convert a virtual address to a physical one.
     */
    static NkPhAddr
nk_vtop (void* addr)
{
    return VTOP(addr);
}

    /*
     * Find first bit set
     */
    static inline nku32_f
_ffs(nku32_f mask)
{
    int n = 31;

    if (mask & 0x0000ffff) { n -= 16;  mask <<= 16; }
    if (mask & 0x00ff0000) { n -=  8;  mask <<=  8; }
    if (mask & 0x0f000000) { n -=  4;  mask <<=  4; }
    if (mask & 0x30000000) { n -=  2;  mask <<=  2; }
    if (mask & 0x40000000) { n -=  1;		    }

    return n;
}

    /*
     * Get next bit of highest priority set in bit mask.
     */
    static nku32_f
nk_bit_get_next (nku32_f mask)
{
    if (mask == 0) {
	printnk("NKDDI: nk_bit_get_next called with zero mask\n");
	nk_panic();
    }

    return _ffs(mask);
}

    /*
     * Insert a bit of given priority into a mask.
     */
    static inline nku32_f
nk_bit_mask (nku32_f bit)
{
    return (1 << bit);
}

    /*
     * Interrupt mask/unmask functions.
     * They are used to implement atomic operations.
     */
    static inline nku32_f
_intr_mask (void)
{
    nku32_f cpsr;
    nku32_f temp;

    __asm__ __volatile__(
	"mrs	%0, cpsr	\n\t"
	"orr	%1, %0, #0x80	\n\t"
	"msr	cpsr_c, %1	\n\t"
	: "=r" (cpsr), "=r" (temp)
    );
    return cpsr;
}

    static inline void
_intr_unmask (nku32_f cpsr)
{
    __asm__ __volatile__(
	"msr	cpsr_c, %0	\n\t"
	:: "r" (cpsr)
    );
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *mask &= ~clear
     * is performed atomically.
     */
    static void
nk_atomic_clear (volatile nku32_f* mask, nku32_f clear)
{
    nku32_f cpsr;

    cpsr = _intr_mask();
    *mask &= ~clear;
    _intr_unmask(cpsr);
}

    /*
     * Atomic operation to clear bits within a bit field.
     *
     * The following logical operation: *mask &= ~clear
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_clear_and_test (volatile nku32_f* mask, nku32_f clear)
{
    nku32_f cpsr;
    nku32_f res;

    cpsr = _intr_mask();
    res = *mask & ~clear;
    *mask = res;
    _intr_unmask(cpsr);

    return res;
}

    /*
     * Atomic operation to set bits within a bit field.
     * The following logical operation: *mask |= set
     * is performed atomically
     */
    static void
nk_atomic_set (volatile nku32_f* mask, nku32_f set)
{
    nku32_f cpsr;

    cpsr = _intr_mask();
    *mask |= set;
    _intr_unmask(cpsr);
}

    /*
     * Atomic operation to substract value to a given memory location.
     * The following logical operation: *ptr -= val
     * is performed atomically.
     */
    static void
nk_atomic_sub (volatile nku32_f* ptr, nku32_f val)
{
    nku32_f cpsr;

    cpsr = _intr_mask();
    *ptr -= val;
    _intr_unmask(cpsr);
}

    /*
     * Atomic operation to substract value to a given memory location.
     * The following logical operation: *ptr -= val
     * is performed atomically.
     *
     * Returns 0 if and only if the result is 0.
     */
    static nku32_f
nk_sub_and_test (volatile nku32_f* ptr, nku32_f val)
{
    nku32_f cpsr;
    nku32_f res;

    cpsr = _intr_mask();
    res = *ptr - val;
    *ptr = res;
    _intr_unmask(cpsr);

    return res;
}

    /*
     * Atomic operation to add value to a given memory location.
     * The following logical operation: *ptr += val
     * is performed atomically
     */
    static void
nk_atomic_add (volatile nku32_f* ptr, nku32_f val)
{
    nku32_f cpsr;

    cpsr = _intr_mask();
    *ptr += val;
    _intr_unmask(cpsr);
}

    /*
     * Physical (shared) memory mapping/unmapping
     *
     * Use one-to-one mapping for shared memory
     *
     */
    static void*
nk_mem_map (NkPhAddr paddr, NkPhSize size)
{
    return PTOV(paddr);
}

    static void
nk_mem_unmap (void* vaddr, NkPhAddr paddr, NkPhSize size)
{
}

    static NkCpuId
nk_cpu_id_get (void)
{
    printnk("NKDDI: nk_cpu_id_get called\n");
    nk_panic();

    return 0;
}

    static void
nk_xvex_post (NkCpuId cpuid, NkOsId osid, NkVex vex)
{
    printnk("PANIC: nk_xvex_post called\n");
    nk_panic();
}

/*
 * Cross IRQs
 */

    /*
     * Allocates required number of contiguous unused cross-interrupts.
     * Returns first interrupt in allocated range.
     */
    static NkXIrq
nk_xirq_alloc (NkXIrq nb)
{
    NkXIrq free;

    spin_lock(&(nkdev.xirq_b_lock));

    free = nkdev.xirq_free;
    if ((free + nb) >= NK_XIRQ_LIMIT) {
	free = 0;
    } else {
        nkdev.xirq_free += nb;
    }

    spin_unlock(&(nkdev.xirq_b_lock));

    return free;
}

    static NkXIrqId
_nk_xirq_attach (NkXIrq        xirq,
  	         NkXIrqHandler hdl,
	         void*         cookie,
		 int           masked)
{
    XIrqCall* call;
    DTRACE("(%x, %x, %x, %x)\n", xirq, hdl, cookie, masked);
        /*
	 * Check for a valid xirq number
	 */
    if (xirq >= nkdev.xirq_free) {
	return 0;
    }

        /*
	 * Allocate and fill in a new descriptor for event handlers
	 */
    call = _xirq_call_alloc();
    if (!call) {
        return 0;
    }

    call->hdl    = (NkXIrqHandler_f) hdl;
    call->cookie = cookie;
    call->xirq   = xirq;
    call->masked = masked;

    spin_lock(&(nkdev.xirq_b_lock));

    call->next               = nkdev.xirq_call[xirq];
    nkdev.xirq_call[xirq]    = call;
    nkdev.xirq_masked[xirq] += masked;

    spin_unlock(&(nkdev.xirq_b_lock));

    return (NkXIrqId)call;
}


    /*
     * Attach a handler to a given NanoKernel cross-interrupt.
     * 0 is returned on failure.
     */
    static NkXIrqId
nk_xirq_attach (NkXIrq        xirq,
  	        NkXIrqHandler hdl,
	        void*         cookie)
{
    return _nk_xirq_attach(xirq, hdl, cookie, 0);
}

    /*
     * Attach a handler to a given NanoKernel cross-interrupt.
     * 0 is returned on failure.
     */
    static NkXIrqId
nk_xirq_attach_masked (NkXIrq        xirq,
  	               NkXIrqHandler hdl,
	               void*         cookie)
{
    return _nk_xirq_attach(xirq, hdl, cookie, 1);
}

    /*
     * Mask a given cross-interrupt
     */
    static void
nk_xirq_mask (NkXIrq xirq)
{
    NkXIrqMask    mask = nk_bit_mask(xirq);
    unsigned long flags;

    spin_lock_irqsave(&nkdev.xirq_i_lock, flags);

    nkdev.xirq_xcount[xirq]++;
    nkdev.xirq_xmask &= ~mask;

    spin_unlock_irqrestore(&nkdev.xirq_i_lock, flags);
}

    void
nk_xirq_unmask (NkXIrq xirq)
{
    unsigned long flags;
    NkXIrqMask    mask = nk_bit_mask(xirq);

    spin_lock_irqsave(&nkdev.xirq_i_lock, flags);
    if (!(--nkdev.xirq_xcount[xirq])) {
        nkdev.xirq_xmask |= mask;
    } else {
	mask = 0;
    }
    spin_unlock_irqrestore(&nkdev.xirq_i_lock, flags);

    if (*nkdev.xpending & mask) {
#ifdef GMv_FIXME
	printnk("PANIC: nk_xirq_unmask called\n");
	nk_panic();
#ifdef CONFIG_NKERNEL_PRIMARY
	__asm__ __volatile__("int %0" :: "i" (NK_IDT_GATE_PXIRQ));
#else
	nk_atomic_set(&(NKCTX()->pending), NK_VEX_XIRQ_MASK);
#endif
#endif
    }
}

    /*
     * Detach a handler (previously connected with irq_attach())
     */
    static void
nk_xirq_detach (NkXIrqId id)
{
    XIrqCall*  call = (XIrqCall*)id;
    XIrqCall** link = &(nkdev.xirq_call[call->xirq]);

    spin_lock(&(nkdev.xirq_b_lock));

    while (*link != call) link = &((*link)->next);
    *link = call->next;

    nkdev.xirq_masked[call->xirq] += call->masked;

    spin_unlock(&(nkdev.xirq_b_lock));

    _xirq_call_free(call);
}

    /*
     * Trigger a cross-interrupt to a given operating system.
     */
    static void
nk_xirq_trigger (NkXIrq xirq, NkOsId osid)
{
    nku32_f cpsr;
    nku32_f mask = nk_bit_mask(xirq);
    nku32_f old;

    cpsr = _intr_mask();

    old = nkdev.xirqs[osid];
    nkdev.xirqs[osid] |= mask;

    _intr_unmask(cpsr);

    if (old == 0) {
	NKCTX()->xpost(NKCTX(), osid);
    }
}

    /*
     * Trigger a local cross-interrupt to a given operating system.
     */
    static void
nk_local_xirq_post (NkXIrq xirq, NkOsId osid)
{
    printnk("NKDDI: nk_local_xirq_post called\n");
    nk_panic();
}

    /*
     * Process pending cross-interrupts to call attached handlers
     * On entry & exit CPU and xirq source are masked.
     */
    inline static void
xirq_call_hdls (NkXIrq xirq)
{
	XIrqCall* call = nkdev.xirq_call[xirq];
    while (call) {
	DTRACE("(%x, %x, %x, %x)\n", call, call->hdl, call->cookie, xirq);
	call->hdl(call->cookie, xirq);
	call = call->next;
    }
}

#ifdef CONFIG_PNX_POWER_TRACE
#include <mach/power_debug.h>
char * pnx_dbg_xirq_name = "XIrq name";
#endif
    unsigned int
nk_do_xirq (int irq, struct pt_regs* regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

    irq_enter();

    spin_lock(&nkdev.xirq_i_lock);

    for (;;) {
	NkXIrq     xirq;
	NkXIrqMask xmask;

	xmask = *nkdev.xpending & nkdev.xirq_xmask;
	if (!xmask) {
	   break;
	}

	xirq = nk_bit_get_next(xmask);
	xmask = nk_bit_mask(xirq);

	nkdev.xirq_xcount[xirq]++;
	nkdev.xirq_xmask &= ~xmask;

	nk_atomic_clear(nkdev.xpending, xmask);

#ifdef CONFIG_PNX_POWER_TRACE
	if (nkdev.xirq_call[xirq])
		pnx_dbg_put_element(pnx_dbg_xirq_name, xirq, nkdev.xirq_call[xirq]->hdl);
	else
		pnx_dbg_put_element(pnx_dbg_xirq_name, xirq, NULL);
#endif

	spin_unlock(&nkdev.xirq_i_lock);

	if (nkdev.xirq_masked[xirq]) {
	    xirq_call_hdls(xirq);
	} else {
	    local_irq_enable();
	    xirq_call_hdls(xirq);
	    local_irq_disable();
	}

	spin_lock(&nkdev.xirq_i_lock);

	if (!(--nkdev.xirq_xcount[xirq])) {
	    nkdev.xirq_xmask |= xmask;
	}
    }

    spin_unlock(&nkdev.xirq_i_lock);

    irq_exit();
	set_irq_regs(old_regs);

    return 1;
}

#ifdef CONFIG_PNX_POWER_TRACE
pnx_dbg_register(nk_do_xirq);
#endif

NkDevOps nkops = {
    NK_VERSION_1,
    nk_dev_lookup_by_class,
    nk_dev_lookup_by_type,
#ifdef CONFIG_NKERNEL_PRIMARY
    nk_p_dev_alloc,
#else
    nk_s_dev_alloc,
#endif
    nk_dev_add,
    nk_id_get,
    nk_last_id_get,
    nk_running_ids_get,
    nk_ptov,
    nk_vtop,
    nk_vex_mask,
    nk_vex_addr,
    nk_xirq_alloc,
    nk_xirq_attach,
    nk_xirq_mask,
    nk_xirq_unmask,
    nk_xirq_detach,
    nk_xirq_trigger,
    nk_bit_get_next,
    nk_bit_mask,
    nk_atomic_clear,
    nk_clear_and_test,
    nk_atomic_set,
    nk_atomic_sub,
    nk_sub_and_test,
    nk_atomic_add,
    nk_mem_map,
    nk_mem_unmap,
    nk_cpu_id_get,
    nk_xvex_post,
    nk_local_xirq_post,
    nk_xirq_attach_masked
};

#ifdef FIXME_GMv

#ifdef CONFIG_X86_REMOTE_DEBUG
extern void breakpoint(void);
    asmlinkage void
nk_cons_ctrl_irq (struct pt_regs regs)
{
    breakpoint();
}

asmlinkage void nk_cons_ctrlc_vector(void);

__asm__(
    "nk_cons_ctrlc_vector:\n\t"
    "	push $0\n\t"
	SAVE_ALL
    "   call nk_cons_ctrl_irq\n\t"
    "	jmp ret_from_intr\n");
#endif /* CONFIG_X86_REMOTE_DEBUG */

#endif

    void __init
nk_ddi_init (void)
{
#ifdef FIXME_GMv
    if (NK_CTX_OFF_PENDING != (int)&(((NkOsCtx*)0)->pending)) {
	printnk("wrong NK_CTX_OFF_PENDING offset\n");
	nk_panic();
    }
    if (NK_CTX_OFF_ENABLED != (int)&(((NkOsCtx*)0)->enabled)) {
	printnk("wrong NK_CTX_OFF_ENABLED offset\n");
	nk_panic();
    }
#endif
    spin_lock_init(&(nkdev.xirq_b_lock));
    spin_lock_init(&(nkdev.xirq_i_lock));

    _xirq_call_init();

    nkdev.xirqs    = ((NkXIrqMask*)PTOV(NKCTX()->xirqs));
    nkdev.xpending = &nkdev.xirqs[NKCTX()->id];

    nkdev.xirq_free  = NK_XIRQ_FREE;
    nkdev.xirq_xmask = 0xffffffff;

    nkdev.running   = (NkOsMask*)PTOV(NKCTX()->running);

#ifdef FIXME_GMv

#ifdef CONFIG_X86_REMOTE_DEBUG
    interrupt[NK_IDT_GATE_DEBUG - FIRST_EXTERNAL_VECTOR] =
        nk_cons_ctrlc_vector;
#endif /* CONFIG_X86_REMOTE_DEBUG */

#endif

}

EXPORT_SYMBOL(nkops);

