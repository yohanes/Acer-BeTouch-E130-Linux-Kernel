#ifndef __ASM_ARM_IRQFLAGS_H
#define __ASM_ARM_IRQFLAGS_H

#ifdef __KERNEL__

#include <asm/ptrace.h>

/*
 * CPU interrupt mask handling.
 */
#if __LINUX_ARM_ARCH__ >= 6

#define hw_raw_local_irq_save(x)					\
	({							\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_irq_save\n"	\
	"cpsid	i"						\
	: "=r" (x) : : "memory", "cc");				\
	})

#define hw_raw_local_irq_enable()  __asm__("cpsie i	@ __sti" : : : "memory", "cc")
#define hw_raw_local_irq_disable() __asm__("cpsid i	@ __cli" : : : "memory", "cc")
#define hw_local_fiq_enable()  __asm__("cpsie f	@ __stf" : : : "memory", "cc")
#define hw_local_fiq_disable() __asm__("cpsid f	@ __clf" : : : "memory", "cc")

#else

/*
 * Save the current interrupt enable state & disable IRQs
 */
#define hw_raw_local_irq_save(x)					\
	({							\
		unsigned long temp;				\
		(void) (&temp == &x);				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_irq_save\n"	\
"	orr	%1, %0, #128\n"					\
"	msr	cpsr_c, %1"					\
	: "=r" (x), "=r" (temp)					\
	:							\
	: "memory", "cc");					\
	})
	
/*
 * Enable IRQs
 */
#define hw_raw_local_irq_enable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_irq_enable\n"	\
"	bic	%0, %0, #128\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Disable IRQs
 */
#define hw_raw_local_irq_disable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_irq_disable\n"	\
"	orr	%0, %0, #128\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Enable FIQs
 */
#define hw_local_fiq_enable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ stf\n"		\
"	bic	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

/*
 * Disable FIQs
 */
#define hw_local_fiq_disable()					\
	({							\
		unsigned long temp;				\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ clf\n"		\
"	orr	%0, %0, #64\n"					\
"	msr	cpsr_c, %0"					\
	: "=r" (temp)						\
	:							\
	: "memory", "cc");					\
	})

#endif

/*
 * Save the current interrupt enable state.
 */
#define hw_raw_local_save_flags(x)					\
	({							\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_save_flags"	\
	: "=r" (x) : : "memory", "cc");				\
	})

/*
 * restore saved IRQ & FIQ state
 */
#define hw_raw_local_irq_restore(x)				\
	__asm__ __volatile__(					\
	"msr	cpsr_c, %0		@ local_irq_restore\n"	\
	:							\
	: "r" (x)						\
	: "memory", "cc")

#define hw_raw_irqs_disabled_flags(flags)	\
({					\
	(int)((flags) & PSR_I_BIT);	\
})

/*
#define hw_irqs_disabled()			\
({					\
	unsigned long flags;		\
	hw_local_save_flags(flags);	\
	(int)(flags & PSR_I_BIT);	\
})
*/

#ifdef CONFIG_NKERNEL

unsigned int _irq_save(void);
unsigned int _irq_set(void);
unsigned int _save_flags(void);
unsigned int _irq_restore(unsigned int);
unsigned int _irq_pending(void);

#define raw_local_irq_save(X)   (X = _irq_save())

#define raw_local_irq_enable()  (void) _irq_set()
#define raw_local_irq_disable() (void) _irq_save()

#define raw_local_save_flags(X) (X  = _save_flags())
#define raw_local_irq_restore(X) _irq_restore(X)
#define raw_local_irq_pending()  _irq_pending()


#define raw_irqs_disabled_flags(flags)			\
 ({					\
 /*	unsigned long flags;*/		\
 	/*local_save_flags(flags);*/	\
	!(flags & __VEX_IRQ_FLAG);	\
 })


#define local_fiq_enable()	hw_local_fiq_enable()
#define local_fiq_disable()	hw_local_fiq_disable()

#else

#define raw_local_irq_save(x)	hw_raw_local_irq_save(x)
#define raw_local_irq_set(x)	hw_local_irq_set(x)
#define raw_local_irq_enable()	hw_raw_local_irq_enable()
#define local_fiq_enable()	    hw_local_fiq_enable()
#define local_fiq_disable()	    hw_local_fiq_disable()
#define raw_local_irq_disable()	hw_raw_local_irq_disable()
#define raw_local_save_flags(x)	hw_raw_local_save_flags(x)
#define raw_local_irq_restore(x)	hw_raw_local_irq_restore(x)
#define raw_irqs_disabled_flags(x) hw_raw_irqs_disabled_flags(x)
//#define irqs_disabled()		hw_irqs_disabled()

#endif /* CONFIG_NKERNEL */



#endif
#endif
