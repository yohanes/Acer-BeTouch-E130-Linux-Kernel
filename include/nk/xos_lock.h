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

#ifndef XOS_LOCK_H
#define XOS_LOCK_H

/**
 * @file xos_lock.h
 * @author Arnaud TROEL
 * @date 23-Apr-2007
 *
 * @brief This file provides an interface to tristate lock inter-OS devices.
 *
 * The reason for this implementation is that it is designed so as to
 * reduce notifications and subsequent system switches compared to the
 * use of an inter-OS pipe.
 *
 * @see xos_ctrl.h for inter-OS control (e.g. notification) management.
 */

/**
 * Inter-OS lock descriptor.
 */
typedef struct xos_lock_t * xos_lock_handle_t;

/**
 * @brief Connect an inter-OS lock device.
 *
 * The device is created unless it already exists.
 * Both sites should agree on the name of the device.
 *
 * @param [in] name specifies the name of the pipe
 *
 * @return A descriptor of the device or 0 if operation failed.
 */
xos_lock_handle_t
xos_lock_connect ( const char * name );

/**
 * @brief Attempt to acquire the lock.
 *
 * @param [in] self specifies the lock descriptor
 *
 * @return true if the lock is actually acquired or false if the
 * caller should wait for a remote site notification to complete the
 * operation.
 */
int
xos_lock_acquire ( xos_lock_handle_t self );

/**
 * @brief Release the lock.
 *
 * @param [in] self specifies the lock descriptor
 *
 * @return true if the lock is actually released or false if the
 * caller notify the remote site to complete the operation.
 */
int
xos_lock_release ( xos_lock_handle_t self );

#endif /* XOS_LOCK_H */
