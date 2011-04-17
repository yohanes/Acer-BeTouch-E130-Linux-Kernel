#define RAMDUMP_AREA_NAME "RAMDUMP"

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
};

