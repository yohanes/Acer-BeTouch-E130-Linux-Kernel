#
#***************************************************************
#
# Component = Nano-Kernel device driver interface (NK DDI)
#
# Copyright (C) 2002-2005 Jaluna SA.
#
# This program is free software;  you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# #ident  "@(#)Makefile 1.17     05/12/05 Jaluna"
#
# Contributor(s):
#	Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
#	Guennadi Maslov <guennadi.maslov@jaluna.com>
#	Chi Dat Truong <chidat.truong@jaluna.com>
#
#***************************************************************
#

# Each configuration option enables a list of files.

obj-y			:= ddi.o # vtick.o
obj-$(CONFIG_NKERNEL_CONSOLE)	+= console.o
obj-$(CONFIG_PROC_FS)	+= proc.o
obj-$(CONFIG_XOSCORE)	+= xos_core.o xos_ctrl.o xos_fifo.o xos_lock.o xos_area.o
obj-$(CONFIG_XOSCORE)	+= vreg.o
obj-$(CONFIG_VMON) 		+= vmon.o
obj-$(CONFIG_NET_9P_XOSCORE)	+= pmu9p.o  adcclient.o console9p.o
obj-$(CONFIG_VMUX) 		+= vmux.o

vmon-objs := vmon_kthread.o vmon_pipe.o vmon_fifo.o

