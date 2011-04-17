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

#include <nk/xos_fifo.h>
#include "xos_core.h"
#include <osware/osware.h>

#ifndef XOS_CORE_FIFO_ID
#define XOS_CORE_FIFO_ID 0x6669666f /* "fifo" */
#endif /* XOS_CORE_FIFO_ID */

struct xos_fifo_t
{
    char *            addr[2]; /**< fifo buffer address for each OS */
    unsigned          size;    /**< size of the fifo in bytes */
    NkOsId            osid;    /**< owner OS id */
    unsigned          widx;    /**< fifo writer index */
    unsigned          ridx;    /**< fifo reader index */
    volatile unsigned used;    /**< number of used bytes in the fifo */
};



#define TO_FIFO(h) ( ( ( struct xos_fifo_t * ) (*h) ) - 1 )



xos_fifo_handle_t
xos_fifo_connect ( const char * name,
		   unsigned size )
{
    const NkOsId        osid = nkops.nk_id_get ( );
    const int           type = XOS_CORE_FIFO_ID;
    struct xos_fifo_t * fifo = xos_core_lookup ( name, type );
    xos_fifo_handle_t   self;

    if ( fifo == 0 )
    {
	/* device not found; try to allocate it */
	fifo = xos_core_create ( name, type, sizeof ( *fifo ) + size );

	if ( fifo != 0 )
	{
	    /* local end-point initialization */
	    self = &(fifo->addr[0]);
	    *self = ( char * ) ( fifo + 1 );

	    /* virtual device initialization */
	    fifo->addr[1] = 0;
	    fifo->size = size;
	    fifo->osid = osid;

	    xos_fifo_reset ( self );

	    /* make device available. */
	    xos_core_publish ( fifo );
	}
	else
	{
	    /* allocation failed */
	    self = 0;
	}
    }
    else if ( osid != fifo->osid )
    {
	/* device found; OS is not the owner */
	self = &(fifo->addr[1]);

	/* local end-point initialization at first connection */
	if ( *self == 0 )
	    *self = ( char * ) ( fifo + 1 );
    }
    else
    {
	/* device found; OS is the owner */
	self = &(fifo->addr[0]);
    }

    return self;
}



void
xos_fifo_reset ( xos_fifo_handle_t self )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );

    fifo->used = 0;
    fifo->widx = 0;
    fifo->ridx = 0;
}



void *
xos_fifo_reserve ( xos_fifo_handle_t self,
                   unsigned * size )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );
    unsigned            ridx = fifo->ridx;
    unsigned            widx = fifo->widx;

    if ( widx < ridx )
	*size = ridx - widx - 1;
    else
	*size = fifo->size - widx - ( ridx == 0 );

    return *size > 0 ? *self + widx : ( void * ) 0L;
}



void
xos_fifo_commit ( xos_fifo_handle_t self,
                  unsigned size )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );
    unsigned            widx = fifo->widx + size;

    if ( widx >= fifo->size )
        fifo->widx = 0;
    else
        fifo->widx = widx;

    nkops.nk_atomic_add ( &fifo->used, size );
}



void *
xos_fifo_decommit ( xos_fifo_handle_t self,
                    unsigned * size )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );
    unsigned            widx = fifo->widx;
    unsigned            ridx = fifo->ridx;

    if ( ridx < widx )
    {
        *size = widx - ridx;
	return *self + ridx;
    }
    else if ( fifo->ridx > widx )
    {
        *size = fifo->size - ridx;
	return *self + ridx;
    }
    else
	return ( void * ) 0L;
}



void
xos_fifo_release ( xos_fifo_handle_t self,
		   unsigned size )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );
    unsigned            ridx = fifo->ridx + size;

    if ( ridx >= fifo->size )
        fifo->ridx = 0;
    else
        fifo->ridx = ridx;

    nkops.nk_atomic_sub ( &fifo->used, size );
}



unsigned
xos_fifo_count ( xos_fifo_handle_t self )
{
    struct xos_fifo_t * fifo = TO_FIFO ( self );

    return fifo->used;
}

unsigned 
xos_fifo_cleat (xos_fifo_handle_t self)
{  
	struct xos_fifo_t * fifo = TO_FIFO ( self );
    unsigned            widx = fifo->widx;
	unsigned            ridx = fifo->ridx;
	return ( widx >= ridx ? 1 : 0 );
}

#include <linux/module.h>

EXPORT_SYMBOL(xos_fifo_connect);
EXPORT_SYMBOL(xos_fifo_reset);
EXPORT_SYMBOL(xos_fifo_reserve);
EXPORT_SYMBOL(xos_fifo_commit);
EXPORT_SYMBOL(xos_fifo_decommit);
EXPORT_SYMBOL(xos_fifo_release);
EXPORT_SYMBOL(xos_fifo_count);
EXPORT_SYMBOL(xos_fifo_cleat);
