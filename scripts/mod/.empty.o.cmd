cmd_scripts/mod/empty.o := /opt/arm-2008q1/bin/arm-none-linux-gnueabi-gcc -Wp,-MD,scripts/mod/.empty.o.d  -nostdinc -isystem /data/linux/opt/arm-2008q1/bin/../lib/gcc/arm-none-linux-gnueabi/4.2.3/include -Iinclude  -I/data/embedded/acer/acergit/linux/arch/arm/include -include include/linux/autoconf.h -D__KERNEL__ -mlittle-endian -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os -marm -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5te -mtune=arm9tdmi -msoft-float -Uarm -fno-stack-protector -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -Wdeclaration-after-statement -Wno-pointer-sign -fwrapv  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(empty)"  -D"KBUILD_MODNAME=KBUILD_STR(empty)"  -c -o scripts/mod/empty.o scripts/mod/empty.c

deps_scripts/mod/empty.o := \
  scripts/mod/empty.c \

scripts/mod/empty.o: $(deps_scripts/mod/empty.o)

$(deps_scripts/mod/empty.o):
