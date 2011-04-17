/*
 * drivers/spi/gns7560.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Michel JAOUEN <michel.jaouen@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */
#include <linux/module.h>
#include <linux/kernel.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include "../../net/9p/protocol.h"
#include <net/9p/trans_xosclient.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/gns7560.h>
#include <linux/io.h>
#include <mach/gpio.h>
#include <mach/dma.h>
#include "spi_pnx67xx.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JAOUEN Michel, STEWIRELESS");
MODULE_DESCRIPTION("gps acces nano 9p server for Modem");

#define MODULE_NAME  "gns7560"

/* modules depends on device gns7560 spi */
#if 0
#define DBG(format, arg...) printk(KERN_ALERT \
	"%s - " format "\n", __func__, ## arg)
#define ERR(format, arg...) printk(KERN_ERR \
	"%s - " format "\n", __func__, ## arg)

#else
#define ERR(format, arg...) printk(KERN_ERR \
	"%s - " format "\n", __func__, ## arg)

#define DBG(fmt...) do { } while (0)
#endif

static struct spi_device *gns7560_spi;
static struct gps_pdata *gps_data;
/* hard coded 9P client */
#define GPSLINK 4
#define GPSCTRL 5

/* define for ctrl command */
#define GPS_P_ON 0x11
#define GPS_P_OFF 0x22
#define GPS_R_ON 0x33
#define GPS_R_OFF 0x44

#define MAX_SIZE  50



static unsigned long gps_link_handle;
static unsigned long gps_ctrl_handle;
static struct work_struct v_gpsctrl_wq;
static struct work_struct v_gpslink_wq;
static struct work_struct v_gpswrite_wq;
static struct work_struct v_gpsread_wq;


/**
 * 9P services R/W callbacks
 */

static void gps_wr_cb(struct p9_fcall *ofcall)
{
	kfree(ofcall);
}

static void gps_rd_cb(void *cookie)
{
	schedule_work((struct work_struct *)cookie);
}


/**
 * @brief toggle gpios according to ctrl value
 *
 * @param ctrl
 */
static  void gns7560_ctrl(u8 ctrl)
{
	if (!gps_data) {
		ERR("access without gpio configured\n");
		return;
	}
	switch (ctrl) {
	case GPS_P_ON:
		{
			DBG("GPS_POWER_ON");
			gpio_direction_output(gps_data->gpo_pwr, 1);
			break;
		}
	case GPS_P_OFF:
		{
			DBG("GPS_POWER_OFF");
			gpio_direction_output(gps_data->gpo_pwr, 0);
			gpio_direction_output(gps_data->gpo_rst, 0);
			break;
		}

	case GPS_R_ON:
		{
			DBG("GPS_RESET_ON");
			gpio_direction_output(gps_data->gpo_rst, 1);
			break;
		}

	case GPS_R_OFF:
		{
			DBG("GPS_RESET_OFF");
			gpio_direction_output(gps_data->gpo_rst, 0);
			break;
		}
	default:
		ERR("Undefined Command");
	}
}
#define MAXTRANSACTION 64

struct gps_link_cb {
struct p9_fcall *ifcall;
struct p9_fcall *ofcall;
/* a maximum of 10 is widely enough */
struct spi_transfer	t ;
struct spi_transfer tlist[MAXTRANSACTION];
struct spi_message	m;
int remaining;
unsigned char *cur;
};

static struct gps_link_cb spi_cb;
int txpacket = DMA_TR_SIZE_MAX;
int rxpacket = DMA_TR_SIZE_MAX;
#define MAX_BYTE_RX rxpacket
#define MAX_BYTE_TX txpacket
/**
 * @brief call back executed afte spi transaction complete
 *
 * @param arg
 */
static void gps_write_cb(void *arg)
{
	schedule_work(&v_gpswrite_wq);
}


/**
 * @brief call back executed after spi transaction complete
 *
 * @param arg
 */
static void gps_read_cb(void *arg)
{
	schedule_work(&v_gpsread_wq);
}



/**
 * @brief
 */
static void gps_queue_read(void)
{
	int size = 0;
	int i = 0;
	if (spi_cb.remaining == 0)
		return;
	spi_message_init(&spi_cb. m);
	spi_cb.m.complete = gps_read_cb;
	spi_cb.m.context = &spi_cb;

	while (spi_cb.remaining) {
		if (spi_cb.remaining > MAX_BYTE_RX)
			size = MAX_BYTE_RX;
		else
			size = spi_cb.remaining;
		spi_cb.remaining = spi_cb.remaining-size;
		spi_cb.tlist[i].rx_buf = spi_cb.cur;
		spi_cb.cur += size;
		spi_cb.tlist[i].tx_buf = NULL;
		spi_cb.tlist[i].len = size;
		spi_message_add_tail(&spi_cb.tlist[i], &spi_cb.m);
		i++;
		if (i > MAXTRANSACTION)
			break;
	}
	/* +LMSqc41716 : CS is deasserted at last transfer */
	spi_cb.tlist[i-1].cs_change = 1;
	/* -LMSqc41716 */
	spi_async(gns7560_spi, &spi_cb.m);
	DBG("req access read %d\n", size);

}

/**
 * @brief
 */
static void gps_queue_write(void)
{
	int size = 0;
	int i = 0;
	if (spi_cb.remaining == 0)
		return;

	spi_message_init(&spi_cb.m);
	spi_cb.m.complete = gps_write_cb;
	spi_cb.m.context = &spi_cb;

	while (spi_cb.remaining) {
		if (spi_cb.remaining > MAX_BYTE_TX)
			size = MAX_BYTE_TX;
		else
			size = spi_cb.remaining;
		spi_cb.remaining = spi_cb.remaining-size;
		spi_cb.tlist[i].tx_buf = spi_cb.cur;
		spi_cb.cur += size;
		spi_cb.tlist[i].rx_buf = NULL;
		spi_cb.tlist[i].len = size;
		spi_message_add_tail(&spi_cb.tlist[i], &spi_cb.m);
		i++;
		if (i > MAXTRANSACTION)
			break;
	}
	/* +LMSqc41716 : CS is deasserted at last transfer */
        spi_cb.tlist[i-1].cs_change = 1;
	/* -LMSqc41716 */
	spi_async(gns7560_spi, &spi_cb.m);
	DBG("req access write %d\n", size);
}


/**
 * @brief
 *
 * @param arg
 */
static void gps_read_wq(struct work_struct *work)
{
	struct gps_link_cb *p = (struct gps_link_cb *)&spi_cb;
	if (p->remaining == 0) {
		p9pdu_finalize(p->ofcall);
		p9_xosclient_kwrite(gps_link_handle, p->ofcall);
		DBG("read done");
	} else
		gps_queue_read();
}

/**
 * @brief
 *
 * @param arg
 */
static void gps_write_wq(struct work_struct *work)
{
	struct gps_link_cb *p = (struct gps_link_cb *)&spi_cb;
	if (p->remaining == 0) {
		p9_xosclient_krelease(p->ifcall);
		p9_xosclient_kwrite(gps_link_handle, p->ofcall);
		DBG("write done");
	} else
		gps_queue_write();
}

/**
 * @brief handle all
 *
 * @param work
 */
static void gps_link_handler(struct work_struct *work)
{
	u16 tag;
	struct p9_fcall *ifcall, *ofcall;
	unsigned long len;
	u32 count, tmp1;
	u64 offset;

	ifcall = p9_xosclient_kread(gps_link_handle);
	if (!gns7560_spi)
		ERR("access without spi configured\n");
	if (ifcall) {
		p9_parse_header(ifcall, NULL, NULL, NULL, 0);
		switch (ifcall->id) {
		case P9_TREAD:
			p9pdu_readf(ifcall, 0, "dqd", &tmp1, &offset, &count);
			len = count;
			tag = ifcall->tag;
			DBG("tread access %ld\n", len);
			/* initialise a rread response */
			ofcall = kzalloc(sizeof(struct p9_fcall)+11+len,
					GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->sdata = (u8 *)ofcall+sizeof(struct p9_fcall);
			ofcall->tag = tag;
			ofcall->id = P9_RREAD;
			ofcall->capacity = 11+len;
			p9pdu_writef(ofcall, 0, "dbwd", 0, P9_RREAD, tag, len);
			ofcall->size = ofcall->capacity;

			/* we can release the tread */
			p9_xosclient_krelease(ifcall);
			/* execute the request to spi*/
			spi_cb.remaining = len;
			spi_cb.ofcall = ofcall;
			spi_cb.cur = (u8 *)ofcall->sdata + 11;
			gps_queue_read();
			break;

		case P9_TWRITE:
			p9pdu_readf(ifcall, 0, "dqd", &tmp1, &offset, &count);
			len = count;
			/* len = ifcall->params.tread.count;*/
			DBG("twrite access %ld\n", len);
			tag = ifcall->tag;
			ofcall = kzalloc(sizeof(struct p9_fcall)+11
					, GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->tag = tag;
			ofcall->id = P9_RWRITE;
			ofcall->capacity = 11;
			ofcall->sdata = (u8 *)ofcall+sizeof(struct p9_fcall);
			p9pdu_writef(ofcall, 0, "dbwd"
					, 0, P9_RWRITE, tag, count);
			p9pdu_finalize(ofcall);

			spi_cb.ofcall = ofcall;
			spi_cb.ifcall = ifcall;
			spi_cb.cur = (u8 *)ifcall->sdata + 23;
			spi_cb.remaining = len;
			gps_queue_write();
			break;
		default:
			break;
		}
	}
}

static void gps_ctrl_handler(struct work_struct *work)
{
	struct p9_fcall *ifcall, *ofcall;
	u32 count, tmp1;
	u64 offset;
	unsigned long tag;
	unsigned char ctrl;
	unsigned char *data;
	DBG("access\n");
	ifcall = p9_xosclient_kread(gps_ctrl_handle);
	if (ifcall) {
		p9_parse_header(ifcall, NULL, NULL, NULL, 0);

		if (ifcall->id == P9_TWRITE) {
			p9pdu_readf(ifcall, 0, "dqD", &tmp1
					, &offset, &count, &data);
			if (count == 1) {
				DBG("access\n");
				tag = ifcall->tag;
				/* retrieve ctrl command */
				ctrl = *data;
				p9_xosclient_krelease(ifcall);
				gns7560_ctrl(ctrl);
				ofcall = kmalloc(sizeof(struct p9_fcall)+11,
						GFP_KERNEL);
				p9pdu_reset(ofcall);
				ofcall->tag = tag;
				ofcall->id = P9_RWRITE;
				ofcall->capacity = 11;
				ofcall->sdata
					= (u8 *)ofcall+sizeof(struct p9_fcall);
				p9pdu_writef(ofcall, 0
				, "dbwd", 0, P9_RWRITE, tag, count);
				p9pdu_finalize(ofcall);
				p9_xosclient_kwrite(gps_ctrl_handle, ofcall);
				DBG("access done\n");

			} else {
				DBG("wrong access\n");
				tag = ifcall->tag;
				p9_xosclient_krelease(ifcall);
				ofcall = kmalloc(sizeof(struct p9_fcall)+
					MAX_SIZE, GFP_KERNEL);
				p9pdu_reset(ofcall);
				ofcall->sdata = (u8 *)((u8 *)ofcall+
						sizeof(struct p9_fcall));
				ofcall->capacity = MAX_SIZE;
				p9pdu_writef(ofcall, 0, "dbwT"
					, 0, P9_RERROR, tag, "Unknown Request");
				p9pdu_finalize(ofcall);
				p9_xosclient_kwrite(gps_ctrl_handle, ofcall);

			}
		} else {
			/* unsupported command response rerror */
			DBG("wrong access\n");
			tag = ifcall->tag;
			p9_xosclient_krelease(ifcall);
			ofcall = kmalloc(
				sizeof(struct p9_fcall)+MAX_SIZE, GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->sdata = (u8 *)
				((u8 *)ofcall+sizeof(struct p9_fcall));
			ofcall->capacity = MAX_SIZE;
			p9pdu_writef(ofcall,
			0, "dbwT", 0, P9_RERROR, tag, "Unknown Request");
			p9pdu_finalize(ofcall);
			p9_xosclient_kwrite(gps_ctrl_handle, ofcall);
		}
	}
}

struct p9_xos_kopen gps_link_server = {
&gps_wr_cb, NULL, &gps_rd_cb, &v_gpslink_wq};
struct p9_xos_kopen gps_ctrl_server = {
&gps_wr_cb, NULL, &gps_rd_cb, &v_gpsctrl_wq};


static int __devinit gns7560_plat_probe(struct spi_device *spi)
{
	int rv;
	gps_data = spi->dev.platform_data;
	gns7560_spi = spi;
	DBG("starting\n");
	rv = gpio_request(gps_data->gpo_rst, MODULE_NAME);
	if (rv)
		goto fail;
	pnx_gpio_set_mode_gpio(gps_data->gpo_rst);
	gpio_direction_output(gps_data->gpo_rst, 0);
	rv = gpio_request(gps_data->gpo_pwr, MODULE_NAME);
	if (rv)
		goto failed;
	pnx_gpio_set_mode_gpio(gps_data->gpo_pwr);
	gpio_direction_output(gps_data->gpo_pwr, 0);
	/* switch off componet */
	gns7560_ctrl(GPS_P_OFF);
	/* set up spi */
	if (spi_setup(spi) < 0) {
		ERR("SPI SETUP FAILED\n");
		goto fail2;
	}
	DBG("done ok\n");
	return 0;

fail2:
	ERR("fail2\n");
	gpio_free(gps_data->gpo_pwr);
failed:
	ERR("failed\n");
	gpio_free(gps_data->gpo_rst);

fail:
	return -1;
}

static int __devexit gns7560_plat_remove(struct spi_device *spi)
{
	gpio_free(gps_data->gpo_pwr);
	gpio_free(gps_data->gpo_rst);
	gps_data = NULL;
	return 0;
}

static struct spi_driver gns7560_plat_driver = {
	.probe	= gns7560_plat_probe,
	.remove	= __devexit_p(gns7560_plat_remove),
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MODULE_NAME,
	},
};



static int __init gns7560_init(void)
{
	/* spi initialization */
	if (spi_register_driver(&gns7560_plat_driver))
		goto fail;
	/* 9p initialization */
	/* init the 2 possible work queue */
	INIT_WORK(&v_gpslink_wq, gps_link_handler);
	INIT_WORK(&v_gpsctrl_wq, gps_ctrl_handler);
	INIT_WORK(&v_gpsread_wq, gps_read_wq);
	INIT_WORK(&v_gpswrite_wq, gps_write_wq);
	/* connect to the 9p channel dedicated to pmu service */
	gps_link_handle = p9_xosclient_kopen(GPSLINK,
			&gps_link_server, "spi_link");
	if (gps_link_handle == 0)
		goto fail;
	gps_ctrl_handle =
		p9_xosclient_kopen(GPSCTRL, &gps_ctrl_server, "spi_ctrl");
	if (gps_ctrl_handle == 0)
		goto fail;
	DBG("init ok\n");
	return 0;
fail:
	ERR("init failed \n");
	/* fix me  no specific treatment yet*/
	return 0;
}

/**
 * @brief Module is supported only built-in
 *
 * @return
 */
static void __exit gns7560_exit(void)
{
/* close 9p client */
/* fix me not supported yet */


}

late_initcall(gns7560_init);
module_exit(gns7560_exit);




