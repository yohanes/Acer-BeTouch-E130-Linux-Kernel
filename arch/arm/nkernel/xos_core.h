/*
 * Inter-OS core software
 *
 * Copyright (C) 2007 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef _XOSCORE_H
#define _XOSCORE_H

/**
 * @file xos_core.h
 * @author Arnaud TROEL
 * @date 23-Apr-2007
 *
 * @brief Internal unified interface for create and access inter-OS devices.
 * @internal
 */

/**
 * @brief Look for inter-OS device of a certain type with a certain name.
 * @internal
 *
 * Given name and type parameters, the device is unique.
 *
 * @param [in] name specifies the name of the device to find
 * @param [in] type specifies the type of the device to find
 * @return the address of the device if found or 0 if no device was found.
 */
void *
xos_core_lookup ( const char * name,
		  int type );

/**
 * @brief Create an inter-OS named device with a given type.
 * @internal
 *
 * @warning This function should not be called unless a previous call
 *          to xos_core_lookup failed.
 *
 * @param [in] name specifies the name of the device to create
 * @param [in] type specifies the type of the device to create
 * @param [in] size specifies how much memory (in bytes) to allocate
 * @return the address of allocated memory or 0 if allocation failed.
 */
void *
xos_core_create ( const char * name,
		  int type,
		  unsigned size );


/**
 * @brief Make a previously created inter-OS device available.
 * @internal
 *
 * Calls to xos_core_lookup will fail even if corresponding call to
 * function xos_core_create was done unless this function is called.
 * This allows the inter-OS device creator to perform some
 * initialization before the device is accessed from the remote site.
 *
 * @param [in] ptr specifies the address given back by xos_core_create
 */
void
xos_core_publish ( void * ptr );

#endif /* _XOSCORE_H */
