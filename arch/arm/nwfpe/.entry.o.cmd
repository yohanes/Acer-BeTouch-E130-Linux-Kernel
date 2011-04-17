cmd_arch/arm/nwfpe/entry.o := /opt/arm-2008q1/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,arch/arm/nwfpe/.entry.o.d  -nostdinc -isystem /data/linux/opt/arm-2008q1/bin/../lib/gcc/arm-none-linux-gnueabi/4.2.3/include -Iinclude  -I/data/embedded/acer/acergit/linux/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -gdwarf-2     -c -o arch/arm/nwfpe/entry.o arch/arm/nwfpe/entry.S

deps_arch/arm/nwfpe/entry.o := \
  arch/arm/nwfpe/entry.S \
  include/asm/asm-offsets.h \

arch/arm/nwfpe/entry.o: $(deps_arch/arm/nwfpe/entry.o)

$(deps_arch/arm/nwfpe/entry.o):
