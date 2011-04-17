#include <nk/xos_area.h>
#include "xos_core.h"
#include <osware/osware.h>

#ifndef XOS_CORE_AREA_ID
#define XOS_CORE_AREA_ID 0x61726561 /* "area" */
#endif /* XOS_CORE_AREA_ID */

struct xos_area_t
{
    char *            addr[2]; /**< area buffer address for each OS */
    unsigned          size;    /**< size of the area in bytes */
    NkOsId            osid;    /**< owner OS id */
};



#define TO_AREA(h) ( ( ( struct xos_area_t * ) (*h) ) - 1 )



xos_area_handle_t
xos_area_connect ( const char * name,
		   unsigned size )
{
    const NkOsId        osid = nkops.nk_id_get ( );
    const int           type = XOS_CORE_AREA_ID;
    struct xos_area_t * area = xos_core_lookup ( name, type );
    xos_area_handle_t   self;

    if ( area == 0 )
    {
	/* device not found; try to allocate it */
	area = xos_core_create ( name, type, sizeof ( *area ) + size );

	if ( area != 0 )
	{
	    /* local end-point initialization */
	    self = &(area->addr[0]);
	    *self = ( char * ) ( area + 1 );

	    /* virtual device initialization */
	    area->addr[1] = 0;
	    area->size = size;
	    area->osid = osid;

	    /* make device available. */
	    xos_core_publish ( area );
	}
	else
	{
	    /* allocation failed */
	    self = 0;
	}
    }
    else if ( osid != area->osid )
    {
	/* device found; OS is not the owner */
	self = &(area->addr[1]);

	/* local end-point initialization at first connection */
	if ( *self == 0 )
	    *self = ( char * ) ( area + 1 );
    }
    else
    {
	/* device found; OS is the owner */
	self = &(area->addr[0]);
    }

    return self;
}



void *
xos_area_ptr ( xos_area_handle_t self )
{
	return *self;
}



unsigned
xos_area_len (xos_area_handle_t self )
{
    struct xos_area_t * area = TO_AREA ( self );

    return area->size;
}


#include <linux/module.h>

EXPORT_SYMBOL(xos_area_connect);
EXPORT_SYMBOL(xos_area_ptr);
EXPORT_SYMBOL(xos_area_len);
