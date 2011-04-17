/* ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 * Adaptation to Jaluna OSware
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 */

#include <asm-generic/vmlinux.lds.h>
#include <linux/autoconf.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
	
OUTPUT_ARCH(arm)
ENTRY(stext)

#ifndef __ARMEB__
jiffies = jiffies_64;
#else
jiffies = jiffies_64 + 4;
#endif

SECTIONS
{
#ifdef CONFIG_NKERNEL
#ifndef CONFIG_NKERNEL_RELOC
	. = CONFIG_NKERNEL_START;
#endif
	.filler : { *(.filler) }
#else
#ifdef CONFIG_XIP_KERNEL
	. = XIP_VIRT_ADDR(CONFIG_XIP_PHYS_ADDR);
#else
	. = PAGE_OFFSET + TEXT_OFFSET;
#endif
#endif
	.text.head : {
		_stext = .;
		_sinittext = .;
		*(.text.head)
	}

	.init : {			/* Init code and data		*/
			INIT_TEXT
		_einittext = .;
		__proc_info_begin = .;
			*(.proc.info.init)
		__proc_info_end = .;
		__arch_info_begin = .;
			*(.arch.info.init)
		__arch_info_end = .;
		__tagtable_begin = .;
			*(.taglist.init)
		__tagtable_end = .;
		. = ALIGN(16);
		__setup_start = .;
			*(.init.setup)
		__setup_end = .;
		__early_begin = .;
			*(.early_param.init)
		__early_end = .;
		__initcall_start = .;
			INITCALLS
		__initcall_end = .;
		__con_initcall_start = .;
			*(.con_initcall.init)
		__con_initcall_end = .;
		__security_initcall_start = .;
			*(.security_initcall.init)
		__security_initcall_end = .;
#ifdef CONFIG_BLK_DEV_INITRD
		. = ALIGN(32);
		__initramfs_start = .;
			usr/built-in.o(.init.ramfs)
		__initramfs_end = .;
#endif
		. = ALIGN(4096);
		__per_cpu_start = .;
			*(.data.percpu)
			*(.data.percpu.shared_aligned)
		__per_cpu_end = .;
#ifndef CONFIG_XIP_KERNEL
		__init_begin = _stext;
		INIT_DATA
		. = ALIGN(4096);
		__init_end = .;
#endif
	}

	/DISCARD/ : {			/* Exit code and data		*/
		EXIT_TEXT
		EXIT_DATA
		*(.exitcall.exit)
#ifndef CONFIG_MMU
		*(.fixup)
		*(__ex_table)
#endif
	}

	.text : {			/* Real text segment		*/
		_text = .;		/* Text and read-only data	*/
			__exception_text_start = .;
			*(.exception.text)
			__exception_text_end = .;
			TEXT_TEXT
			SCHED_TEXT
			LOCK_TEXT
			KPROBES_TEXT
#ifdef CONFIG_MMU
			*(.fixup)
#endif
			*(.gnu.warning)
			*(.rodata)
			*(.rodata.*)
			*(.glue_7)
			*(.glue_7t)
		*(.got)			/* Global offset table		*/
	}

#ifdef CONFIG_PNX_POWER_TRACE
	.pnx_dbg_reg : {
		__pnx_dbg_reg_start = .;
		*(.pnx_dbg_reg)
		__pnx_dbg_reg_end = .;
	}
#endif

	RODATA

#ifdef CONFIG_NKERNEL
	.text_data_barrier : {
		_etext = .;
		*(.text_data_barrier)
		__data_loc = .;
	}

	.data : AT(__data_loc) {
		_data = .;		/* address in memory */
		_sdata = .;     /* added for kmemleak */
#else

	_etext = .;			/* End of text and rodata section */

#ifdef CONFIG_XIP_KERNEL
	__data_loc = ALIGN(4);		/* location in binary */
	. = PAGE_OFFSET + TEXT_OFFSET;
#else
	. = ALIGN(THREAD_SIZE);
	__data_loc = .;
#endif

	.data : AT(__data_loc) {
		_data = .;		/* address in memory */

		/*
		 * first, the init task union, aligned
		 * to an 8192 byte boundary.
		 */
#endif
		*(.data.init_task)

#ifdef CONFIG_XIP_KERNEL
		. = ALIGN(4096);
		__init_begin = .;
		INIT_DATA
		. = ALIGN(4096);
		__init_end = .;
#endif

		. = ALIGN(4096);
		__nosave_begin = .;
		*(.data.nosave)
		. = ALIGN(4096);
		__nosave_end = .;

		/*
		 * then the cacheline aligned data
		 */
#ifndef CONFIG_NKERNEL
		. = ALIGN(32);
#endif
		*(.data.cacheline_aligned)

		/*
		 * The exception fixup table (might need resorting at runtime)
		 */
		. = ALIGN(32);
		__start___ex_table = .;
#ifdef CONFIG_MMU
		*(__ex_table)
#endif
		__stop___ex_table = .;

		/*
		 * and the usual data section
		 */
		DATA_DATA
		CONSTRUCTORS

		_edata = .;
	}
	_edata_loc = __data_loc + SIZEOF(.data);

	.bss : {
		__bss_start = .;	/* BSS				*/
		*(.bss)
		*(COMMON)
		_end = .;
	}
					/* Stabs debugging sections.	*/
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
}

/*
 * These must never be empty
 * If you have to comment these two assert statements out, your
 * binutils is too old (for other reasons as well)
 * OK this does not seems to work currently don't know why - Jaluna
ASSERT((__proc_info_end - __proc_info_begin), "missing CPU support")
ASSERT((__arch_info_end - __arch_info_begin), "no machine record defined")
*/
