cmd_arch/arm/kernel/entry-common.o := /opt/arm-2008q1/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,arch/arm/kernel/.entry-common.o.d  -nostdinc -isystem /data/linux/opt/arm-2008q1/bin/../lib/gcc/arm-none-linux-gnueabi/4.2.3/include -Iinclude  -I/data/embedded/acer/acergit/linux/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -gdwarf-2     -c -o arch/arm/kernel/entry-common.o arch/arm/kernel/entry-common.S

deps_arch/arm/kernel/entry-common.o := \
  arch/arm/kernel/entry-common.S \
    $(wildcard include/config/nkernel.h) \
    $(wildcard include/config/xip/kernel.h) \
    $(wildcard include/config/function/tracer.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/cpu/arm710.h) \
    $(wildcard include/config/oabi/compat.h) \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/aeabi.h) \
    $(wildcard include/config/alignment/trap.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/unistd.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/ftrace.h \
  arch/arm/plat-pnx/include/mach/entry-macro.S \
  arch/arm/plat-pnx/include/mach/hardware.h \
    $(wildcard include/config/mach/pnx/realloc.h) \
    $(wildcard include/config/vaddr.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/sizes.h \
  arch/arm/plat-pnx/include/mach/regs-pnx67xx.h \
    $(wildcard include/config/offset.h) \
    $(wildcard include/config/reg.h) \
    $(wildcard include/config/value.h) \
  arch/arm/kernel/entry-header.S \
    $(wildcard include/config/arch/xxxx.h) \
    $(wildcard include/config/frame/pointer.h) \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  include/linux/linkage.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/linkage.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/smp.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/hwcap.h \
  include/asm/asm-offsets.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/thread_info.h \
    $(wildcard include/config/arm/thumbee.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/fpstate.h \
    $(wildcard include/config/vfpv3.h) \
    $(wildcard include/config/iwmmxt.h) \
  include/asm/nkern.h \
    $(wildcard include/config/nkernel/console.h) \
  include/asm/nk/f_nk.h \
  include/asm/nk/nk_f.h \
  arch/arm/kernel/calls.S \

arch/arm/kernel/entry-common.o: $(deps_arch/arm/kernel/entry-common.o)

$(deps_arch/arm/kernel/entry-common.o):
