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

#include <osware/osware.h>



static int
xos_core_strcmp ( const char * s1, const char * s2 )
{
    while ( *s1 && *s2 && *s1 == *s2 )
    {
        s1 += 1;
        s2 += 1;
    }

    return *s1 == *s2;
}



static unsigned
xos_core_strlen ( const char * s )
{
    const char * b = s;

    while ( *s ) s += 1;

    return s - b;
}



static void
xos_core_strcpy ( char * d, const char * s )
{
    while ( ( *d++ = *s++ ) );
}



struct xoscore_t
{
  NkDevDesc desc;
  char * name;
};



void *
xos_core_lookup ( const char * name, int type )
{
    void * ptr = 0;
    NkPhAddr addr = 0;
    struct xoscore_t * vdev;

    while ( ( addr = nkops.nk_dev_lookup_by_type ( type, addr ) ) )
    {
        vdev = nkops.nk_ptov ( addr );

        if ( xos_core_strcmp ( vdev->name, name ) )
	{
	    ptr = vdev + 1;
            break;
	}
    }

    return ptr;
}



void *
xos_core_create ( const char * name, int type, unsigned size )
{
    unsigned long name_size = xos_core_strlen ( name );
    unsigned long user_size = ( size + 3 ) & ( ~3 );
    unsigned long core_size = sizeof ( struct xoscore_t );

    NkPhAddr addr = nkops.nk_dev_alloc ( core_size + user_size + name_size );
    void * ptr;

    if ( addr != 0 )
    {
	struct xoscore_t * vdev = nkops.nk_ptov ( addr );
	ptr = vdev + 1;

	vdev->name = ( char * ) ptr + user_size;
	xos_core_strcpy ( vdev->name, name );

	vdev->desc.class_id   = NK_DEV_CLASS_GEN;
	vdev->desc.dev_id     = type;
	vdev->desc.dev_header = addr + sizeof ( vdev->desc );
	vdev->desc.dev_owner  = nkops.nk_id_get ( );
    }
    else
	ptr = 0;

    return ptr;
}



void
xos_core_publish ( void * ptr )
{
    struct xoscore_t * vdev = ( ( struct xoscore_t * ) ptr ) - 1;
    NkPhAddr           addr = nkops.nk_vtop ( vdev );

    nkops.nk_dev_add ( addr );
}


#include <linux/module.h>

EXPORT_SYMBOL(xos_core_lookup);
EXPORT_SYMBOL(xos_core_create);
EXPORT_SYMBOL(xos_core_publish);
