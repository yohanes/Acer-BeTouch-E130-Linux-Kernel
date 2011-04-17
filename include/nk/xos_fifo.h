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

#ifndef _XOS_FIFO_H
#define _XOS_FIFO_H

/**
 * @file xos_fifo.h
 * @author Arnaud TROEL
 * @date 23-Apr-2007
 *
 * @brief This file provides an interface to data oriented inter-OS devices.
 *
 * @see xos_ctrl.h for inter-OS control management.
 */

/**
 * @brief Inter-OS fifo descriptor.
 */
typedef char ** xos_fifo_handle_t;

/**
 * @brief Connect an inter-OS fifo.
 *
 * The device is created (if possible) unless it already exists.
 * Both sites should agree on the name and size of the device.
 *
 * @param [in] name specifies the name of the fifo
 * @param [in] size specifies the size (in bytes) of each fifo
 *
 * @return A descriptor of the device or 0 if operation failed.
 */
xos_fifo_handle_t
xos_fifo_connect ( const char * name,
		   unsigned size );

/**
 * @brief Re-initialize a fifo.
 *
 * @param [in] self specifies the fifo descriptor
 */
void
xos_fifo_reset ( xos_fifo_handle_t self );

/**
 * @brief Get available contiguous memory in the fifo so as to write in.
 *
 * @param [in] self specifies the fifo descriptor
 * @param [out] length returns the size of the reserved chunk of memory
 *
 * @return the address of the available chunk of memory of 0 if fifo is full.
 */
void *
xos_fifo_reserve ( xos_fifo_handle_t self,
                   unsigned * size );

/**
 * @brief Make previously reserved space in the fifo available to the remote
 *        site.
 *
 * @param [in] self specifies the fifo descriptor
 * @param [in] length specifies how much space to commit
 */
void
xos_fifo_commit ( xos_fifo_handle_t self,
                  unsigned size );

/**
 * @brief Get committed memory from the fifo to read from.
 *
 * @param [in] self specifies the fifo descriptor
 * @param [out] length returns the size of the chunk of memory
 *
 * @return the address of the readable chunk of memory of 0 if fifo is empty.
 */
void *
xos_fifo_decommit ( xos_fifo_handle_t self,
                    unsigned * size );

/**
 * @brief Make previously decommitted memory availble to the remote site.
 *
 * @param [in] self specifies the fifo descriptor
 * @param [in] length specifies how much space to release
 */
void
xos_fifo_release ( xos_fifo_handle_t self,
                   unsigned size );

/**
 * @brief Get how much committed data is pending.
 *
 * @note This function is subject to inter-OS race conditions so you should
 *       not expect an accurate value.
 *
 * @param [in] self specifies the fifo descriptor
 *
 * @return how much data is available for reading to the remote site.
 */
unsigned
xos_fifo_count ( xos_fifo_handle_t self );

unsigned
xos_fifo_cleat ( xos_fifo_handle_t self );


#endif /* _XOS_FIFO_H */
