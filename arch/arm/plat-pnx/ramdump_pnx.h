#define RAMDUMP_AREA_NAME "RAMDUMP"

#define RAMDUMP_SEND_REQUEST_EVENT_NAME "SEND_REQUEST" 

#define RAMDUMP_SEND_REQUEST_EVENT_ID 0

struct ramdump_mngt_os_rtke {
	unsigned long var;
};
struct ramdump_mngt_os_kernel {
	unsigned long var;
	/* kernel panic information */
	char panic_buf[1024];
};

struct ramdump_mngt_shared {
	struct ramdump_mngt_os_kernel kernel;
	struct ramdump_mngt_os_rtke rtke;
};

struct ramdump_ctxt {
	xos_area_handle_t shared;
	struct ramdump_mngt_shared *base_adr;
	xos_ctrl_handle_t new_request;
};



