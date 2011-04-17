/*
 * linux/arch/arm/plat-pnx/include/mach/xos_trace.h
 *
 * Copyright (C) 2010 ST-Ericsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *     Created:  03/02/2010 01:01:11 PM
 *      Author:  Gabriel Fernandez (GFE), gabriel.fernandez@stericsson.com
 *
 */

#if defined (XOSTRACE_C)

#define XOS_TRACE_AREA_NAME "XOSTRACE"

enum
{
      XOSTRACE_APP_MODEM_READY = 0
	, XOSTRACE_APP_DATA_READY
	, XOSTRACE_APP_BUFFER_FULL
	, XOSTRACE_APP_SEND_CMD

	, XOSTRACE_SPY_MODEM_READY
	, XOSTRACE_SPY_DATA_READY
	, XOSTRACE_SPY_BUFFER_FULL
	, XOSTRACE_SPY_SEND_CMD

	, MAX_XOSTRACE_EVENT
};


#define MAX_MODEM_CMD_BUFFER_LEN 8192

struct xos_trace_cmd_t {
	int		status;
	char	buffer[MAX_MODEM_CMD_BUFFER_LEN];
	char	*c_ptr;
	int		length;
};

struct xos_trace_modem_spy {
	char	enabled;
	struct xos_trace_cmd_t			cmd;
	struct xos_trace_ring_buffer	ring_buffer;

};


struct xos_trace_modem_app {
	char   enabled;
	struct xos_trace_cmd_t			cmd;
	struct xos_trace_ring_buffer	ring_buffer;
};

struct xos_trace_mngt_shared {
	struct xos_trace_modem_spy modem_spy;
	struct xos_trace_modem_app modem_app;
};

struct xos_trace_ctxt {
	xos_area_handle_t				shared;
	struct xos_trace_mngt_shared	*base_adr;
	
	xos_ctrl_handle_t spy_request;
	xos_ctrl_handle_t app_request;

	xos_ctrl_handle_t request;
};

struct xos_trace_app_frame {
	 u8  start;
	u32 id;
	u64 time_stamp;
	u32 size;
};

#define XOSTRACE_MAX_PROCESS 32

#define XOS_TRACE_MAX_MSG_LENGTH 1024

#define XOS_TRACE_START 0x55


#define XOS_TRACE_MODEM_APP_MASK 0x80000000
#define XOS_TRACE_LINUX_KER_MASK 0x40000000   
#define XOS_TRACE_MODEM_DAE_MASK 0x20000000

#endif

#if defined (XOSTRACE_C)
#define XOSTRACE_GLOBAL 
#else 
#define XOSTRACE_GLOBAL extern
#endif

XOSTRACE_GLOBAL int  xos_Trace_Register(char *name, u8 active);
XOSTRACE_GLOBAL int  xos_Trace_PutBuffer(int id, u32 size, char *buffer);
XOSTRACE_GLOBAL void xos_Trace_Put_buffer(u32 id, const char *fmt, ...);
XOSTRACE_GLOBAL int  xos_trace_get_process_status(int id);


#define XOS_TRACE_REGISTER(name, status)		   xos_Trace_Register(name,status)

#define XOS_TRACE_PUTBUFFER(id,size,buf) \
	do { \
		if ( (id!=-1) && (xos_trace_get_process_status(id))) \
			xos_Trace_PutBuffer(id,size,buf); \
	} while(0)

#define XOS_TRACE_PUT_BUFFER(id, fmt, __args__...) \
	do { \
		if ( (id!=-1) && (xos_trace_get_process_status(id))) \
			xos_Trace_Put_buffer(id, "" fmt, "", ##__args__); \
	} while(0)


