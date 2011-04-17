/*
 * ============================================================================
 *
 * Filename:     drvchg.h
 *
 * Description:  
 *
 * Version:      1.0
 * Created:      21.08.2009 11:37:21
 * Revision:     none
 * Compiler:     gcc
 *
 * Author:       Ludovic Barre (LBA), Ludovic PT Barre AT stericsson PT com
 * Company:      ST-Ericsson Le Mans
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * changelog:
 *
 * ============================================================================
 */

#ifndef _LINUX_DRVCHG_H
#define _LINUX_DRVCHG_H

/* List of devices and applications supported by 
 * drvchg to handle voltage compensation
 */
/// +ACER_AUx PenhoYu, show PMU charge register
#if 1
enum {
    DRVCHG_BACKLIGHT,
    DRVCHG_KERNEL_MAX,
};

#define DRVCHG_APP_MAX 10
#define DRVCHG_MAX (DRVCHG_KERNEL_MAX + DRVCHG_APP_MAX)

enum {
	USBCHG_DLSHORT,		/* USB event */
	USBCHG_DATALINK,	/* USB event */
	USBCHG_SUSPEND,		/* USB event */
	USBCHG_RESUME,		/* USB event */
	USBCHG_USBSUP,		/* USB charging state */
	USBCHG_NONE			/* USB charging state */
};
#else
enum {
    DRVCHG_BACKLIGHT,
    DRVCHG_AUDIO,
    DRVCHG_VIDEO,
    DRVCHG_CALL,
    DRVCHG_GAME,
    DRVCHG_MAX,
};
#endif
/// +ACER_AUx PenhoYu, show PMU charge register
    
/* Ioctl interface */
enum {
    DRVCHG_IOCTL_READ_MVOLT,
    DRVCHG_IOCTL_READ_TEMP,
    DRVCHG_IOCTL_READ_CURRENT,
	DRVCHG_IOCTL_SET_CHARGE_MODE_TAT,
	DRVCHG_IOCTL_SET_CHARGE_HIGH_CURRENT_TAT,
	DRVCHG_IOCTL_SET_CHARGE_LOW_CURRENT_TAT,
	DRVCHG_IOCTL_STOP_CHARGE_IN_CHARGE_TAT,
	DRVCHG_IOCTL_STOP_CHARGE_IN_DISCHARGE_TAT,
	DRVCHG_IOCTL_MAX,
};

enum {
	DRVCHG_MAIN_CHARGER,
	DRVCHG_USB_CHARGER,
	DRVCHG_NONE_CHARGER,
};

enum {
	DRVCHG_SINGLE_CHARGE_INPUT,
	DRVCHG_DUAL_CHARGE_INPUT,
	DRVCHG_MAX_CHARGE_INPUT,
};

struct drvchg_ioctl_tat_charge {
    int input;
    int source;
};
#endif //_LINUX_DRVCHG_H
