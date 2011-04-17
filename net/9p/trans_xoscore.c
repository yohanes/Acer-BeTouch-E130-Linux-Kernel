#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/poll.h>

#include <linux/wakelock.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include <net/9p/transport.h>
#include "protocol.h"

#include <nk/xos_ctrl.h>
#include <nk/xos_area.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define P9_XOS_VERSION "2.0"
#define DEVICE_NAME "ttnp"
#define NB_CHANNEL (1 << 8)

#define P9_XOS_EVENT 0
#define P9_XOS_EVENT_INIT 1

#define MIN_PACKETS_TO_RELEASE 10

static int nb_free_packets;
static int debug_mask;
module_param_named(debug, debug_mask, int, S_IRUGO | S_IWUSR);

#ifdef CONFIG_NET_9P_XOSCORE_DEBUG
#define __pdebug(fmt, arg...) printk(KERN_DEBUG fmt, ## arg)
#define __log_event(drv, who, what, from)				\
	do								\
		if (unlikely(debug_mask & 8))				\
			 p9_xos_log_event(drv, who, what, from);	\
	while(0)
#else
#define __pdebug(fmt, arg...) do { } while (0)
#define __log_event(drv, who, what, from) do { } while (0)
#endif

#define prolog(fmt, arg...)						\
	do								\
		if (unlikely(debug_mask & 1))				\
			__pdebug("%s> " fmt "\n", __func__, ## arg);	\
	while (0)

#define epilog(fmt, arg...)						\
	do								\
		if (unlikely(debug_mask & 2))				\
			__pdebug("%s< " fmt "\n", __func__, ## arg);	\
	while (0)

#define critical(fmt, arg...)					\
	printk(KERN_ERR "%s: " fmt "\n", __func__, ## arg)

#define warning(fmt, arg...)					\
	printk(KERN_WARNING "%s: " fmt "\n", __func__, ## arg)

#define info(fmt, arg...)					\
	printk(KERN_INFO "%s: " fmt "\n", __func__, ## arg)

#define deque_null (~0)
#define deque_head(q) ((q)[0])
#define deque_tail(q) ((q)[1])
#define deque_next(n, ep) (*((int *)n2a(n, ep)))

/*
 * Linux reads on endpoint 0 and writes on endpoint 1.
 */

enum p9_xos_ep_type {
	RD_EP = 0,
	WR_EP = 1
};

enum p9_xos_driver_state {
	WE_BIT,
};

struct p9_xos_endpoint {
	int *regs;
	void *chunks;
	int lqueue[2];
	int rqueue[2];
};

struct p9_xos_device {
	unsigned int id;
	struct p9_client *client;
	struct p9_xos_driver *driver;
	unsigned long int latency;

	spinlock_t lock;
	struct list_head req_list;
	int open;
	int ack_count;

	wait_queue_head_t t_wait;
	wait_queue_head_t r_wait;
	void (*rd_cb) (struct p9_xos_device *, struct p9_req_t *);
	void (*wr_cb) (struct p9_xos_device *, struct p9_req_t *);
	struct p9_xos_kopen *kops;

	char c_name[32];
	char s_name[32];
};

struct p9_xos_driver {
	xos_ctrl_handle_t ctrl;
	struct p9_xos_endpoint ep[2];

	spinlock_t q_lock;
	struct list_head req_list;

	struct work_struct rwork;
	struct work_struct wwork;
	struct work_struct swork;
	struct workqueue_struct *workqueue;
	struct timer_list timer;
	long int state;

	spinlock_t c_lock;
	spinlock_t ep_lock;
	struct p9_xos_device device[NB_CHANNEL];

	struct kmem_cache *cache;

	unsigned e_skip;
	unsigned e_curr;
	unsigned e_full;
	struct {
		unsigned what;
		unsigned long long when;
		unsigned who;
		unsigned from;
	} e_ring[1024];
	unsigned long long e_date;
	spinlock_t e_lock;

	int wake_count;
	int wake_status;
	struct wake_lock wake_lock;
};

enum p9_xos_device_regs {
	CARD,
	SIZE,
	ADDR,
	LHEAD,
	LTAIL,
	RHEAD,
	RTAIL,
	STARVATION,
	FAIL,
};

enum flow_state {
	MUST_SYNC,
	MUST_READ,
	MUST_WRITE
};

/* log each 9P activity */
static void p9_xos_log_event(struct p9_xos_driver *driver, int who, int what,
			     int from)
{
	if (driver->e_skip)
		return;

	spin_lock(&driver->e_lock);

	driver->e_ring[driver->e_curr].when = sched_clock();
	driver->e_ring[driver->e_curr].what = what;
	driver->e_ring[driver->e_curr].who = who;
	driver->e_ring[driver->e_curr].from = from;

	driver->e_curr++;

	if (unlikely(driver->e_curr == ARRAY_SIZE(driver->e_ring))) {
		driver->e_curr = 0;
		driver->e_full = 1;
	}

	spin_unlock(&driver->e_lock);
}

/* index to chunk virtual address converter */
static inline char *n2a(int n, struct p9_xos_endpoint *ep)
{
	return (char *)ep->chunks + (n << ep->regs[SIZE]);
}

/* chunck virtual address to index converter */
static inline unsigned int a2n(char *chunk, struct p9_xos_endpoint *ep)
{
	return (chunk - (char *)ep->chunks) >> ep->regs[SIZE];
}

static int p9_xos_deque_pop(int *deque, struct p9_xos_endpoint *ep)
{
	int n = deque_head(deque);

	if (n == deque_null)
		goto bail_out;

	deque_head(deque) = deque_next(n, ep);
	deque_next(n, ep) = deque_null;

	if (deque_head(deque) == deque_null)
		deque_tail(deque) = deque_null;

bail_out:
	return n;
}

static void p9_xos_deque_push(int *deque, int n, struct p9_xos_endpoint *ep)
{
	if (deque_tail(deque) != deque_null)
		deque_next(deque_tail(deque), ep) = n;
	else
		deque_head(deque) = n;

	deque_tail(deque) = n;
}

static void p9_xos_deque_reset(int *deque, int head, int tail)
{
	deque_head(deque) = head;
	deque_tail(deque) = tail;
}

static void p9_xos_deque_clear(int *deque)
{
	p9_xos_deque_reset(deque, deque_null, deque_null);
}

static void p9_xos_deque_move(int *d, int *s, struct p9_xos_endpoint *ep)
{
	if (deque_tail(s) == deque_null)
		return;

	if (deque_tail(d) != deque_null)
		deque_next(deque_tail(d), ep) = deque_head(s);
	else
		deque_head(d) = deque_head(s);

	deque_tail(d) = deque_tail(s);
	p9_xos_deque_clear(s);
}

static int p9_xos_chunks_array_init(struct p9_xos_endpoint *ep, int writer)
{
	int retval = 0;
	int i;

	/* Allocate memory chunks */
	ep->chunks = kmalloc(ep->regs[CARD] << ep->regs[SIZE], GFP_KERNEL);
	if (!ep->chunks) {
		critical("kmalloc of xosnp paquets failed");
		ep->regs[FAIL] = 1;
		retval = -ENOMEM;
		goto bail_out;
	}

	/* Save physical address of chunks array */
	ep->regs[ADDR] = __pa(ep->chunks);

	/* Link chunks */
	BUG_ON(!ep->regs[CARD]);
	for (i = 0; i < ep->regs[CARD] - 1; i += 1)
		deque_next(i, ep) = i + 1;
	deque_next(i, ep) = deque_null;

	if (writer) {
		p9_xos_deque_clear(ep->rqueue);
		p9_xos_deque_reset(ep->lqueue, 0, ep->regs[CARD] - 1);
	} else {
		p9_xos_deque_clear(ep->lqueue);
		p9_xos_deque_clear(ep->rqueue);
	}

bail_out:
	return retval;
}

static void p9_xos_flow(unsigned int event, void *cookie)
{
	struct p9_xos_driver *drv = cookie;
	struct p9_xos_endpoint *ep;
	unsigned long flags;
	long int state = 0;

	prolog("e=%u c=%p", event, cookie);

	hw_raw_local_irq_save(flags);

	BUG_ON(event != 0);

	/* get empty packets */
	ep = &drv->ep[WR_EP];
	p9_xos_deque_move(ep->lqueue, &ep->regs[LHEAD], ep);

	if (ep->regs[STARVATION]) { /* Linux needs empty packets */
		ep->regs[STARVATION] = 0;
		set_bit(MUST_WRITE, &state);
	}

	/* get data */
	ep = &drv->ep[RD_EP];
	p9_xos_deque_move(ep->lqueue, &ep->regs[LHEAD], ep);
	if (ep->regs[STARVATION]) { /* RTK needs empty packets */
		drv->wake_status = 2;
		set_bit(MUST_SYNC, &state);
	}

	if (deque_head(ep->lqueue) != deque_null) {
		drv->wake_status = 2;
		set_bit(MUST_READ, &state);
	}

	hw_raw_local_irq_restore(flags);

	if (test_bit(MUST_SYNC, &state))
		queue_work(drv->workqueue, &drv->swork);

	if (test_bit(MUST_WRITE, &state))
		queue_work(drv->workqueue, &drv->wwork);

	if (test_bit(MUST_READ, &state))
		queue_work(drv->workqueue, &drv->rwork);

	if (drv->wake_status == 2)
		wake_lock(&drv->wake_lock);

	epilog();
}

static void p9_xos_init(unsigned int event, void *cookie)
{
	struct p9_xos_driver *drv = cookie;

	prolog("e=%u c=%p", event, cookie);

	if (drv->ep[event].regs[FAIL])
		panic("9P2000 inter-OS transport initialization failed");

	if (event)
		xos_ctrl_unregister(drv->ctrl, event);
	else
		xos_ctrl_register(drv->ctrl, P9_XOS_EVENT, p9_xos_flow, drv, 0);

	epilog();
}

static void p9_xos_start_write_pump(unsigned long int data)
{
	struct p9_xos_driver *drv = (struct p9_xos_driver *)data;

	prolog("d=%p", drv);

	if (!test_and_set_bit(WE_BIT, &drv->state))
		queue_work(drv->workqueue, &drv->wwork);

	epilog();
}

static void p9_xos_sync_work(struct work_struct *work)
{
	struct p9_xos_driver *drv;
	struct p9_xos_endpoint *ep;
	unsigned long flags;

	prolog("w=%p", work);

	drv = container_of(work, struct p9_xos_driver, swork);

	/* send data */
	ep = &drv->ep[WR_EP];
	hw_raw_local_irq_save(flags);
	p9_xos_deque_move(&ep->regs[RHEAD], ep->rqueue, ep);
	hw_raw_local_irq_restore(flags);

	/* send empty packets if needed */
	ep = &drv->ep[RD_EP];
	if (nb_free_packets >= MIN_PACKETS_TO_RELEASE || ep->regs[STARVATION]) {
		nb_free_packets = 0;
		hw_raw_local_irq_save(flags);
		p9_xos_deque_move(&ep->regs[RHEAD], ep->rqueue, ep);
		hw_raw_local_irq_restore(flags);
	}

	xos_ctrl_raise(drv->ctrl, P9_XOS_EVENT);

	if ((!drv->wake_count) && (drv->wake_status == 1)) {
		drv->wake_status = 0;
		wake_unlock(&drv->wake_lock);
		wmb();
		if (drv->wake_status == 2)
			wake_lock(&drv->wake_lock);
	}

	epilog();
}

static void p9_xos_write_work(struct work_struct *work)
{
	struct p9_xos_driver *drv;
	struct p9_xos_endpoint *ep;
	unsigned long flags;

	prolog("w=%p", work);

	drv = container_of(work, struct p9_xos_driver, wwork);
	ep = &drv->ep[WR_EP];

	spin_lock(&drv->q_lock);
	if (list_empty(&drv->req_list)) {
		clear_bit(WE_BIT, &drv->state);
		spin_unlock(&drv->q_lock);
		goto done;
	}
	spin_unlock(&drv->q_lock);

	do {
		u8 *ptr;
		struct p9_req_t *req;
		struct p9_xos_device *device;
		int n;

		req = list_first_entry(&drv->req_list, struct p9_req_t,
				       req_list);

		spin_lock_irqsave(&drv->ep_lock, flags);
		n = p9_xos_deque_pop(ep->lqueue, ep);
		spin_unlock_irqrestore(&drv->ep_lock, flags);
		if (n == deque_null) {
			ep->regs[STARVATION] = 1;
			break;
		}
		ptr = n2a(n, ep) + 4;

		device = req->aux;
		spin_lock(&drv->q_lock);
		list_del(&req->req_list);
		req->status = REQ_STATUS_SENT;
		spin_unlock(&drv->q_lock);

		*(unsigned int *)ptr = device->id;
		ptr += 4;

		if (req->tc) {
			memcpy(ptr, req->tc->sdata, req->tc->size);
		} else {
			memcpy(ptr, req->rc->sdata, req->rc->size);
			spin_lock(&device->lock);
			BUG_ON(!device->ack_count);
			device->ack_count -= 1;

			/*
			 *  Dirty hack for pmu_int server
			 *    pmu_int is on channel 1
			 *    pmu_int client has always a request pending
			 *    so does not keep the wake lock if only
			 *    pmu_int request pending
			 */
			if (likely(device != &drv->device[1]))
				drv->wake_count--;

			if (device->wr_cb)
				device->wr_cb(device, req);
			kmem_cache_free(drv->cache, req);
			spin_unlock(&device->lock);
		}
		__log_event(drv, device->id, ptr[4], WR_EP);

		spin_lock_irqsave(&drv->ep_lock, flags);
		p9_xos_deque_push(ep->rqueue, n, ep);
		spin_unlock_irqrestore(&drv->ep_lock, flags);

	} while (!list_empty(&drv->req_list));

	queue_work(drv->workqueue, &drv->swork);

done:
	clear_bit(WE_BIT, &drv->state);

	epilog();
}

static void p9_xos_read_work(struct work_struct *work)
{
	struct p9_xos_driver *drv;
	struct p9_xos_endpoint *ep;
	int n;
	unsigned long flags;

	prolog("w=%p", work);

	drv = container_of(work, struct p9_xos_driver, rwork);
	ep = &drv->ep[RD_EP];

	drv->wake_status = 1;

	spin_lock_irqsave(&drv->ep_lock, flags);
	n = p9_xos_deque_pop(ep->lqueue, ep);
	spin_unlock_irqrestore(&drv->ep_lock, flags);
	if (n == deque_null)
		goto done;

	do {
		u16 tag;
		int id;
		unsigned int size;
		struct p9_xos_device *device;
		struct p9_req_t *req;
		u8 *ptr;
		u8 type;

		ptr = n2a(n, ep) + 4;

		id = *(int *)ptr;
		ptr += 4;

		size = le32_to_cpu(*(__le32 *) ptr);
		if (size < 7) {
			critical("ignoring too short request");
			break;
		}

		type = *(ptr + 4);

		__log_event(drv, id, type, RD_EP);

		device = &drv->device[id];

		if (type & 1) {
			if (size >= device->client->msize) {
				warning("requested packet size too big: %d\n",
					size);
				goto ignore;
			}
			tag = le16_to_cpu(*(__le16 *) (ptr + 5));
			req = p9_tag_lookup(device->client, tag);

			if (req == NULL) {
				warning("ignoring unexpected response");
				goto ignore;
			}

			BUG_ON(!req->rc);

			if (likely(req->aio_cb != NULL)) {
				req->rc->sdata = ptr;
				req->status = REQ_STATUS_RCVD;
				p9_client_notify_aio(device->client, req);
			} else {
				req->rc->sdata =
				    (char *)req->rc + sizeof(*req->rc);
				memcpy(req->rc->sdata, ptr, size);
				p9_client_cb(device->client, req);
			}
ignore:
			spin_lock_irqsave(&drv->ep_lock, flags);
			p9_xos_deque_push(ep->rqueue, n, ep);
			nb_free_packets++;
			spin_unlock_irqrestore(&drv->ep_lock, flags);
		} else {
			/*
			 *  Dirty hack for pmu_int server
			 *    pmu_int is on channel 1
			 *    pmu_int client has always a request pending
			 *    so does not keep the wake lock if only
			 *    pmu_int request pending
			 */
			if (likely(device != &drv->device[1]))
				drv->wake_count++;

			if (unlikely(!device->open)) {
				warning("DEVICE %d NOT OPENED, ignoring req",
					device->id);
				goto ignore2;
			}
			req = kmem_cache_alloc(drv->cache, GFP_KERNEL);
			req->tc = kmalloc(sizeof(struct p9_fcall), GFP_KERNEL);
			req->tc->size = size;
			req->tc->sdata = ptr;
			req->aux = device;

			spin_lock(&device->lock);
			list_add_tail(&req->req_list, &device->req_list);
			spin_unlock(&device->lock);

			if (device->rd_cb)
				device->rd_cb(device, req);
		}
ignore2:
		spin_lock_irqsave(&drv->ep_lock, flags);
		n = p9_xos_deque_pop(ep->lqueue, ep);
		spin_unlock_irqrestore(&drv->ep_lock, flags);
	} while (n != deque_null);

done:
	if ((!drv->wake_count) && (drv->wake_status == 1)) {
		drv->wake_status = 0;
		wake_unlock(&drv->wake_lock);
		wmb();
		if (drv->wake_status == 2)
			wake_lock(&drv->wake_lock);
	}
	epilog();
}

static int p9_xos_cancel(struct p9_client *client, struct p9_req_t *req)
{
	int retval = 1;
	struct p9_xos_device *device;
	struct p9_xos_driver *drv;

	device = (struct p9_xos_device *)client->conn;
	BUG_ON(device != req->aux);

	spin_lock(&drv->q_lock);
	if (req->status == REQ_STATUS_UNSENT) {
		req->status = REQ_STATUS_FLSHD;
		list_del(&req->req_list);
		if (req->aio_cb)
			p9_client_notify_aio(device->client, req);
		retval = 0;
	}
	spin_unlock(&drv->q_lock);

	return retval;
}

static void p9_xos_add_write_request(struct p9_req_t *req,
				     unsigned long int latency)
{
	struct p9_xos_device *dev = (struct p9_xos_device *)req->aux;
	struct p9_xos_driver *drv = dev->driver;
	unsigned long int expires;

	spin_lock(&drv->q_lock);
	req->status = REQ_STATUS_UNSENT;
	INIT_LIST_HEAD(&req->req_list);
	list_add_tail(&req->req_list, &drv->req_list);
	spin_unlock(&drv->q_lock);

	expires = latency + jiffies;
	if (time_is_after_jiffies(expires) &&
	    (!timer_pending(&drv->timer) ||
	     time_after(drv->timer.expires, expires)))
		mod_timer(&drv->timer, expires);
	else {
		del_timer(&drv->timer);
		p9_xos_start_write_pump((unsigned long int)drv);
	}
}

static int p9_xos_request(struct p9_client *client, struct p9_req_t *req)
{
	int retval = 0;
	struct p9_xos_device *device;
	struct p9_xos_driver *drv;

	prolog("c=%p r=%p", client, req);

	device = (struct p9_xos_device *)client->conn;
	drv = device->driver;

	req->aux = device;
	p9_xos_add_write_request(req, device->latency);

	epilog("%d", retval);

	return retval;
}

static void p9_xos_close(struct p9_client *client)
{
	struct p9_xos_device *device;

	prolog("c=%p", client);

	client->status = Disconnected;
	device = (struct p9_xos_device *)client->conn;
	device->client = NULL;
	device->c_name[0] = 0;

	epilog();
}

static int p9_xos_parse_opts(char *params, struct p9_xos_device *device)
{
	enum { opt_latency, opt_err, };
	static match_table_t tokens = {
		{opt_latency, "latency=%u"},
		{opt_err, NULL},
	};
	int retval = 0;
	char *options, *tmp_options, *p;

	prolog("p=%s c=%p", params, device);

	tmp_options = kstrdup(params, GFP_KERNEL);
	if (NULL == tmp_options) {
		retval = -ENOMEM;
		goto done;
	}

	options = tmp_options;

	while ((p = strsep(&options, ",")) != NULL) {
		int token, option;
		substring_t args[MAX_OPT_ARGS];

		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case opt_latency:
			if (!match_int(&args[0], &option))
				device->latency = msecs_to_jiffies(option);
			else
				warning("ignoring malformed latency option");
			break;
		default:
			break;
		}
	}

	kfree(tmp_options);

done:
	epilog("%d", retval);

	return retval;
}

static struct p9_xos_driver driver;

static int p9_xos_create(struct p9_client *client, const char *addr, char *args)
{
	struct p9_xos_device *device = NULL;	/* avoid compiler warning */
	unsigned int id;
	int error;

	prolog("c=%p a=%s a=%s", client, addr, args);

	if (unlikely(!addr))
		addr = "anonymous";

	id = ARRAY_SIZE(driver.device);
	spin_lock(&driver.c_lock);
	while (id--) {
		device = &driver.device[id];
		if (device->client == NULL) {
			device->client = client;
			break;
		}
	}
	spin_unlock(&driver.c_lock);
	if (id > ARRAY_SIZE(driver.device)) {
		error = -ENOMEM;
		goto bail_out;
	}

	strncpy(device->c_name, addr, ARRAY_SIZE(device->c_name) - 1);
	device->c_name[ARRAY_SIZE(device->c_name) - 1] = 0;

	client->status = Connected;
	client->conn = (struct p9_conn *)device;
	device->latency = 0;

	error = p9_xos_parse_opts(args, device);
	if (error)
		warning("bad options for client '%s' (%d)", addr, error);

	info("client '%s' got device %u", addr, id);
	error = 0;

bail_out:
	epilog("%d", error);

	return error;
}

static struct p9_trans_module p9_xos_trans_module = {
	.name = "xoscore",
	.create = p9_xos_create,
	.close = p9_xos_close,
	.request = p9_xos_request,
	.cancel = p9_xos_cancel,
	.def = 0,
	.owner = THIS_MODULE,
};

struct p9_xos_kopen {
	void (*r_callback) (struct p9_fcall *);
	void *r_cookie;
	void (*t_callback) (void *);
	void *t_cookie;
};

static void p9_xosclient_rerror(struct p9_req_t *req)
{
	struct p9_fcall *rcall;
	u16 tag;
	struct p9_xos_device *dev = (struct p9_xos_device *)req->aux;
	struct p9_xos_endpoint *ep = &dev->driver->ep[RD_EP];
	unsigned long flags;

	if (p9_parse_header(req->tc, &req->tc->size, &req->tc->id, &tag, 1))
		warning("Failed to decode header !");
	kfree(req->tc);
	spin_lock_irqsave(&dev->driver->ep_lock, flags);
	p9_xos_deque_push(ep->rqueue, a2n(req->tc->sdata - 8, ep), ep);
	nb_free_packets++;
	spin_unlock_irqrestore(&dev->driver->ep_lock, flags);

	rcall = kmalloc(sizeof(struct p9_fcall) + 32, GFP_KERNEL);
	p9pdu_reset(rcall);
	rcall->sdata = (u8 *) (rcall + sizeof(struct p9_fcall));
	p9pdu_writef(rcall, 0, "dbwT", 0, P9_RERROR, tag,
		     "9P destination service closed");
	p9pdu_finalize(rcall);

	req->rc = rcall;
	req->tc = NULL;

	spin_lock(&dev->lock);
	dev->ack_count += 1;
	spin_unlock(&dev->lock);

	p9_xos_add_write_request(req, 0);
}

static unsigned int p9_xosclient_poll(struct file *instance,
				      poll_table *event_list)
{
	struct p9_xos_device *dev =
	    (struct p9_xos_device *)instance->private_data;
	int r_empty, tcount;
	int ret = 0;

	poll_wait(instance, &dev->t_wait, event_list);
	spin_lock(&dev->lock);
	r_empty = list_empty(&dev->req_list);
	tcount = dev->ack_count;
	spin_unlock(&dev->lock);
	if (!r_empty)
		ret |= POLLIN | POLLRDNORM;
	if (tcount == 0)
		ret |= POLLOUT;
	return ret;
}

static void p9_xos_kern_rd_cb(struct p9_xos_device *device,
			      struct p9_req_t *req)
{
	device->kops->t_callback(device->kops->t_cookie);
}

static void p9_xos_kern_wr_cb(struct p9_xos_device *device,
			      struct p9_req_t *req)
{
	device->kops->r_callback(req->rc);
}

u32 p9_xosclient_kopen(int channel, struct p9_xos_kopen *kops, const char *name)
{
	struct p9_xos_device *dev = &driver.device[channel];
	int ret = 0;

	BUG_ON(kops == NULL);

	spin_lock(&dev->lock);
	if (dev->open)
		goto fail;

	dev->open = 1;
	dev->kops = kops;
	dev->wr_cb = kops->r_callback ? p9_xos_kern_wr_cb : NULL;
	dev->rd_cb = kops->t_callback ? p9_xos_kern_rd_cb : NULL;

	strncpy(dev->s_name, name, ARRAY_SIZE(dev->s_name) - 1);
	dev->s_name[ARRAY_SIZE(dev->s_name) - 1] = 0;

	ret = (u32) dev;

fail:
	spin_unlock(&dev->lock);

	return ret;
}
EXPORT_SYMBOL(p9_xosclient_kopen);

static void p9_xos_user_rd_cb(struct p9_xos_device *device,
			      struct p9_req_t *req)
{
	wake_up_interruptible(&device->t_wait);
}

static void p9_xos_user_wr_cb(struct p9_xos_device *device,
			      struct p9_req_t *req)
{
	kfree(req->rc);
	wake_up_interruptible(&device->r_wait);
}

static int p9_xosclient_open(struct inode *inode, struct file *instance)
{
	int channel = iminor(inode);
	struct p9_xos_device *dev = &driver.device[channel];
	int ret = 0;

	spin_lock(&dev->lock);
	if (dev->open && dev->kops) {
		ret = -ENODEV;
		goto fail;
	}
	snprintf(dev->s_name, ARRAY_SIZE(dev->s_name) - 1, "/dev/%s:\t%u",
		 DEVICE_NAME, channel);
	dev->s_name[ARRAY_SIZE(dev->s_name) - 1] = 0;

	init_waitqueue_head(&dev->t_wait);
	init_waitqueue_head(&dev->r_wait);
	dev->wr_cb = p9_xos_user_wr_cb;
	dev->rd_cb = p9_xos_user_rd_cb;
	instance->private_data = dev;
	dev->open += 1;

fail:
	spin_unlock(&dev->lock);

	return ret;
}

static int p9_xosclient_close(struct inode *inode, struct file *instance)
{
	struct p9_xos_device *dev =
	    (struct p9_xos_device *)instance->private_data;
	struct p9_req_t *req;

	BUG_ON(!dev->open);
	if (!--dev->open) {
		spin_lock(&dev->lock);
		while (!list_empty(&dev->req_list)) {
			warning("Channel releasing while req_list not empty! "
				"Replying RERROR to requesters.");
			req =
			    list_first_entry(&dev->req_list, struct p9_req_t,
					     req_list);
			list_del(&req->req_list);
			p9_xosclient_rerror(req);
		}
		spin_unlock(&dev->lock);
		wait_event_interruptible(dev->r_wait,
					 list_empty(&dev->req_list)); /*FIXME*/
		dev->rd_cb = NULL;
		dev->wr_cb = NULL;
	}

	return 0;
}

void p9_xosclient_krelease(struct p9_fcall *fcall)
{
	struct p9_xos_driver *drv = &driver;
	struct p9_xos_endpoint *ep = &drv->ep[RD_EP];
	unsigned long flags;

	spin_lock_irqsave(&drv->ep_lock, flags);
	p9_xos_deque_push(ep->rqueue, a2n(fcall->sdata - 8, ep), ep);
	nb_free_packets++;
	spin_unlock_irqrestore(&drv->ep_lock, flags);

	kfree(fcall);
}
EXPORT_SYMBOL(p9_xosclient_krelease);

struct p9_fcall *p9_xosclient_kread(unsigned long handle)
{
	struct p9_xos_device *dev = (struct p9_xos_device *)handle;
	struct p9_fcall *tc = NULL;

	spin_lock(&dev->lock);
	if (!list_empty(&dev->req_list)) {
		struct p9_req_t *req = list_first_entry(&dev->req_list,
							struct p9_req_t,
							req_list);
		list_del(&req->req_list);
		tc = req->tc;
		kmem_cache_free(dev->driver->cache, req);
		if (p9_parse_header(tc, &(tc->size), &(tc->id), &(tc->tag), 1))
			warning("failed to decode header !");
	}
	spin_unlock(&dev->lock);

	return tc;
}
EXPORT_SYMBOL(p9_xosclient_kread);

static ssize_t p9_xosclient_read(struct file *instance, char __user * data,
				 size_t count, loff_t *offset)
{
	struct p9_xos_device *dev =
	    (struct p9_xos_device *)instance->private_data;
	int ret;
	struct p9_req_t *req;
	struct p9_xos_endpoint *ep;
	unsigned long flags;

	if (wait_event_interruptible(dev->t_wait, !list_empty(&dev->req_list)))
		return -ERESTARTSYS;

	spin_lock(&dev->lock);
	req = list_first_entry(&dev->req_list, struct p9_req_t, req_list);
	ep = &dev->driver->ep[RD_EP];
	list_del(&req->req_list);
	spin_unlock(&dev->lock);

	ret = req->tc->size;
	if (count < ret) {
		warning("User buffer too small to copy complete request !");
		ret = count;
	}
	if (copy_to_user(data, req->tc->sdata, ret))
		ret = -EFAULT;

	spin_lock_irqsave(&dev->driver->ep_lock, flags);
	p9_xos_deque_push(ep->rqueue, a2n(req->tc->sdata - 8, ep), ep);
	nb_free_packets++;
	spin_unlock_irqrestore(&dev->driver->ep_lock, flags);

	kfree(req->tc);
	kmem_cache_free(dev->driver->cache, req);

	return ret;
}

static ssize_t p9_xosclient_write(struct file *instance,
				  const char __user *data, size_t count,
				  loff_t *offset)
{
	int retval;
	struct p9_xos_device *dev =
	    (struct p9_xos_device *)instance->private_data;
	struct p9_xos_driver *drv = dev->driver;
	struct p9_req_t *req = NULL;

	req = kmem_cache_alloc(drv->cache, GFP_KERNEL);
	if (!req) {
		warning("Cannot allocate 9p request !");
		retval = -ENOMEM;
		goto fail;
	}
	req->aux = dev;
	req->tc = NULL;
	req->rc = kmalloc(sizeof(struct p9_fcall) + count, GFP_KERNEL);
	if (!req->rc) {
		warning("Cannot allocate 9p fcall !");
		retval = -ENOMEM;
		goto fail;
	}
	req->rc->sdata = (u8 *) req->rc + sizeof(struct p9_fcall);
	req->rc->size = count;

	if (copy_from_user(req->rc->sdata, data, count)) {
		retval = -EFAULT;
		goto fail;
	}

	spin_lock(&dev->lock);
	dev->ack_count += 1;
	spin_unlock(&dev->lock);

	p9_xos_add_write_request(req, 0);

	retval = count;

done:
	return retval;

fail:
	if (req) {
		kfree(req->rc);
		kmem_cache_free(drv->cache, req);
	}
	goto done;
}

int p9_xosclient_kwrite(unsigned long handle, struct p9_fcall *rcall)
{
	struct p9_xos_device *dev = (struct p9_xos_device *)handle;
	struct p9_xos_driver *drv = dev->driver;
	struct p9_req_t *req;

	req = kmem_cache_alloc(drv->cache, GFP_KERNEL);
	if (!req) {
		warning("Slab allocator runs out of memory !");
		return -ENOMEM;
	}
	req->aux = dev;
	req->rc = rcall;
	req->tc = NULL;

	spin_lock(&dev->lock);
	dev->ack_count += 1;
	spin_unlock(&dev->lock);

	p9_xos_add_write_request(req, 0);

	return 0;
}
EXPORT_SYMBOL(p9_xosclient_kwrite);

static void p9_req_constructor(void *mem)
{
	struct p9_req_t *req = (struct p9_req_t *)mem;
	memset(req, 0, sizeof(struct p9_req_t));
	INIT_LIST_HEAD(&req->req_list);
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = p9_xosclient_open,
	.read = p9_xosclient_read,
	.write = p9_xosclient_write,
	.poll = p9_xosclient_poll,
	.release = p9_xosclient_close,
};

static struct proc_dir_entry *proc_xos;
static struct proc_dir_entry *proc_event;
static struct proc_dir_entry *proc_state;

static int p9_xos_proc_read_event(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	static const char *const p9_xos_type[] = {
		"Tversion",
		"Rversion",
		"Tauth   ",
		"Rauth   ",
		"Tattach ",
		"Rattach ",
		NULL,
		"Rerror  ",
		"Tflush  ",
		"Rflush  ",
		"Twalk   ",
		"Rwalk   ",
		"Topen   ",
		"Ropen   ",
		"Tcreate ",
		"Rcreate ",
		"Tread   ",
		"Rread   ",
		"Twrite  ",
		"Rwrite  ",
		"Tclunk  ",
		"Rclunk  ",
		"Tremove ",
		"Rremove ",
		"Tstat   ",
		"Rstat   ",
		"Twstat  ",
		"Rwstat  "
	};

	struct p9_xos_driver *driver = data;
	int len = count;
	unsigned limit;

	spin_lock(&driver->e_lock);

	if (unlikely(!offset))
		driver->e_skip = 1;

	if (unlikely(!driver->e_skip)) {
		*eof = 1;
		goto done;
	}

	limit = driver->e_full ? ARRAY_SIZE(driver->e_ring) : driver->e_curr;

	while (len > 80) {
		unsigned what, from;
		unsigned long long when;
		const char *who_c;
		const char *who_s;
		int n;

		if (unlikely(offset >= limit)) {
			driver->e_curr = 0;
			driver->e_full = 0;
			driver->e_skip = 0;
			*eof = 1;
			break;
		}

		if (driver->e_full) {
			unsigned i = offset + driver->e_curr;
			if (i >= ARRAY_SIZE(driver->e_ring))
				i -= ARRAY_SIZE(driver->e_ring);
			what = driver->e_ring[i].what;
			when = driver->e_ring[i].when;
			who_c = driver->device[driver->e_ring[i].who].c_name;
			who_s = driver->device[driver->e_ring[i].who].s_name;
			from = driver->e_ring[i].from;
		} else {
			what = driver->e_ring[offset].what;
			when = driver->e_ring[offset].when;
			who_c =
			    driver->device[driver->e_ring[offset].who].c_name;
			who_s =
			    driver->device[driver->e_ring[offset].who].s_name;
			from = driver->e_ring[offset].from;
		}

		switch (from) {
		case RD_EP:
			if (what & 1)
				n = sprintf(buf,
					    "%llu\tlinux client:\t%s\t%s\n",
					    when, who_c,
					    p9_xos_type[what - P9_TVERSION]);
			else
				n = sprintf(buf, "%llu\t%s\t%s\n", when, who_s,
					    p9_xos_type[what - P9_TVERSION]);
			break;
		case WR_EP:
			{
				if (what & 1)
					n = sprintf(buf, "%llu\t%s\t%s\n", when,
						    who_s,
						    p9_xos_type[what -
								P9_TVERSION]);
				else
					n = sprintf(buf, "%llu\tlinux client:"
						    "\t%s\t%s\n", when, who_c,
						    p9_xos_type[what -
								P9_TVERSION]);
			}
			break;
		default:
			n = sprintf(buf, "wrong data");
			break;
		}

		buf += n;
		len -= n;

		*start += 1;
		offset += 1;

	}

done:
	spin_unlock(&driver->e_lock);

	return count - len;
}

static int p9_xos_proc_read_state(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	struct p9_xos_driver *driver = data;
	int id = ARRAY_SIZE(driver->device), len;
	char server[32];
	char client[32];

	spin_lock(&driver->c_lock);
	len = sprintf(buf, "state:\t0x%lx\n", driver->state);
	len += sprintf(buf + len, "\nchannels:\n");

	while (id--) {
		struct p9_xos_device *device = &driver->device[id];

		if ((!device->client) && (!device->open))
			continue;

		if (!device->client)
			sprintf(client, "none");
		else
			strcpy(client, device->c_name);

		if (!device->open)
			sprintf(server, "none");
		else {
			if (!device->kops)
				sprintf(server, "/dev/%s", DEVICE_NAME);
			else
				strcpy(server, device->s_name);
		}

		len +=
		    sprintf(buf + len, "channel: %3d\tclient: %s\tserver: %s\n",
			    id, client, server);
	}
	spin_unlock(&driver->c_lock);

	*eof = 1;
	return len;
}

static struct class *ex_class;
static dev_t dev_num;
static struct cdev cdev;

static int __init p9_xos_initialize(void)
{
	int retval;
	unsigned int i;

	prolog("");

	info("installing 9P2000 inter-OS transport");

	driver.e_curr = 0;
	driver.e_full = 0;
	driver.e_skip = 0;

	proc_xos = proc_mkdir("p9_xos", NULL);
	if (proc_xos) {
		proc_event =
		    create_proc_read_entry("log_9P_activity", 0, proc_xos,
					   p9_xos_proc_read_event, &driver);
		proc_state =
		    create_proc_read_entry("state", 0, proc_xos,
					   p9_xos_proc_read_state, &driver);
	}

	spin_lock_init(&driver.q_lock);
	spin_lock_init(&driver.e_lock);
	INIT_LIST_HEAD(&driver.req_list);

	INIT_WORK(&driver.wwork, p9_xos_write_work);
	INIT_WORK(&driver.rwork, p9_xos_read_work);
	INIT_WORK(&driver.swork, p9_xos_sync_work);

	init_timer(&driver.timer);
	driver.timer.data = (unsigned long int)&driver;
	driver.timer.function = p9_xos_start_write_pump;

	driver.state = 0;
	/* real-time single-threaded workqueue */
	driver.workqueue = __create_workqueue("p9_xos", 1, 0, 1);
	if (driver.workqueue == NULL) {
		critical("cannot create workqueue");
		return -ENOMEM;
		goto bail_out;
	}

	spin_lock_init(&driver.c_lock);
	spin_lock_init(&driver.ep_lock);
	wake_lock_init(&driver.wake_lock, WAKE_LOCK_SUSPEND, "9P_transport");
	driver.wake_count = 0;
	driver.wake_status = 0;

	i = ARRAY_SIZE(driver.device);
	while (i--) {
		struct p9_xos_device *device = &driver.device[i];
		device->id = i;
		device->client = NULL;
		device->driver = &driver;
		INIT_LIST_HEAD(&device->req_list);
		spin_lock_init(&device->lock);
		device->wr_cb = NULL;
		device->rd_cb = NULL;
		device->kops = NULL;
	}

	driver.cache = kmem_cache_create("p9_xos_cache",
					 sizeof(struct p9_req_t),
					 0, 0, p9_req_constructor);

	driver.ctrl = xos_ctrl_connect("styxcsrv", 0);
	if (!driver.ctrl) {
		critical("cannot connect inter-OS resource styxcsrv");
		retval = -ENODEV;
		goto bail_out;
	}

	for (i = 0; i < ARRAY_SIZE(driver.ep); i += 1) {
		struct p9_xos_endpoint *ep;
		xos_area_handle_t xos_area;
		char name[4];

		snprintf(name, sizeof(name), "9p%d", i);
		xos_area = xos_area_connect(name, 0);
		if (!xos_area) {
			critical("cannot connect inter-OS resource 9p%d", i);
			retval = -ENODEV;
			goto bail_out;
		}

		ep = &driver.ep[i];
		ep->regs = xos_area_ptr(xos_area);
		if (!ep->regs) {
			retval = -ENOMEM;
			goto bail_out;
		}

		retval = p9_xos_chunks_array_init(ep, i == WR_EP);
		if (retval)
			goto bail_out;

		xos_ctrl_register(driver.ctrl, i, p9_xos_init, &driver, 0);
		xos_ctrl_raise(driver.ctrl, i);
	}

	p9_xos_trans_module.maxsize = (1 << driver.ep[0].regs[SIZE]) - 8;
	v9fs_register_trans(&p9_xos_trans_module);

	/* 9P channels allocation */
	retval = alloc_chrdev_region(&dev_num, 0, NB_CHANNEL, DEVICE_NAME);
	if (retval < 0) {
		printk(KERN_WARNING "cannot register my device\n");
		return retval;
	}
	ex_class = class_create(THIS_MODULE, DEVICE_NAME);
	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;
	if (cdev_add(&cdev, dev_num, NB_CHANNEL)) {
		printk(KERN_WARNING "Bad cdev\n");
		return 1;
	}
	for (i = 0; i < NB_CHANNEL; i++)
		device_create(ex_class, NULL, (dev_num | i), NULL,
			      DEVICE_NAME "%d", i);

	retval = 0;

	xos_ctrl_raise(driver.ctrl, P9_XOS_EVENT_INIT);

bail_out:
	epilog("%d", retval);

	return retval;
}

static void __exit p9_xos_finalize(void)
{
	if (proc_xos) {
		if (proc_state)
			remove_proc_entry("state", proc_xos);

		if (proc_event)
			remove_proc_entry("log_9P_activity", proc_xos);

		remove_proc_entry("p9_xos", NULL);
	}

	v9fs_unregister_trans(&p9_xos_trans_module);

	if (driver.ctrl)
		xos_ctrl_unregister(driver.ctrl, P9_XOS_EVENT);

	if (driver.workqueue != NULL)
		destroy_workqueue(driver.workqueue);
}

module_init(p9_xos_initialize);
module_exit(p9_xos_finalize);

MODULE_AUTHOR("Arnaud Troel, Michel Jaouen, Maxime Coquelin, Damien Arcuset"
	      "<{arnaud.troel,michel.jaouen,"
	      "maxime.coquelin-nonst,damien.arcuset-nonst}"
	      "@stericsson.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(P9_XOS_VERSION);
MODULE_DESCRIPTION("9P2000 Inter-OS transport");
