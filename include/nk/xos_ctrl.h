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

#ifndef _XOS_CTRL_H
#define _XOS_CTRL_H

/**
 * @file xos_ctrl.h
 * @author Arnaud TROEL
 * @date 23-Apr-2007
 *
 * @brief This file provides an interface to control oriented inter-OS devices.
 *
 * @see xos_fifo.h for inter-OS data management.
 * @see xos_lock.h for inter-OS tristate lock management.
 */

/**
 * @brief Type for control device call-back function.
 */
typedef void ( * xos_ctrl_handler_t ) ( unsigned, void * );

/**
 * @brief Inter-OS control device descriptor.
 */
typedef struct xos_ctrl_t * xos_ctrl_handle_t;

/**
 * @brief Connect an inter-OS control device.
 *
 * The device is created unless it already exists.
 * Both sites should agree on the name and event number of the device.
 *
 * @param [in] name specifies the name of the pipe
 * @param [in] nb_events specifies how much event the device should handle
 *
 * @return A descriptor of the device or 0 if operation failed.
 */
xos_ctrl_handle_t
xos_ctrl_connect ( char const * name, unsigned nb_events );

/**
 * @brief Subscribe to an inter-OS event.
 *
 * @warning Parameter event must be greater or equal to 0 and lower
 * than the number of supported events by the device or something bad
 * will happen (@see xos_ctrl_connect).
 *
 * @param [in] self specifies the device descriptor
 * @param [in] event specifies the event to subscribe to
 * @param [in] handler specifies the call-back function
 * @param [in] cookie specifies an opaque data to pass to the call-back
 * @param [in] clear specifies whether pending status of the event
 *             should be cleared before registering to the event.
 */
void
xos_ctrl_register ( xos_ctrl_handle_t self,
                    unsigned event,
                    xos_ctrl_handler_t handler,
                    void * cookie,
                    int clear );

/**
 * @brief Unsubscribe from an inter-OS event.
 *
 * @warning Parameter event must be greater or equal to 0 and lower
 * than the number of supported events by the device or something bad
 * will happen (@see xos_ctrl_connect).
 *
 * @param [in] self specifies the device descriptor
 * @param [in] event specifies the event to unsubscribe from
 */
void
xos_ctrl_unregister ( xos_ctrl_handle_t self,
                      unsigned event );

/**
 * @brief Mark an event pending for the remote site.
 *
 * @param [in] self specifies the device descriptor
 * @param [in] event specifies the event to raise
 *
 * @return true if event was raised or false if remote site had not
 * subscribed to event when raising it (this is subject to race
 * condition and should be considered as a hint or simply ignored).
 */
int
xos_ctrl_raise ( xos_ctrl_handle_t self,
                 unsigned event );

#endif /* _XOS_CTRL_H */
