/*
 * Filename:     i2c.h
 *
 * Description:  
 *
 * Created:      10.04.2009 15:02:56
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
 * Copyrigth:    (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * changelog:
 *
 */

#ifndef _ARCH_I2C_H
#define _ARCH_I2C_H

struct i2c_platform_data  {
	int gpio_sda;
	int gpio_sda_mode;
	int gpio_scl;
	int gpio_scl_mode;
};


#endif /* _ARCH_I2C_H */

