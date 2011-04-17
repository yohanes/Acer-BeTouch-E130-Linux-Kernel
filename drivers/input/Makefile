#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-objs := input.o input-compat.o ff-core.o

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o

obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
# ACER modify by Jen@20090615
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
# end Jen@20090615
obj-$(CONFIG_INPUT_MISC)	+= misc/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o
obj-$(CONFIG_INPUT_KEYRESET)	+= keyreset.o

obj-$(CONFIG_XEN_KBDDEV_FRONTEND)	+= xen-kbdfront.o

# Owen add eCompass @ 2010.02.06
 obj-$(CONFIG_INPUT_ECOMPASS)      += ecompass/
# End of Owen
