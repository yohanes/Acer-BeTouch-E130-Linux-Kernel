
struct p9_xos_kopen{
	/* R message call back */
	/* called after Server R Message has been sent */
    void (*r_callback)(struct p9_fcall *);
	void *r_cookie;
	/* T message call back */
	/* called after T Message reception */
    void (*t_callback)(void *);
	void *t_cookie;
};

u32  p9_xosclient_kopen(int channel,struct p9_xos_kopen *callback, const char *name);
struct p9_fcall * p9_xosclient_kread(unsigned long handle);
int p9_xosclient_krelease(struct p9_fcall *ifcall);
int p9_xosclient_kwrite(unsigned long handle, struct p9_fcall *ofcall);

