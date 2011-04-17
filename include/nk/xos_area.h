#ifndef _XOS_AREA_H
#define _XOS_AREA_H

/**
 * @file xos_area.h
 * @author Arnaud TROEL
 * @date 05-Aug-2008
 *
 * @brief This file provides an interface for simple inter-OS buffer devices.
 */

/**
 * @brief Inter-OS area descriptor.
 */
typedef char ** xos_area_handle_t;

/**
 * @brief Connect an inter-OS area.
 *
 * The device is created (if possible) unless it already exists.
 * Both sites should agree on the name and size of the device.
 *
 * @param [in] name specifies the name of the area
 * @param [in] size specifies the size (in bytes) of each area
 *
 * @return A descriptor of the device or 0 if operation failed.
 */
xos_area_handle_t
xos_area_connect ( const char * name,
		   unsigned size );

/**
 * @brief Get a pointer to the memory to read from or write in the area.
 *
 * @param [in] self specifies the area descriptor
 *
 * @return the address of the memory.
 */
void *
xos_area_ptr ( xos_area_handle_t self );

/**
 * @brief Get the size (in bytes) of an inter-OS area.
 *
 * @param [in] self specifies the area descriptor
 *
 * @return how much data is available in the area.
 */
unsigned
xos_area_len ( xos_area_handle_t self );

#endif /* _XOS_AREA_H */
