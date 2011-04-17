/*
 *  linux/arch/arm/plat-pnx/include/mach/pwr.h
 *
 *  Copyright (C) 2010 ST-Ericsson
 *  Written by Vincent Guittot <vincent.guittot@stericsson.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_PWR_H
#define __LINUX_PWR_H

struct device;

/*
 * struct pwr - an machine class defined object / cookie.
 */
struct pwr;

/**
 * pwr_get - lookup and obtain a reference to a power producer.
 * @dev: device for power "consumer"
 * @id: power comsumer ID
 *
 * Returns a struct pwr corresponding to the power producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the power consumer, and thereby
 * the power producer.  (IOW, @id may be identical strings, but
 * pwr_get may return different clock producers depending on @dev.)
 *
 * Drivers must assume that the power source is not enabled.
 */
struct pwr *pwr_get(struct device *dev, const char *id);

/**
 * pwr_enable - inform the system when the power source should be running.
 * @pwr: pwoer source
 *
 * If the power can not be enabled/disabled, this should return success.
 *
 * Returns success (0) or negative errno.
 */
int pwr_enable(struct pwr *pwr);

/**
 * pwr_disable - inform the system when the power source is no longer required.
 * @pwr: power source
 *
 * Inform the system that a power source is no longer required by
 * a driver and may be shut down.
 *
 * Implementation detail: if the power source is shared between
 * multiple drivers, pwr_enable() calls must be balanced by the
 * same number of pwr_disable() calls for the power source to be
 * disabled.
 */
void pwr_disable(struct pwr *pwr);

/**
 * pwr_put	- "free" the power source
 * @pwr: power source
 *
 * Note: drivers must ensure that all pwr_enable calls made on this
 * power source are balanced by pwr_disable calls prior to calling
 * this function.
 */
void pwr_put(struct pwr *pwr);

#endif
