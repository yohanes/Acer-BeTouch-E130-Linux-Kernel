cmd_lib/reciprocal_div.o := /opt/arm-2008q1/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,lib/.reciprocal_div.o.d  -nostdinc -isystem /data/linux/opt/arm-2008q1/bin/../lib/gcc/arm-none-linux-gnueabi/4.2.3/include -Iinclude  -I/data/embedded/acer/acergit/linux/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os -marm -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -Uarm -fno-stack-protector -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -Wdeclaration-after-statement -Wno-pointer-sign -fwrapv  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(reciprocal_div)"  -D"KBUILD_MODNAME=KBUILD_STR(reciprocal_div)"  -c -o lib/reciprocal_div.o lib/reciprocal_div.c

deps_lib/reciprocal_div.o := \
  lib/reciprocal_div.c \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/div64.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/system.h \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/sa110.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/memory.h \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/page/offset.h) \
    $(wildcard include/config/dram/size.h) \
    $(wildcard include/config/dram/base.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler-gcc4.h \
  include/linux/const.h \
  arch/arm/plat-pnx/include/mach/memory.h \
    $(wildcard include/config/nkernel.h) \
    $(wildcard include/config/mach/pnx/realloc.h) \
    $(wildcard include/config/android/pmem.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/sizes.h \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
  include/linux/linkage.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/linkage.h \
  include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
    $(wildcard include/config/x86.h) \
  include/linux/typecheck.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/irqflags.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/hwcap.h \
  include/asm/nkern.h \
    $(wildcard include/config/nkernel/console.h) \
  include/asm/nk/f_nk.h \
  include/asm/nk/nk_f.h \
  include/asm-generic/cmpxchg-local.h \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/lbd.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/posix_types.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/types.h \
  include/asm-generic/int-ll64.h \
  include/asm-generic/cmpxchg.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/debug/bugverbose.h) \
  include/asm-generic/bug.h \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  include/linux/reciprocal_div.h \

lib/reciprocal_div.o: $(deps_lib/reciprocal_div.o)

$(deps_lib/reciprocal_div.o):
