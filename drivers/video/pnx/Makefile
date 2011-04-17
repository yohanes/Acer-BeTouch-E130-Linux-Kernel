#
# Makefile for the framebuffer drivers
# based on the ST-Ericsson platform
#

obj-$(CONFIG_FB_LCDBUS_LOGIC_PAGEFAULT) += lcdbus_pagefaultfb.o lcdbus_debug_proc.o

obj-y +=  pnxfbpower.o busses/ displays/

ifeq ($(CONFIG_FB_LCDBUS_PAGEFAULT_KERNEL_SPLASH_SCREEN),y)
	obj-y += splash/
endif

ifeq ($(CONFIG_FB_LCDBUS_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

