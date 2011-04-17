/*
 * Copyright (C) 1995-2003 Russell King
 *               2001-2002 Keith Owens
 *
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 *
 * Adaptation to Jaluna OSware
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/mach/arch.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
#include <asm/procinfo.h>
#include <linux/kbuild.h>

#ifdef CONFIG_NKERNEL
#include <asm/nk/f_nk.h>
#endif

/*
 * Make sure that the compiler and target are compatible.
 */
#if defined(__APCS_26__)
#error Sorry, your compiler targets APCS-26 but this kernel requires APCS-32
#endif
/*
 * GCC 3.0, 3.1: general bad code generation.
 * GCC 3.2.0: incorrect function argument offset calculation.
 * GCC 3.2.x: miscompiles NEW_AUX_ENT in fs/binfmt_elf.c
 *            (http://gcc.gnu.org/PR8896) and incorrect structure
 *	      initialisation in fs/jffs2/erase.c
 */
#if (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#error Your compiler is too buggy; it is known to miscompile kernels.
#error    Known good compilers: 3.3
#endif

int main(void)
{
  DEFINE(TSK_ACTIVE_MM,		offsetof(struct task_struct, active_mm));
  BLANK();
  DEFINE(TI_FLAGS,		offsetof(struct thread_info, flags));
  DEFINE(TI_PREEMPT,		offsetof(struct thread_info, preempt_count));
  DEFINE(TI_ADDR_LIMIT,		offsetof(struct thread_info, addr_limit));
  DEFINE(TI_TASK,		offsetof(struct thread_info, task));
  DEFINE(TI_EXEC_DOMAIN,	offsetof(struct thread_info, exec_domain));
  DEFINE(TI_CPU,		offsetof(struct thread_info, cpu));
  DEFINE(TI_CPU_DOMAIN,		offsetof(struct thread_info, cpu_domain));
  DEFINE(TI_CPU_SAVE,		offsetof(struct thread_info, cpu_context));
  DEFINE(TI_USED_CP,		offsetof(struct thread_info, used_cp));
  DEFINE(TI_TP_VALUE,		offsetof(struct thread_info, tp_value));
  DEFINE(TI_FPSTATE,		offsetof(struct thread_info, fpstate));
  DEFINE(TI_VFPSTATE,		offsetof(struct thread_info, vfpstate));
#ifdef CONFIG_ARM_THUMBEE
  DEFINE(TI_THUMBEE_STATE,	offsetof(struct thread_info, thumbee_state));
#endif
#ifdef CONFIG_IWMMXT
  DEFINE(TI_IWMMXT_STATE,	offsetof(struct thread_info, fpstate.iwmmxt));
#endif
#ifdef CONFIG_CRUNCH
  DEFINE(TI_CRUNCH_STATE,	offsetof(struct thread_info, crunchstate));
#endif
  BLANK();
  DEFINE(S_R0,			offsetof(struct pt_regs, ARM_r0));
  DEFINE(S_R1,			offsetof(struct pt_regs, ARM_r1));
  DEFINE(S_R2,			offsetof(struct pt_regs, ARM_r2));
  DEFINE(S_R3,			offsetof(struct pt_regs, ARM_r3));
  DEFINE(S_R4,			offsetof(struct pt_regs, ARM_r4));
  DEFINE(S_R5,			offsetof(struct pt_regs, ARM_r5));
  DEFINE(S_R6,			offsetof(struct pt_regs, ARM_r6));
  DEFINE(S_R7,			offsetof(struct pt_regs, ARM_r7));
  DEFINE(S_R8,			offsetof(struct pt_regs, ARM_r8));
  DEFINE(S_R9,			offsetof(struct pt_regs, ARM_r9));
  DEFINE(S_R10,			offsetof(struct pt_regs, ARM_r10));
  DEFINE(S_FP,			offsetof(struct pt_regs, ARM_fp));
  DEFINE(S_IP,			offsetof(struct pt_regs, ARM_ip));
  DEFINE(S_SP,			offsetof(struct pt_regs, ARM_sp));
  DEFINE(S_LR,			offsetof(struct pt_regs, ARM_lr));
  DEFINE(S_PC,			offsetof(struct pt_regs, ARM_pc));
  DEFINE(S_PSR,			offsetof(struct pt_regs, ARM_cpsr));
  DEFINE(S_OLD_R0,		offsetof(struct pt_regs, ARM_ORIG_r0));
  DEFINE(S_FRAME_SIZE,		sizeof(struct pt_regs));
  BLANK();
#ifdef CONFIG_CPU_HAS_ASID
  DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, context.id));
  BLANK();
#endif
  DEFINE(VMA_VM_MM,		offsetof(struct vm_area_struct, vm_mm));
  DEFINE(VMA_VM_FLAGS,		offsetof(struct vm_area_struct, vm_flags));
  BLANK();
  DEFINE(VM_EXEC,	       	VM_EXEC);
  BLANK();
  DEFINE(PAGE_SZ,	       	PAGE_SIZE);
  BLANK();
  DEFINE(SYS_ERROR0,		0x9f0000);
  BLANK();
  DEFINE(SIZEOF_MACHINE_DESC,	sizeof(struct machine_desc));
  DEFINE(MACHINFO_TYPE,		offsetof(struct machine_desc, nr));
  DEFINE(MACHINFO_NAME,		offsetof(struct machine_desc, name));
  DEFINE(MACHINFO_PHYSIO,	offsetof(struct machine_desc, phys_io));
  DEFINE(MACHINFO_PGOFFIO,	offsetof(struct machine_desc, io_pg_offst));
  BLANK();
  DEFINE(PROC_INFO_SZ,		sizeof(struct proc_info_list));
  DEFINE(PROCINFO_INITFUNC,	offsetof(struct proc_info_list, __cpu_flush));
  DEFINE(PROCINFO_MM_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
  DEFINE(PROCINFO_IO_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_io_mmu_flags));
  BLANK();
#ifdef MULTI_DABORT
  DEFINE(PROCESSOR_DABT_FUNC,	offsetof(struct processor, _data_abort));
#endif
#ifdef MULTI_PABORT
  DEFINE(PROCESSOR_PABT_FUNC,	offsetof(struct processor, _prefetch_abort));
#endif
#ifdef CONFIG_NKERNEL
  DEFINE(ctx_sizeof,            sizeof(NkOsCtx));
  DEFINE(ctx_swi_off,           offsetof(NkOsCtx, os_vectors[2]));
  DEFINE(ctx_irq_off,           offsetof(NkOsCtx, os_vectors[6]));
  DEFINE(ctx_iirq_off,          offsetof(NkOsCtx, os_vectors[10]));
  DEFINE(ctx_xirq_off,          offsetof(NkOsCtx, os_vectors[9]));
  DEFINE(ctx_pending_off,       offsetof(NkOsCtx, pending));
  DEFINE(ctx_enabled_off,       offsetof(NkOsCtx, enabled));
  DEFINE(ctx_sp_svc_off,        offsetof(NkOsCtx, sp_svc));
  DEFINE(ctx_root_dir_off,      offsetof(NkOsCtx, root_dir));
  DEFINE(ctx_domain_off,        offsetof(NkOsCtx, domain));
  DEFINE(ctx_arch_id_off,       offsetof(NkOsCtx, arch_id));
  DEFINE(ctx_vil_off,           offsetof(NkOsCtx, vil));
  DEFINE(ctx_idle_off,          offsetof(NkOsCtx, idle));
  DEFINE(ctx_prio_set_off,      offsetof(NkOsCtx, prio_set));
  DEFINE(ctx_regs_r0_off,       offsetof(NkOsCtx, regs[0]));
  DEFINE(ctx_regs_r4_off,       offsetof(NkOsCtx, regs[4]));
  DEFINE(ctx_regs_r10_off,      offsetof(NkOsCtx, regs[10]));
  DEFINE(ctx_regs_r12_off,      offsetof(NkOsCtx, regs[12]));
  DEFINE(ctx_regs_sp_off,       offsetof(NkOsCtx, regs[13]));
  DEFINE(ctx_regs_lr_off,       offsetof(NkOsCtx, regs[14]));
  DEFINE(ctx_regs_pc_off,       offsetof(NkOsCtx, regs[15]));
  DEFINE(ctx_regs_cpsr_off,     offsetof(NkOsCtx, regs[16]));
  DEFINE(ctx_nkvector_off,      offsetof(NkOsCtx, nkvector));
#endif
  return 0; 
}
