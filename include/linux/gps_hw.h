/*
 * linux/include/linux/gps_hw.h
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Michel JAOUEN <michel.jaouen@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */
#ifndef _LINUX_GPS_HW_H_
#define _LINUX_GPS_HW_H_

#define GPS_POWER_ON      _IO('p', 0x01) /*   chip on */
#define GPS_POWER_OFF     _IO('p', 0x02) /*  chip off  */
#define GPS_RESET_ON      _IO('p', 0x03) /*  chip reset on */
#define GPS_RESET_OFF     _IO('p', 0x04) /*  chip reset off */

#endif
