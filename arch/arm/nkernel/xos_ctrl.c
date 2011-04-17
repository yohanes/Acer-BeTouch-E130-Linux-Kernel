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

#include "xos_core.h"
#include <nk/xos_ctrl.h>
#include <osware/osware.h>



#ifndef XOS_CORE_CTRL_ID
#define XOS_CORE_CTRL_ID 0x65766e74 /* evnt */
#endif /* XOS_CORE_CTRL_ID */



/**
 * event descriptor.
 */
struct xos_evnt_t
{
    xos_ctrl_handler_t handler;
    int pending;
    void * cookie;
};



/**
 * local end-point descriptor.
 */
struct xos_ctrl_t
{
    NkXIrq xirq;
    NkOsId osid;
    int    rank;
};



/**
 * device descriptor.
 */
struct xos_ctrl_dev_t
{
    struct xos_ctrl_t ctrl[2]; /**< device end-points descriptors. */
    unsigned          nevt;    /**< number of managed events. */
};



#define TO_VDEV(c) ( struct xos_ctrl_dev_t * ) ( (c) - (c)->rank )
#define TO_EVNT(v,i) ( ( struct xos_evnt_t * ) ( (v) + 1 ) ) + (i) * (v)->nevt



static void
xos_ctrl_handler ( void * cookie,
		   NkXIrq xirq )
{
    struct xos_ctrl_t *     self = ( struct xos_ctrl_t * ) cookie;
    struct xos_ctrl_dev_t * vdev = TO_VDEV ( self );
    unsigned                nevt = vdev->nevt;
    struct xos_evnt_t *     evnt = TO_EVNT ( vdev, self->rank ) + nevt;

    /* travel the events list and execute subscribed pending ones. */
    while ( nevt-- > 0 )
    {
	evnt -= 1;
        if ( evnt->pending != 0 && evnt->handler != ( void * ) 0L )
        {
            evnt->pending = 0;
            evnt->handler ( nevt, evnt->cookie );
        }
    }
}



struct xos_ctrl_t *
xos_ctrl_connect ( const char * name,
                   unsigned nb_events )
{
    const NkOsId            osid = nkops.nk_id_get ( );
    const int               type = XOS_CORE_CTRL_ID;
    struct xos_ctrl_t *     self = 0;
    struct xos_ctrl_dev_t * vdev = xos_core_lookup ( name, type );

    if ( vdev == 0 )
    {
	unsigned size = nb_events * sizeof ( struct xos_evnt_t );

	/* attempt to allocate a cross-interrupt. */
	NkXIrq xirq = nkops.nk_xirq_alloc ( 1 );
	if ( xirq == 0 )
	    goto bail_out;

	/* shared device not found: attempt to create one. */
	vdev = xos_core_create ( name, type, sizeof ( *vdev ) + 2 * size );

	if ( vdev != 0 )
	{
	    /* initialize device end-points. */
	    int i = 2;

	    while ( i-- )
	    {
		struct xos_evnt_t * evnt = TO_EVNT ( vdev, i );
		unsigned e = nb_events;

		while ( e-- > 0 )
		{
		    evnt->pending = 0;
		    evnt->handler = ( void * ) 0L;
		    evnt->cookie  = ( void * ) 0L;
		    evnt += 1;
		}
	    }

	    vdev->ctrl[0].xirq = xirq;
	    vdev->ctrl[0].osid = osid;
	    vdev->ctrl[0].rank = 0;
	    vdev->ctrl[1].xirq = 0;
	    vdev->ctrl[1].osid = 0;
	    vdev->ctrl[1].rank = 1;

	    vdev->nevt = nb_events;

            self = &(vdev->ctrl[0]);

	    /* attach handler to the cross-interrupt. */
	    nkops.nk_xirq_attach ( self->xirq, xos_ctrl_handler, self );

            /* make device available. */
	    xos_core_publish ( vdev );
	}
	else
	    goto bail_out;
    }
    else
    {
	self = &(vdev->ctrl[(vdev->ctrl[0].osid == osid ? 0 : 1)]);

	if ( self->xirq == 0 )
	{
	    /* local end-point initialization */
	    self->osid = osid;
	    self->xirq = nkops.nk_xirq_alloc ( 1 );

	    if ( self->xirq == 0 )
		goto bail_out;

	    /* attach handler to the cross-interrupt. */
	    nkops.nk_xirq_attach ( self->xirq, xos_ctrl_handler, self );
	}
    }

bail_out:
    return self;
}



void
xos_ctrl_register ( struct xos_ctrl_t * self,
                    unsigned event,
                    xos_ctrl_handler_t handler,
                    void * cookie,
                    int clear )
{
    struct xos_ctrl_dev_t * vdev = TO_VDEV ( self );
    struct xos_evnt_t *     evnt = TO_EVNT ( vdev, self->rank ) + event;

    /* clear pending flag is asked to. */
    if ( clear )
	evnt->pending = 0;

    /* execute possibly already pending event. */
    if ( evnt->pending && handler != ( void * ) 0L )
	handler ( event, cookie );

    /* register event information. */
    evnt->cookie  = cookie;
    evnt->handler = handler;
    evnt->pending = 0;
}



void
xos_ctrl_unregister ( struct xos_ctrl_t * self,
                      unsigned event )
{
    struct xos_ctrl_dev_t * vdev = TO_VDEV ( self );
    struct xos_evnt_t *     evnt = TO_EVNT ( vdev, self->rank ) + event;

    evnt->cookie  = ( void * ) 0L;
    evnt->handler = ( void * ) 0L;
}



int
xos_ctrl_raise ( struct xos_ctrl_t * self,
                 unsigned event )
{
    struct xos_ctrl_dev_t * vdev = TO_VDEV ( self );
    unsigned peer = 1 - self->rank;
    struct xos_evnt_t * evnt = TO_EVNT ( vdev, peer ) + event;
    int retval;

    retval = evnt->handler != ( void * ) 0L;

    /* mark remote event pending and trigger a cross-interrupt if
       remote site subscribed the event. */
    evnt->pending = 1;
    if ( retval )
        nkops.nk_xirq_trigger ( vdev->ctrl[peer].xirq, vdev->ctrl[peer].osid );

    return retval;
}


#include <linux/module.h>

EXPORT_SYMBOL(xos_ctrl_connect);
EXPORT_SYMBOL(xos_ctrl_register);
EXPORT_SYMBOL(xos_ctrl_unregister);
EXPORT_SYMBOL(xos_ctrl_raise);
