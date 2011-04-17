/*
 * ============================================================================
 *
 *       Filename:  pmu9p.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/19/2008 02:14:19 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  michel JAOUEN
 *        Company:  
 *
 * ============================================================================
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include "../../../net/9p/protocol.h"
#include <net/9p/trans_xosclient.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("JAOUEN Michel, STNWIRELESS");
MODULE_DESCRIPTION("9p like PMU server for Modem");
/* modules depends on device pcf50626srv.c */
/* This device is connected to i2c */
#define CLIENT_REG 2
#define CLIENT_INT 1
static unsigned long pmu_reg_handle;
static unsigned long pmu_int_handle;
static struct work_struct v_pmuint_wq;
static struct work_struct v_pmuhandle_wq;
struct workqueue_struct *pmu9p_wq;
int race=0;
#define MAX_SIZE 100 
/** 
 * @brief this function is called in the context of a cross interrupt
 * Therefore function creates a bottom half which executes the process inside 
 *
 * @param event 
 * @param cookie 
 */
static void pmu_acc( void * cookie)
{
	if(!queue_work(pmu9p_wq, &v_pmuhandle_wq)) {
	race++;
	}	
}

/** 
 * @brief this function is called in the context of a cross interrupt
 * Therefore function creates a bottom half which executes the process inside 
 *
 * @param event 
 * @param cookie 
 */
static void pmu_int( void * cookie)
{
	if (!queue_work(pmu9p_wq, &v_pmuint_wq))
		race++;
}


static void pmu_wr_cb(struct p9_fcall *ofcall)
{
	kfree(ofcall);
}


extern void pcf5062x_bb_write(unsigned char addr, unsigned char *src, unsigned char len);
extern void pcf5062x_bb_read(unsigned char addr, unsigned char *src, unsigned char len);
typedef void (*irquser_t)(unsigned long );


extern void pcf5062x_bb_int(irquser_t callback,unsigned long cookie);

/** 
 * @brief handle all
 * 
 * @param work 
 */
static void pmu_acc_handler(struct work_struct *work)
{	
	u32 count,tmp1;
	u16 tag;
	struct p9_fcall *ifcall,*ofcall;
	u64 offset;
	unsigned char addr;
	unsigned char len;
	unsigned char tmp[MAX_SIZE],*data;

	/* we recopy the 9p request inside tmp which is on the stack of bottomhalf */
	/* fix me this copy must be avoid in or to do so */
	/* interface must be changed */
	ifcall=p9_xosclient_kread(pmu_reg_handle);
	if (ifcall)
	{
		/* LPA test returned error */
		p9_parse_header(ifcall, NULL, NULL, NULL, 0);

		switch (ifcall->id)
		{
		case P9_TREAD:
            p9pdu_readf(ifcall,0,"dqd",&tmp1,&offset,&count);
			len = (unsigned char )count;
			addr= (unsigned char )offset;
            tag= ifcall->tag;
			/* we can release the tread */				
			p9_xosclient_krelease(ifcall);
			/* execute the request to pmu */
			pcf5062x_bb_read(addr,tmp,len);		 
			/* initialise a rread response */
			ofcall=kmalloc(sizeof(struct p9_fcall)+MAX_SIZE,GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->sdata=(u8*)ofcall+sizeof(struct p9_fcall);
			ofcall->tag=tag;
			ofcall->id=P9_RREAD;
			ofcall->capacity=sizeof(struct p9_fcall)+MAX_SIZE;
			p9pdu_writef(ofcall, 0, "dbwD", 0, P9_RREAD, tag,count,tmp);
			p9pdu_finalize(ofcall);
			p9_xosclient_kwrite(pmu_reg_handle,ofcall);
			break;
		case P9_TWRITE:
			p9pdu_readf(ifcall,0,"dqD",&tmp1,&offset,&count,&data);
			len = (unsigned char)count;
			addr= (unsigned char)offset;
			tag =ifcall->tag;
			/* as pmu action are not fast */
			/* we do an intermediate copy */
			if (len >100) len=100;
			memcpy(tmp,data,len);
			p9_xosclient_krelease(ifcall);
			pcf5062x_bb_write(addr,tmp,len);
			/* build 9P message and post it  */
			ofcall=kmalloc(sizeof(struct p9_fcall)+MAX_SIZE,GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->tag=tag;
			ofcall->id=P9_RWRITE;
			ofcall->capacity=sizeof(struct p9_fcall)+MAX_SIZE;			
			ofcall->sdata=(u8*)ofcall+sizeof(struct p9_fcall);
			p9pdu_writef(ofcall, 0, "dbwd", 0, P9_RWRITE, tag,count);
            p9pdu_finalize(ofcall);
			p9_xosclient_kwrite(pmu_reg_handle,ofcall);
			break;
		default: break;
		}
	}
}


static void pmu_int_handler_ack(unsigned long tag)
{		
	struct p9_fcall *ofcall;
	unsigned char tmp[]="PMUIT";
	ofcall=kmalloc(sizeof(struct p9_fcall)+5+11,GFP_KERNEL);
	p9pdu_reset(ofcall);
	ofcall->sdata=(u8*)ofcall+sizeof(struct p9_fcall);
	ofcall->capacity=sizeof(struct p9_fcall)+5+11;
    p9pdu_writef(ofcall,0,"dbwD",11+5,P9_RREAD,tag,5,tmp);
    p9_xosclient_kwrite(pmu_int_handle,ofcall);
}

static void pmu_int_handler(struct work_struct *work)
{
	struct p9_fcall *ifcall,*ofcall;
	unsigned long tag;
	ifcall=p9_xosclient_kread(pmu_int_handle);
	if ((ifcall->id ==P9_TREAD))
	{
		tag=ifcall->tag;
		p9_xosclient_krelease(ifcall);
		/* function is non blocking */
		pcf5062x_bb_int(pmu_int_handler_ack,tag);	
	}
	else
	{
		tag=ifcall->tag;
		p9_xosclient_krelease(ifcall);
		ofcall=kmalloc(sizeof(struct p9_fcall)+MAX_SIZE,GFP_KERNEL);
     	p9pdu_reset(ofcall);
	    ofcall->sdata=(u8*)ofcall+sizeof(struct p9_fcall);
		ofcall->capacity=sizeof(struct p9_fcall)+MAX_SIZE;
        p9pdu_writef(ofcall,0,"dbwT",0,P9_RERROR,tag,"Unknown Request");
		p9pdu_finalize(ofcall);
		p9_xosclient_kwrite(pmu_int_handle,ofcall);
	}

}

struct p9_xos_kopen reg_server=
{&pmu_wr_cb, NULL, &pmu_acc, NULL};
struct p9_xos_kopen int_server=
{&pmu_wr_cb, NULL, &pmu_int, NULL};


static int __init vdpmu_init(void)
{
   
	pmu9p_wq = create_workqueue("pmu9p_wq");
	/* init the 2 possible work queue */
	INIT_WORK(&v_pmuhandle_wq,pmu_acc_handler);
	INIT_WORK(&v_pmuint_wq,pmu_int_handler);
	/* connect to the 9p channel dedicated to pmu service */
    pmu_reg_handle=p9_xosclient_kopen(CLIENT_REG,&reg_server, "pmu_reg");
	if (pmu_reg_handle == 0 ) goto fail;
	pmu_int_handle=p9_xosclient_kopen(CLIENT_INT,&int_server, "pmu_int");
	if (pmu_int_handle == 0 ) goto fail;

	return 0;
fail:
	/* fix me  no specific treatment yet*/
	return 0;
}

static void __exit vdpmu_exit(void)
{/* fix me unregister */
}

late_initcall(vdpmu_init);
module_exit(vdpmu_exit);




