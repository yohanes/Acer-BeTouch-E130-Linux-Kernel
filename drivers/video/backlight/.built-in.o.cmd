cmd_drivers/video/backlight/built-in.o :=  /opt/arm-2008q1/bin/arm-none-linux-gnueabi-ld -EL    -r -o drivers/video/backlight/built-in.o drivers/video/backlight/lcd.o drivers/video/backlight/platform_lcd.o drivers/video/backlight/backlight.o drivers/video/backlight/generic_bl.o drivers/video/backlight/pnx_bl.o 