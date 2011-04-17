#
# Makefile for the linux kernel, harware independant part of STE PNX.
#

# Common support
obj-y := hwmem.o irq.o dma.o gpio.o io.o serial.o pnx_sysfs.o pctooldetection.o process_pnx.o version.o update-flash.o
obj-m :=
obj-n :=
obj-  :=

#obj-$(CONFIG_NKERNEL) += bootmode.o
obj-$(CONFIG_PNX_POWER_SYSFS) += power_sysfs.o
obj-$(CONFIG_PNX_POWER_TRACE)+= power_debug.o
obj-$(CONFIG_PNX_MTU_TIMER) += timer_mtu.o
obj-$(CONFIG_PNX_SCTU_TIMER) += timer_sctu.o
obj-$(CONFIG_PNX_RTKE_TIMER) += timer_rtke.o
obj-$(CONFIG_CPU_IDLE_PNX) += cpu-idle_pnx.o
obj-$(CONFIG_CPU_FREQ_PNX) += cpu-freq_pnx.o
obj-$(CONFIG_DBG_RAMDUMP) += ramdump_pnx.o
obj-$(CONFIG_DBG_XOSTRACE) += xos_trace.o xos_trace_ringbuffer.o
obj-$(CONFIG_TEST_KERNEL_PANIC) += test_kernel_panic.o
obj-$(CONFIG_EBI_BUS) += pnx_ebi.o
obj-$(CONFIG_MODEM_BLACK_BOX) += modem_config.o
obj-y += pnx_qos.o
obj-$(CONFIG_PM) += pm.o

obj-y += clock.o power.o

obj-$(CONFIG_BOOTTIME) += boottime.o
