cmd_arch/arm/lib/csumpartialcopyuser.o := /opt/arm-2008q1/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,arch/arm/lib/.csumpartialcopyuser.o.d  -nostdinc -isystem /data/linux/opt/arm-2008q1/bin/../lib/gcc/arm-none-linux-gnueabi/4.2.3/include -Iinclude  -I/data/embedded/acer/acergit/linux/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -gdwarf-2     -c -o arch/arm/lib/csumpartialcopyuser.o arch/arm/lib/csumpartialcopyuser.S

deps_arch/arm/lib/csumpartialcopyuser.o := \
  arch/arm/lib/csumpartialcopyuser.S \
    $(wildcard include/config/cpu.h) \
  include/linux/linkage.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/enable/warn/deprecated.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/linkage.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/assembler.h \
    $(wildcard include/config/cpu/feroceon.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/nkernel.h) \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/smp.h) \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/hwcap.h \
  /data/embedded/acer/acergit/linux/arch/arm/include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \
  include/asm/asm-offsets.h \
  arch/arm/lib/csumpartialcopygeneric.S \

arch/arm/lib/csumpartialcopyuser.o: $(deps_arch/arm/lib/csumpartialcopyuser.o)

$(deps_arch/arm/lib/csumpartialcopyuser.o):
