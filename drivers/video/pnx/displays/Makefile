#
# Makefile for Displays using the "LCDBUS"
#

ifeq ($(CONFIG_FB_LCDBUS_JBT6K71_KERNEL_SPLASH_SCREEN),y)
JBT6K71_IMG = $(wildcard $(src)/jbt6k71*.png $(src)/jbt6k71*.raw)
$(obj)/jbt6k71_splash.h:
	# create jbt6k71_splash.h
	cat $(JBT6K71_IMG) > jbt6k71_splash.tmp
	$(src)/bin2c.pl -b8 -vjbt6k71_splash_data -fjbt6k71_splash.tmp -o$@
	rm jbt6k71_splash.tmp

$(obj)/jbt6k71-as-a.o: $(obj)/jbt6k71_splash.h
endif

ifeq ($(CONFIG_FB_LCDBUS_HX8357_KERNEL_SPLASH_SCREEN),y)
HX8357_IMG = $(wildcard $(src)/hx8357*.png $(src)/hx8357*.raw)
$(obj)/hx8357_splash.h:
	# create hx8357_splash.h
	cat $(HX8357_IMG) > hx8357_splash.tmp
	$(src)/bin2c.pl -b8 -vhx8357_splash_data -fhx8357_splash.tmp -o$@
	rm jbt6k71_splash.tmp

$(obj)/hx8357.o: $(obj)/hx8357_splash.h
endif

ifeq ($(CONFIG_FB_LCDBUS_ILI9325_KERNEL_SPLASH_SCREEN),y)
ILI9325_IMG = $(wildcard $(src)/ili9325*.png $(src)/lil9325*.raw)
$(obj)/lil9325_splash.h:
	# create ili9325_splash.h
	cat $(ILI9325_IMG) > ili9325_splash.tmp
	$(src)/bin2c.pl -b8 -vili9325_splash_data -fili9325_splash.tmp -o$@
	rm jbt6k71_splash.tmp

$(obj)/ili9325.o: $(obj)/ili9325_splash.h
endif


ifeq ($(CONFIG_FB_LCDBUS_LC822973_KERNEL_SPLASH_SCREEN),y)
LC822973_IMG = $(wildcard $(src)/lc822973*.png $(src)/lc822973*.raw)
$(obj)/lc822973_splash.h:
	# create lc822973_splash.h
	cat $(LC822973_IMG) > lc822973_splash.tmp
	$(src)/bin2c.pl -b8 -vlc822973_splash_data -flc822973_splash.tmp -o$@
	rm lc822973_splash.tmp

$(obj)/lc822973.o: $(obj)/lc822973_splash.h
endif

ifeq ($(CONFIG_FB_LCDBUS_L5F30906_KERNEL_SPLASH_SCREEN),y)
L5F30906_IMG = $(wildcard $(src)/l5f30906*.png $(src)/l5f30906*.raw)
$(obj)/l5f30906_splash.h:
	# create l5f30906_splash.h
	cat $(L5F30906_IMG) > l5f30906_splash.tmp
	$(src)/bin2c.pl -b8 -vl5f30906_splash_data -fl5f30906_splash.tmp -o$@
	rm l5f30906_splash.tmp

$(obj)/l5f30906.o: $(obj)/l5f30906_splash.h
endif

ifeq ($(CONFIG_FB_LCDBUS_ILI9481BB_KERNEL_SPLASH_SCREEN),y)
ILI9481BB_IMG = $(wildcard $(src)/ili9481bb*.png $(src)/ili9481bb*.raw)
$(obj)/ili9481bb_splash.h:
	# create ili9481bb_splash.h
	cat $(ILI9481BB_IMG) > ili9481bb_splash.tmp
	$(src)/bin2c.pl -b8 -vili9481bb_splash_data -fili9481bb_splash.tmp -o$@
	rm ili9481bb_splash.tmp

$(obj)/ili9481bb.o: $(obj)/ili9481bb_splash.h
endif

ifeq ($(CONFIG_FB_LCDBUS_TVOUT_KERNEL_SPLASH_SCREEN),y)
TVO_IMG = $(wildcard $(src)/tvout*.png $(src)/tvout*.raw)
$(obj)/tvout_splash.h:
	# create tvout_splash.h
	cat $(TVO_IMG) > tvout_splash.tmp
	$(src)/bin2c.pl -b8 -vtvout_splash_data -ftvout_splash.tmp -o$@
	rm tvout_splash.tmp

$(obj)/tvout.o: $(obj)/tvout_splash.h
endif


obj-$(CONFIG_FB_LCDBUS_LCDFB)     += lcdfb.o
obj-$(CONFIG_FB_LCDBUS_JBT6K71)   += jbt6k71-as-a.o
obj-$(CONFIG_FB_LCDBUS_HX8357)    += hx8357.o
obj-$(CONFIG_FB_LCDBUS_ILI9325)   += ili9325.o
obj-$(CONFIG_FB_LCDBUS_L5F30906)  += l5f30906.o
obj-$(CONFIG_FB_LCDBUS_ILI9481BB) += ili9481bb.o
obj-$(CONFIG_FB_LCDBUS_LC822973)  += lc822973.o
obj-$(CONFIG_FB_LCDBUS_TVOUT)     += tvout.o

ifeq ($(CONFIG_FB_LCDBUS_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG
endif

