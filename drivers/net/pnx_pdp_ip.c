#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <asm/atomic.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#define DRIVER_NAME "pnx_pdp_ip"

#define critical(fmt, arg...)					\
	printk(KERN_ERR "%s: " fmt "\n", DRIVER_NAME, ## arg)

#define warning(fmt, arg...)					\
	printk(KERN_WARNING "%s: " fmt "\n", DRIVER_NAME, ## arg)

#define info(fmt, arg...)					\
	printk(KERN_INFO "%s: " fmt "\n", DRIVER_NAME, ## arg)

struct pnx_pdp_ip_req {
	struct list_head list;
	int tag;
	struct sk_buff *skb;
	struct net_device *dev;
};

struct pnx_pdp_ip_ext {
	struct pnx_pdp_ip *drv;
	struct p9_fid *fid;
	struct net_device_stats stats;
	atomic_t refcount;
	struct pnx_pdp_ip_req tread[32];
	unsigned short int index;
	unsigned short int should_stop;
	wait_queue_head_t wq;
};

struct pnx_pdp_ip {
	struct semaphore sem;
	struct p9_client *client;
	struct p9_fid *fid;
	struct work_struct work;
	struct sk_buff_head queue;
	struct list_head list_twr;
	struct pnx_pdp_ip_req twrite[32];
	struct net_device *dev[4];
	struct workqueue_struct *workqueue;
};

static void pnx_pdp_ip_incref(struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);

	atomic_inc(&ext->refcount);
}

static void pnx_pdp_ip_decref(struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);

	if (atomic_dec_and_test(&ext->refcount))
		wake_up(&ext->wq);
}

static int pnx_pdp_ip_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);
	struct pnx_pdp_ip *drv = ext->drv;

	skb_set_queue_mapping(skb, ext->index);

	pnx_pdp_ip_incref(dev);

	spin_lock(&drv->queue.lock);
	__skb_queue_tail(&drv->queue, skb);
 	spin_unlock(&drv->queue.lock);

	queue_work(drv->workqueue, &drv->work);

	return NETDEV_TX_OK;
}

static struct net_device_stats *pnx_pdp_ip_get_stats(struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);

	return &ext->stats;
}

static void pnx_pdp_ip_write_cb(int error, u32 count, void *data, void *cookie)
{
	struct pnx_pdp_ip_req *req = cookie;
	struct net_device *dev = req->dev;
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);
	struct pnx_pdp_ip *drv = ext->drv;
	int i;

	if (unlikely(error))
		ext->stats.tx_errors += 1;
	else {
		ext->stats.tx_packets += 1;
		ext->stats.tx_bytes += req->skb->len;
	}

	dev_kfree_skb(req->skb);

	spin_lock(drv->queue.lock);
	list_add_tail(&req->list, &drv->list_twr);
	spin_unlock(drv->queue.lock);

	pnx_pdp_ip_decref(dev);

	spin_lock(drv->queue.lock);
	if (list_is_singular(&drv->list_twr)) {
		if (likely(!ext->should_stop))
		for (i = 0; i < ARRAY_SIZE(ext->drv->dev); i += 1)
			netif_wake_queue(drv->dev[i]);
		if (!skb_queue_empty(&drv->queue))
			queue_work(drv->workqueue, &drv->work);
	}
	spin_unlock(drv->queue.lock);
}

static void pnx_pdp_ip_work(struct work_struct *work)
{
	struct pnx_pdp_ip *drv = container_of(work, struct pnx_pdp_ip, work);

	while (!skb_queue_empty(&drv->queue)) {
		struct sk_buff *skb;
		struct pnx_pdp_ip_req *req;
		struct pnx_pdp_ip_ext *ext;
		int error, index, i;


		spin_lock(&drv->queue.lock);
		if (list_empty(&drv->list_twr)) {
			for (i = 0; i < ARRAY_SIZE(drv->dev); i += 1)
				netif_stop_queue(drv->dev[i]);
			spin_unlock(&drv->queue.lock);
			break;
		}
		skb = __skb_dequeue(&drv->queue);
		spin_unlock(&drv->queue.lock);

		index = skb_get_queue_mapping(skb);
		ext = netdev_priv(drv->dev[index]);

		spin_lock(&drv->queue.lock);
		req = list_first_entry(&drv->list_twr, struct pnx_pdp_ip_req, list);
		list_del(&req->list);
		spin_unlock(&drv->queue.lock);

		req->skb = skb;
		req->dev = drv->dev[index];

		error = p9_client_post_aio(ext->fid, req->tag, 0, skb->len,
					   skb->data, pnx_pdp_ip_write_cb, req);
		BUG_ON(error);
	}
}

static void pnx_pdp_ip_read_cb(int error, u32 count, void *data, void *cookie)
{
	struct pnx_pdp_ip_req *req = cookie;
	struct net_device *dev = req->dev;
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);
	struct sk_buff *skb;

	if (error)
		goto job_done;

	if (!count)
		goto job_done;

	skb = netdev_alloc_skb(dev, count);
	if (unlikely(skb == NULL)) {
		ext->stats.rx_dropped += 1;
		goto job_done;
	}

	skb->dev = dev;
	memcpy(skb_put(skb, count), data, count);
	skb_reset_mac_header(skb);
	skb->protocol = htons(ETH_P_IP);
	netif_rx_ni(skb);

	ext->stats.rx_packets += 1;
	ext->stats.rx_bytes += count;

job_done:
	if (likely(!ext->should_stop)) {
		error = p9_client_post_aio(ext->fid, req->tag, 0, dev->mtu,
					   NULL, pnx_pdp_ip_read_cb, req);
		BUG_ON(error);
	} else {
		p9_client_destroy_aio(ext->fid, req->tag);
		pnx_pdp_ip_decref(req->dev);
	}
}

static int pnx_pdp_ip_lazy_init(struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);
	struct pnx_pdp_ip *drv = ext->drv;
	int retval = 0;

	if (down_interruptible(&drv->sem))
		return -ERESTARTSYS;

	if (unlikely(drv->workqueue == NULL)) {
	drv->workqueue = __create_workqueue(DRIVER_NAME, 1, 0, 1);
	if (drv->workqueue == NULL) {
		retval = -ENOMEM;
		critical("cannot create_workqueue");
		goto bail_out;
	}
	}

	if (unlikely(drv->client == NULL)) {
		drv->client = p9_client_create(DRIVER_NAME, "trans=xoscore,"
					       "noextend,latency=20");
		if (IS_ERR(drv->client)) {
			retval = PTR_ERR(drv->client);
			drv->client = NULL;
			critical("cannot create 9P client");
		}
	}

bail_out:
	up(&drv->sem);

	return retval;
}

static int pnx_pdp_ip_open(struct net_device *dev)
{

	unsigned int i;
	int retval;
	char *path[] = { "ip", "255" };
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);
	struct pnx_pdp_ip *drv = ext->drv;

	retval = pnx_pdp_ip_lazy_init(dev);
	if (retval)
		goto bail_out;

	if (unlikely(drv->fid == NULL)) {
		struct p9_fid *fid;
		fid = p9_client_attach(drv->client, NULL, "nobody", 0, "");
		if (IS_ERR(fid)) {
			retval = PTR_ERR(fid);
			critical("cannot attach remote 9P file system");
			goto bail_out;
		}
		drv->fid = fid;
				
		INIT_LIST_HEAD(&drv->list_twr);
		for (i = 0; i < ARRAY_SIZE(drv->twrite); i += 1) {
			drv->twrite[i].dev = NULL;
			drv->twrite[i].skb = NULL;
			drv->twrite[i].tag =  p9_client_create_aio(drv->fid);
			INIT_LIST_HEAD(&drv->twrite[i].list);
			list_add_tail(&drv->twrite[i].list, &drv->list_twr);
			BUG_ON(drv->twrite[i].tag < 0);
		}
	}

	snprintf(path[1], 3, "%u", ext->index);
	ext->fid = p9_client_walk(drv->fid, ARRAY_SIZE(path), path, 1);
	if (IS_ERR(ext->fid)) {
		retval = PTR_ERR(ext->fid);
		ext->fid = NULL;
		critical("cannot walk remote 9P file /%s/%s", path[0], path[1]);
		goto bail_out;
	}

	retval = p9_client_open(ext->fid, P9_ORDWR);
	if (retval) {
		p9_client_clunk(ext->fid);
		ext->fid = NULL;
		critical("cannot open remote 9P file /%s/%s", path[0], path[1]);
		goto bail_out;
	}

	dev->mtu = ext->fid->iounit;
	if (!dev->mtu)
		dev->mtu = drv->client->msize - P9_IOHDRSZ;

	ext->should_stop = 0;
	init_waitqueue_head(&ext->wq);

	for (i = 0; i < ARRAY_SIZE(ext->tread); i += 1) {
		ext->tread[i].dev = dev;
		ext->tread[i].skb = NULL;
		ext->tread[i].tag = p9_client_create_aio(ext->fid);
		if (ext->tread[i].tag < 0)
			break;
	}

	if (unlikely(i < ARRAY_SIZE(ext->tread))) {
		while (i--)
			p9_client_destroy_aio(ext->fid, ext->tread[i].tag);
		retval = -ENOMEM;
		goto bail_out;
	}

	atomic_set(&ext->refcount, ARRAY_SIZE(ext->tread));
	for (i = 0; i < ARRAY_SIZE(ext->tread); i += 1)
		pnx_pdp_ip_read_cb(0, 0, 0, &ext->tread[i]);

	netif_start_queue(dev);

bail_out:
	return retval;
}

static int pnx_pdp_ip_close(struct net_device *dev)
{
	int ret = 0;
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);

	netif_stop_queue(dev);

	ext->should_stop = 1;

	if (ext->fid == NULL)
		goto job_done;

	p9_client_clunk(ext->fid);

	ret = wait_event_interruptible(ext->wq, !atomic_read(&ext->refcount));

	ext->fid = NULL;

job_done:
	return ret;
}

static void pnx_pdp_ip_setup(struct net_device *dev)
{
	struct pnx_pdp_ip_ext *ext = netdev_priv(dev);

	dev->destructor      = free_netdev;
	dev->hard_header_len = 0;
	dev->addr_len        = 0;
	dev->tx_queue_len    = 500;
	dev->mtu             = 0;
	dev->flags           = IFF_NOARP|IFF_POINTOPOINT;

	dev->open            = pnx_pdp_ip_open;
	dev->stop            = pnx_pdp_ip_close;
	dev->get_stats       = pnx_pdp_ip_get_stats;
	dev->hard_start_xmit = pnx_pdp_ip_xmit;

	memset(&ext->stats, 0, sizeof(ext->stats));
}

static struct pnx_pdp_ip drv;

static int __init pnx_pdp_ip_init(void)
{
	unsigned int i;
	struct pnx_pdp_ip_ext *ext;
	char name[16];
	int err;

	info("installing 9P2000 inter-OS network interface");

	drv.client = NULL;
	drv.fid = NULL;
	drv.workqueue = 0;
	init_MUTEX(&drv.sem);

	skb_queue_head_init(&drv.queue);
	INIT_WORK(&drv.work, pnx_pdp_ip_work);

	for (i = 0; i < ARRAY_SIZE(drv.dev); i += 1) {
		snprintf(name, sizeof(name), "pdp_ip%u", i);
		drv.dev[i] = alloc_netdev(sizeof(struct pnx_pdp_ip_ext), name,
					  pnx_pdp_ip_setup);
		if (drv.dev[i] == NULL) {
			warning("cannot allocate network device");
			continue;
		}

		ext = netdev_priv(drv.dev[i]);
		ext->drv = &drv;
		ext->index = i;

		err = register_netdev(drv.dev[i]);
		if (err) {
			warning("cannot register network device (got %d)", err);
			free_netdev(drv.dev[i]);
			drv.dev[i] = NULL;
			continue;
		}
	}

	return 0;
}

static void __exit pnx_pdp_ip_exit(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(drv.dev); i += 1)
		if (drv.dev[i])
			unregister_netdev(drv.dev[i]);

	if (drv.fid)
		p9_client_clunk(drv.fid);

	if (drv.client)
		p9_client_destroy(drv.client);

	if (drv.workqueue)
		destroy_workqueue(drv.workqueue);
}

module_init(pnx_pdp_ip_init);
module_exit(pnx_pdp_ip_exit);

MODULE_AUTHOR("Arnaud Troel - Copyright (C) 2010 ST-Ericsson");
MODULE_DESCRIPTION("Linux IP over 9P inter-OS file system");
MODULE_LICENSE("GPL");
