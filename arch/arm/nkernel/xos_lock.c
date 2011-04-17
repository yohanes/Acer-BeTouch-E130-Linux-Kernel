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

#include <nk/xos_lock.h>
#include "xos_core.h"
#include <osware/osware.h>



#ifndef XOS_CORE_LOCK_ID
#define XOS_CORE_LOCK_ID 0x6c6f636b /* "lock" */
#endif /* XOS_CORE_LOCK_ID */



#define XOS_CORE_LOCK_FREE 2
#define XOS_CORE_LOCK_BUSY 0



struct xos_lock_t
{
    nku32_f state;
};



xos_lock_handle_t
xos_lock_connect ( const char * name )
{
    const int           type = XOS_CORE_LOCK_ID;
    struct xos_lock_t * self = xos_core_lookup ( name, type );

    if ( self == 0 )
    {
	self = xos_core_create ( name, type, sizeof ( *self ) );

	if ( self != 0 )
	    self->state = XOS_CORE_LOCK_FREE;

	xos_core_publish ( self );
    }

    return self;
}



int
xos_lock_acquire ( xos_lock_handle_t self )
{
    int acquired =
	nkops.nk_sub_and_test ( &(self->state), 1 ) != XOS_CORE_LOCK_BUSY;

    return acquired;
}



int
xos_lock_release ( xos_lock_handle_t self )
{
    int expected =
	nkops.nk_sub_and_test ( &(self->state), -1 ) != XOS_CORE_LOCK_FREE;

    return !expected;
}


#include <linux/module.h>

EXPORT_SYMBOL(xos_lock_connect);
EXPORT_SYMBOL(xos_lock_acquire);
EXPORT_SYMBOL(xos_lock_release);
