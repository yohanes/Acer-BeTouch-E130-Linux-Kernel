#ifndef _LINUX_PNX_BL_H
#define _LINUX_PNX_BL_H

struct pnx_bl_platform_data {
	int gpio;
	int pin_mux;
	unsigned int pwm_pf;
	unsigned int pwm_tmr;
    char * pwm_clk;
};

#endif /* _LINUX_PNX_BL_H */
