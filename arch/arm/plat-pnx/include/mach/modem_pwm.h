/*
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Loic Pallardy <loic.pallardy@stericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_PNX_MODEM_PWM_H
#define __ARCH_ARM_PNX_MODEM_PWM_H

/**
 * this structure allows to store PWM configuration to transmit to modem
 */

struct pnx_modem_pwm_config{
	__u32 pwm_start;
	__u32 pwm_name;
	__u32 gpio_number;
	__u32 pwm_accessible;
	__u32 pwm_end;
};

enum{
	PWM_ACCESSIBLE=0,
	PWM_NO_ACCESSIBLE,
};

enum{
	PWM1=0,
	PWM2,
	PWM3,
};

#endif
