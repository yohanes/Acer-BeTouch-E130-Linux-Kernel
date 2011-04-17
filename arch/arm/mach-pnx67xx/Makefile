#
## Makefile for the linux kernel, hardware dependent part of ST-Ericsson PNX67xx
#
#
## Object file lists.

# Common support
obj-y			:= devices.o cgu.o pdcupwr.o
obj-m			:=
obj-n			:=
obj-			:=
ACERPROJ=K3

# Jen add for separating gpio setting 20100323
# Specific board GPIO setting
obj-y += gpio_$(ACERPROJ).o
# Jen end 20100323

# Specific board support
obj-$(CONFIG_MACH_PNX67XX_V2_WAVEB_2GB) += board_pnx67xx_wavex.o
obj-$(CONFIG_MACH_PNX67XX_V2_WAVEC_2GB) += board_pnx67xx_wavex.o
obj-$(CONFIG_MACH_PNX67XX_WAVED) += board_pnx67xx_wavex.o
obj-$(CONFIG_MACH_PNX67XX_V2_E150B_2GB) += board_pnx67xx_e150x.o
obj-$(CONFIG_MACH_PNX67XX_V2_E150C_2GB) += board_pnx67xx_e150x.o
obj-$(CONFIG_MACH_PNX67XX_V2_E150D_2GB) += board_pnx67xx_e150x.o

# ACER Ed 20090610 add TI 6450 BT power
obj-$(CONFIG_BT_NSHUTDOWN) += bluetooth-power.o
# End ACER Ed 20090610

#/*ACER Ed 2010-04-14 for WIFI_3V3_EN GPIO*/
obj-$(CONFIG_WLAN_PW) += wlanhwdrv.o
#/*END ACER Ed 2010-04-14*/

# Framemem flags
EXTRA_CFLAGS+=-DCONFIG_FRAMEMEM_DEBUG
EXTRA_CFLAGS+=-DCONFIG_FRAMEMEM_HWMEM_SUPPORT
EXTRA_CFLAGS+=-Iarch/arm/plat-pnx
