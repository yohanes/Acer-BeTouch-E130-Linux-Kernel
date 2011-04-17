/*
 * ============================================================================
 *
 *       Filename:  console9p.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/26/2009 02:14:19 PM
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

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include "../../../net/9p/protocol.h"
#include <net/9p/trans_xosclient.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("JAOUEN Michel, STNWIRELESS");
MODULE_DESCRIPTION("9p like console server for Modem");
/* modules depends on device pcf50626srv.c */
/* This device is connected to i2c */
#define CLIENT_CONSOLE 3
#define MAX_SIZE  50
static unsigned long npconsole_handle;
static struct work_struct v_npconsole_wq;
/** 
 * @brief this function is called in the context of a cross interrupt
 * Therefore function creates a bottom half which executes the process inside 
 *
 * @param event 
 * @param cookie 
 */
static void npconsole_acc( void * cookie)
{
	schedule_work(&v_npconsole_wq);
}

static void npconsole_wr_cb(struct p9_fcall *ofcall)
{
	kfree(ofcall);
}

/** 
 * @brief handle all
 * 
 * @param work 
 */
static void npconsole_handler(struct work_struct *work)
{	
	u16 tag;
	struct p9_fcall *ifcall,*ofcall;
	u32 count,fid;
    u64 offset;
	char *data;
	/* we recopy the 9p request inside tmp which is on the stack of bottomhalf */
	/* fix me this copy must be avoid in or to do so */
	/* interface must be changed */
	ifcall=p9_xosclient_kread(npconsole_handle);
	if (ifcall)
	{
		p9_parse_header(ifcall, NULL, NULL, NULL, 0);
		switch (ifcall->id)
		{
		case P9_TREAD:
			/* we can release the tread */	
			tag=ifcall->tag;
			p9_xosclient_krelease(ifcall);
			printk("Np Console received read req !\n");
            ofcall=kmalloc(sizeof(struct p9_fcall)+MAX_SIZE,GFP_KERNEL);
     	    p9pdu_reset(ofcall);
	        ofcall->sdata=(u8*)((u8*)ofcall+sizeof(struct p9_fcall));
			ofcall->capacity=sizeof(struct p9_fcall)+MAX_SIZE;
			p9pdu_writef(ofcall,0,"dbwT",0,P9_RERROR,tag,"Unknown Request");
		    p9pdu_finalize(ofcall);
			p9_xosclient_kwrite( npconsole_handle,ofcall);

			break;
		case P9_TWRITE:
			p9pdu_readf(ifcall,0,"dqD",&fid,&offset,&count,&data);
			tag =ifcall->tag;
			printk(KERN_ERR"%s",data);
			p9_xosclient_krelease(ifcall);
			/* build 9P message and post it  */
			ofcall=kmalloc(sizeof(struct p9_fcall)+MAX_SIZE,GFP_KERNEL);
			p9pdu_reset(ofcall);
			ofcall->sdata=(u8*)ofcall+sizeof(struct p9_fcall);
			ofcall->capacity=sizeof(struct p9_fcall)+MAX_SIZE;
            p9pdu_writef(ofcall,0,"dbwd",0,P9_RWRITE,tag,count);
			p9pdu_finalize(ofcall);
			p9_xosclient_kwrite( npconsole_handle,ofcall);
			break;
		default: 
			break;
		}
	}
}




struct p9_xos_kopen npconsole_server=
{ &npconsole_wr_cb,NULL,&npconsole_acc,NULL};

static int __init npconsole_init(void)
{
	/* init the 2 possible work queue */
	INIT_WORK(&v_npconsole_wq,npconsole_handler);
	/* connect to the 9p channel dedicated to pmu service */
    npconsole_handle=p9_xosclient_kopen(CLIENT_CONSOLE,&npconsole_server, "console");
	if (npconsole_handle == 0 ) goto fail;
	return 0;
fail:
	/* fix me  no specific treatment yet*/
	return 0;
}

static void __exit npconsole_exit(void)
{/* fix me unregister */
}

late_initcall(npconsole_init);
module_exit(npconsole_exit);




