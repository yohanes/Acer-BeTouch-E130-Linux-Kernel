	.arch armv5te
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 4
	.eabi_attribute 18, 4
	.file	"bounds.c"
@ GNU C (Sourcery G++ Lite 2008q1-126) version 4.2.3 (arm-none-linux-gnueabi)
@	compiled by GNU C version 3.4.4.
@ GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
@ options passed:  -nostdinc -Iinclude
@ -I/data/embedded/acer/acergit/linux/arch/arm/include
@ -Iarch/arm/mach-pnx67xx/include -Iarch/arm/plat-pnx/include -iprefix
@ -isysroot -D__KERNEL__ -D__LINUX_ARM_ARCH__=5 -Uarm -DKBUILD_STR(s)=#s
@ -DKBUILD_BASENAME=KBUILD_STR(bounds) -DKBUILD_MODNAME=KBUILD_STR(bounds)
@ -isystem -include -MD -mlittle-endian -marm -mapcs -mno-sched-prolog
@ -mabi=aapcs-linux -mno-thumb-interwork -march=armv5te -mtune=arm9tdmi
@ -msoft-float -auxbase-strip -g -Os -Wall -Wundef -Wstrict-prototypes
@ -Wno-trigraphs -Werror-implicit-function-declaration
@ -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-aliasing
@ -fno-common -fno-stack-protector -fno-omit-frame-pointer
@ -fno-optimize-sibling-calls -fwrapv -fverbose-asm
@ options enabled:  -falign-loops -fargument-alias -fbranch-count-reg
@ -fcaller-saves -fcprop-registers -fcrossjumping -fcse-follow-jumps
@ -fcse-skip-blocks -fdefer-pop -fdelete-null-pointer-checks
@ -fearly-inlining -feliminate-unused-debug-types -fexpensive-optimizations
@ -ffunction-cse -fgcse -fgcse-lm -fguess-branch-probability -fident
@ -fif-conversion -fif-conversion2 -finline-functions
@ -finline-functions-called-once -fipa-pure-const -fipa-reference
@ -fipa-type-escape -fivopts -fkeep-static-consts -fleading-underscore
@ -fmath-errno -fmerge-constants -foptimize-register-move -fpeephole
@ -fpeephole2 -freg-struct-return -fregmove -freorder-functions
@ -frerun-cse-after-loop -fsched-interblock -fsched-spec
@ -fsched-stalled-insns-dep -fschedule-insns -fschedule-insns2
@ -fsection-anchors -fshow-column -fsplit-ivs-in-unroller -fstrict-overflow
@ -fthread-jumps -ftoplevel-reorder -ftrapping-math -ftree-ccp
@ -ftree-copy-prop -ftree-copyrename -ftree-dce -ftree-dominator-opts
@ -ftree-dse -ftree-fre -ftree-loop-im -ftree-loop-ivcanon
@ -ftree-loop-optimize -ftree-lrs -ftree-salias -ftree-sink -ftree-sra
@ -ftree-store-ccp -ftree-store-copy-prop -ftree-ter
@ -ftree-vect-loop-version -ftree-vrp -funit-at-a-time -fvar-tracking
@ -fverbose-asm -fwrapv -fzero-initialized-in-bss -mapcs-frame -mglibc
@ -mlittle-endian

	.section	.debug_abbrev,"",%progbits
.Ldebug_abbrev0:
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.text
.Ltext0:
@ Compiler executable checksum: a22f2111cb7cda723f230433c445c397

	.align	2
	.global	foo
	.type	foo, %function
foo:
.LFB2:
	.file 1 "kernel/bounds.c"
	.loc 1 14 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp	@,
.LCFI0:
	stmfd	sp!, {fp, ip, lr, pc}	@,
.LCFI1:
	sub	fp, ip, #4	@,,
.LCFI2:
	.loc 1 16 0
#APP
	
->NR_PAGEFLAGS #22 __NR_PAGEFLAGS	@
	.loc 1 17 0
	
->MAX_NR_ZONES #2 __MAX_NR_ZONES	@
	.loc 1 19 0
	ldmfd	sp, {fp, sp, pc}	@
.LFE2:
	.size	foo, .-foo
	.section	.debug_frame,"",%progbits
.Lframe0:
	.4byte	.LECIE0-.LSCIE0
.LSCIE0:
	.4byte	0xffffffff
	.byte	0x1
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 -4
	.byte	0xe
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x0
	.align	2
.LECIE0:
.LSFDE0:
	.4byte	.LEFDE0-.LASFDE0
.LASFDE0:
	.4byte	.Lframe0
	.4byte	.LFB2
	.4byte	.LFE2-.LFB2
	.byte	0x4
	.4byte	.LCFI0-.LFB2
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI2-.LCFI1
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE0:
	.text
.Letext0:
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LFB2-.Ltext0
	.4byte	.LCFI0-.Ltext0
	.2byte	0x1
	.byte	0x5d
	.4byte	.LCFI0-.Ltext0
	.4byte	.LCFI2-.Ltext0
	.2byte	0x1
	.byte	0x5c
	.4byte	.LCFI2-.Ltext0
	.4byte	.LFE2-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0x0
	.4byte	0x0
	.file 2 "include/linux/page-flags.h"
	.file 3 "include/linux/mmzone.h"
	.section	.debug_info
	.4byte	0x16e
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF46
	.byte	0x1
	.4byte	.LASF47
	.4byte	.LASF48
	.4byte	.Ltext0
	.4byte	.Letext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF1
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF2
	.uleb128 0x3
	.byte	0x4
	.byte	0x7
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF6
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF7
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF10
	.uleb128 0x5
	.4byte	.LASF41
	.byte	0x4
	.byte	0x2
	.byte	0x48
	.4byte	0x13d
	.uleb128 0x6
	.4byte	.LASF11
	.sleb128 0
	.uleb128 0x6
	.4byte	.LASF12
	.sleb128 1
	.uleb128 0x6
	.4byte	.LASF13
	.sleb128 2
	.uleb128 0x6
	.4byte	.LASF14
	.sleb128 3
	.uleb128 0x6
	.4byte	.LASF15
	.sleb128 4
	.uleb128 0x6
	.4byte	.LASF16
	.sleb128 5
	.uleb128 0x6
	.4byte	.LASF17
	.sleb128 6
	.uleb128 0x6
	.4byte	.LASF18
	.sleb128 7
	.uleb128 0x6
	.4byte	.LASF19
	.sleb128 8
	.uleb128 0x6
	.4byte	.LASF20
	.sleb128 9
	.uleb128 0x6
	.4byte	.LASF21
	.sleb128 10
	.uleb128 0x6
	.4byte	.LASF22
	.sleb128 11
	.uleb128 0x6
	.4byte	.LASF23
	.sleb128 12
	.uleb128 0x6
	.4byte	.LASF24
	.sleb128 13
	.uleb128 0x6
	.4byte	.LASF25
	.sleb128 14
	.uleb128 0x6
	.4byte	.LASF26
	.sleb128 15
	.uleb128 0x6
	.4byte	.LASF27
	.sleb128 16
	.uleb128 0x6
	.4byte	.LASF28
	.sleb128 17
	.uleb128 0x6
	.4byte	.LASF29
	.sleb128 18
	.uleb128 0x6
	.4byte	.LASF30
	.sleb128 19
	.uleb128 0x6
	.4byte	.LASF31
	.sleb128 20
	.uleb128 0x6
	.4byte	.LASF32
	.sleb128 21
	.uleb128 0x6
	.4byte	.LASF33
	.sleb128 22
	.uleb128 0x6
	.4byte	.LASF34
	.sleb128 8
	.uleb128 0x6
	.4byte	.LASF35
	.sleb128 8
	.uleb128 0x6
	.4byte	.LASF36
	.sleb128 4
	.uleb128 0x6
	.4byte	.LASF37
	.sleb128 6
	.uleb128 0x6
	.4byte	.LASF38
	.sleb128 11
	.uleb128 0x6
	.4byte	.LASF39
	.sleb128 6
	.uleb128 0x6
	.4byte	.LASF40
	.sleb128 1
	.byte	0x0
	.uleb128 0x5
	.4byte	.LASF42
	.byte	0x4
	.byte	0x3
	.byte	0xc3
	.4byte	0x15c
	.uleb128 0x6
	.4byte	.LASF43
	.sleb128 0
	.uleb128 0x6
	.4byte	.LASF44
	.sleb128 1
	.uleb128 0x6
	.4byte	.LASF45
	.sleb128 2
	.byte	0x0
	.uleb128 0x7
	.byte	0x1
	.ascii	"foo\000"
	.byte	0x1
	.byte	0xe
	.byte	0x1
	.4byte	.LFB2
	.4byte	.LFE2
	.4byte	.LLST0
	.byte	0x0
	.section	.debug_abbrev
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x6
	.byte	0x0
	.byte	0x0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0x0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0x0
	.byte	0x0
	.uleb128 0x3
	.uleb128 0x24
	.byte	0x0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0x0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0x0
	.byte	0x0
	.uleb128 0x5
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.uleb128 0x6
	.uleb128 0x28
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0x0
	.byte	0x0
	.uleb128 0x7
	.uleb128 0x2e
	.byte	0x0
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.section	.debug_pubnames,"",%progbits
	.4byte	0x16
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x172
	.4byte	0x15c
	.ascii	"foo\000"
	.4byte	0x0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0x0
	.2byte	0x0
	.2byte	0x0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0x0
	.4byte	0x0
	.section	.debug_str,"MS",%progbits,1
.LASF27:
	.ascii	"PG_mappedtodisk\000"
.LASF48:
	.ascii	"/data/embedded/acer/acergit/linux\000"
.LASF22:
	.ascii	"PG_private\000"
.LASF8:
	.ascii	"short int\000"
.LASF35:
	.ascii	"PG_pinned\000"
.LASF46:
	.ascii	"GNU C 4.2.3\000"
.LASF28:
	.ascii	"PG_reclaim\000"
.LASF39:
	.ascii	"PG_slub_frozen\000"
.LASF21:
	.ascii	"PG_reserved\000"
.LASF11:
	.ascii	"PG_locked\000"
.LASF33:
	.ascii	"__NR_PAGEFLAGS\000"
.LASF30:
	.ascii	"PG_swapbacked\000"
.LASF29:
	.ascii	"PG_buddy\000"
.LASF5:
	.ascii	"long long int\000"
.LASF44:
	.ascii	"ZONE_MOVABLE\000"
.LASF4:
	.ascii	"long int\000"
.LASF45:
	.ascii	"__MAX_NR_ZONES\000"
.LASF43:
	.ascii	"ZONE_NORMAL\000"
.LASF20:
	.ascii	"PG_arch_1\000"
.LASF19:
	.ascii	"PG_owner_priv_1\000"
.LASF7:
	.ascii	"unsigned char\000"
.LASF14:
	.ascii	"PG_uptodate\000"
.LASF6:
	.ascii	"signed char\000"
.LASF9:
	.ascii	"long long unsigned int\000"
.LASF25:
	.ascii	"PG_tail\000"
.LASF38:
	.ascii	"PG_slob_free\000"
.LASF2:
	.ascii	"unsigned int\000"
.LASF13:
	.ascii	"PG_referenced\000"
.LASF15:
	.ascii	"PG_dirty\000"
.LASF40:
	.ascii	"PG_slub_debug\000"
.LASF3:
	.ascii	"short unsigned int\000"
.LASF34:
	.ascii	"PG_checked\000"
.LASF37:
	.ascii	"PG_slob_page\000"
.LASF1:
	.ascii	"char\000"
.LASF41:
	.ascii	"pageflags\000"
.LASF10:
	.ascii	"_Bool\000"
.LASF16:
	.ascii	"PG_lru\000"
.LASF31:
	.ascii	"PG_unevictable\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF26:
	.ascii	"PG_swapcache\000"
.LASF17:
	.ascii	"PG_active\000"
.LASF24:
	.ascii	"PG_head\000"
.LASF47:
	.ascii	"kernel/bounds.c\000"
.LASF42:
	.ascii	"zone_type\000"
.LASF18:
	.ascii	"PG_slab\000"
.LASF32:
	.ascii	"PG_mlocked\000"
.LASF12:
	.ascii	"PG_error\000"
.LASF23:
	.ascii	"PG_writeback\000"
.LASF36:
	.ascii	"PG_savepinned\000"
	.ident	"GCC: (Sourcery G++ Lite 2008q1-126) 4.2.3"
	.section	.note.GNU-stack,"",%progbits
