/*
 * Linux kernel module for  ADC measurement on Icebird
 * Authors: Michel JAOUEN (michel.jaouen@stericsson.com)
 *          Ludovic BARRE (ludovic.barre@stericsson.com)
 * Copyright (c) STERICSSON 2009
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/parser.h>

#include <net/9p/9p.h>
#include <net/9p/client.h>
#include <net/9p/transport.h>


/* for each measure request an element of following type is allocated*/
struct adc_measure {
	struct p9_fid *fid;
	void (*callback)(unsigned long);
	u16 tag;
};

#define ADC_DIR "adc"

#define ADC_FID_CLOSED		0
#define ADC_FID_OPENING		1
#define ADC_FID_OPENED		2

static struct adc_vdev {
	struct	p9_client *client;
	int		nb_adcfile;
	struct	p9_fid *p9_fid_adcroot;
	struct	p9_fid **p9_fid_adcfile;
	int		p9_fid_status;
  spinlock_t lock;
} adc_ctx = { NULL,};

/******************************************************************************
 * adc printk
 *****************************************************************************/
#define adc_printk(level, fmt, args...) \
	printk(level "adcclient: %-25s: " fmt,__FUNCTION__, ##args)

/******************************************************************************
 * adc callback
 *****************************************************************************/
static void adc_rpc_aread(int error, u32 count, void *data, void *cookie)
{
	struct adc_measure *adcmes = cookie;
	unsigned long val;

	if (unlikely(count!=4))
		adc_printk(KERN_ERR,"count error\n");

	memcpy(&val, data, 4);
	adcmes->callback(val);

	p9_client_destroy_aio(adcmes->fid, adcmes->tag);
	kfree(adcmes);
}
/******************************************************************************
 * adc private functions
 *****************************************************************************/
static int adc_p9_register(void)
{
	int err=0;
	
	adc_ctx.client = p9_client_create("adc", "trans=xoscore,noextend");
	if (IS_ERR(adc_ctx.client)) {
		adc_printk(KERN_ERR, "p9_client_create fail\n");
		err = PTR_ERR(adc_ctx.client);
		adc_ctx.client = NULL;
		goto out;
	}
	
	adc_ctx.p9_fid_adcroot = p9_client_attach(adc_ctx.client,
			NULL, "nobody", 0, "");
	if (IS_ERR(adc_ctx.p9_fid_adcroot)) {
		adc_printk(KERN_ERR, "cannot attach to 9P server\n");
		err = PTR_ERR(adc_ctx.p9_fid_adcroot);
		p9_client_destroy(adc_ctx.client);
		adc_ctx.client = NULL;
		adc_ctx.p9_fid_adcroot=NULL;
		goto out;
	}

out:
	return err;
}

static int adc_readdir(void)
{
	int err=0;
	struct p9_fid *p9_dirfid;
	struct p9_wstat st;
	int buflen;
	char *path[] = { ADC_DIR };
	char *statbuf;
	int n, i = 0;
	int count=0;

	p9_dirfid=p9_client_walk(adc_ctx.p9_fid_adcroot,
			ARRAY_SIZE(path), path, 1);
	if (IS_ERR(p9_dirfid)) {
		adc_printk(KERN_ERR, "cannot walk on dir:%s\n",path[0]);
		err = PTR_ERR(p9_dirfid);
		goto out;
	}

	err=p9_client_open(p9_dirfid, P9_OREAD);
	if (IS_ERR_VALUE(err) ) {
		adc_printk(KERN_ERR, "cannot open %s dir\n",path[0]);
		goto out;
	}

	buflen = p9_dirfid->clnt->msize - P9_IOHDRSZ;
	statbuf = kmalloc(buflen, GFP_KERNEL);
	if (IS_ERR(statbuf)) {
		adc_printk(KERN_ERR, "cannot alloc memory\n");
		err=PTR_ERR(statbuf);
		goto close;
	}

	while (1) {
		err = p9_client_read(p9_dirfid,statbuf, 
				NULL, p9_dirfid->rdir_fpos, buflen);
		if (err <= 0)
			break;
		n=err;
		i=0;
		while (i < n) {
			err = p9stat_read(statbuf + i, buflen-i, &st,
							p9_dirfid->clnt->dotu);
			if (err) {
				err = -EIO;
				p9stat_free(&st);
				goto free;
			}
			if (strcmp(st.name, "readme.txt")) /* Ignore "readme.txt" entry */
				count++;
			i += st.size+2;
			p9_dirfid->rdir_fpos += st.size+2;
			p9stat_free(&st);
		}
	}	
	err=count;

free:
	kfree(statbuf);
close:
	p9_client_clunk(p9_dirfid);
out:
	return err;
}

static void adc_close(void)
{
	int fileid;
  
	if (!adc_ctx.p9_fid_adcfile) 
		goto out;

	//close all files
	for(fileid=0; fileid<adc_ctx.nb_adcfile;fileid++) {
		if (adc_ctx.p9_fid_adcfile[fileid]!=NULL)
			p9_client_clunk(adc_ctx.p9_fid_adcfile[fileid]);
		adc_ctx.p9_fid_adcfile[fileid]=NULL;
	}
	adc_ctx.nb_adcfile=0;
	//free all p9_fid * by files 
	kfree(adc_ctx.p9_fid_adcfile);
	adc_ctx.p9_fid_adcfile = NULL;
	
out:
	spin_lock(&adc_ctx.lock);
	adc_ctx.p9_fid_status = ADC_FID_CLOSED;
	spin_unlock(&adc_ctx.lock);
}

static int adc_open(void)
{
	int err=0;
	char *path[] = { ADC_DIR, "255" };
	int fileid;

  spin_lock(&adc_ctx.lock);
	if (adc_ctx.p9_fid_status == ADC_FID_OPENED) {
		spin_unlock(&adc_ctx.lock);
		goto out;
	} else if (adc_ctx.p9_fid_status == ADC_FID_OPENING) {
		err = -EAGAIN;
		spin_unlock(&adc_ctx.lock);
		goto out;
	}
	adc_ctx.p9_fid_status = ADC_FID_OPENING;
	spin_unlock(&adc_ctx.lock);
	
	if ( !adc_ctx.client ) {
		err=adc_p9_register();
		if (IS_ERR_VALUE(err))
			goto err;
	}
	
	if ( !adc_ctx.nb_adcfile ) {
		err=adc_readdir();
		if (IS_ERR_VALUE(err)){
			adc_printk(KERN_ERR, "cannot read %s dir\n",path[0]);
			goto err;
		}
		adc_ctx.nb_adcfile=err;
	}

	adc_ctx.p9_fid_adcfile=
		kzalloc((adc_ctx.nb_adcfile * sizeof(struct p9_fid *)), GFP_KERNEL);
	if (!adc_ctx.p9_fid_adcfile) {
		adc_printk(KERN_ERR, "out of memory\n");
		err = -ENOMEM;
		goto err;
	}

	//open all adc files
	for(fileid=0; fileid<adc_ctx.nb_adcfile;fileid++) {
		snprintf(path[1],3, "%u", fileid);
		adc_ctx.p9_fid_adcfile[fileid]=
			p9_client_walk(adc_ctx.p9_fid_adcroot, ARRAY_SIZE(path), path, 1);
		if (IS_ERR(adc_ctx.p9_fid_adcfile[fileid])) {
			adc_printk(KERN_DEBUG, "cannot walk 9P file\n");
			err = PTR_ERR(adc_ctx.p9_fid_adcfile[fileid]);			
			adc_ctx.p9_fid_adcfile[fileid]=NULL;
			goto err;
		}

		err = p9_client_open(adc_ctx.p9_fid_adcfile[fileid], P9_OREAD);
		if (IS_ERR_VALUE(err) ) {
			adc_printk(KERN_DEBUG, "cannot open 9P file\n");
			goto err;
		}
	}

	err = 0;
	spin_lock(&adc_ctx.lock);
	adc_ctx.p9_fid_status = ADC_FID_OPENED;
	spin_unlock(&adc_ctx.lock);

out:

	return err;
err:

	adc_close();
	return err;
}

/******************************************************************************
 * adc export function
 *****************************************************************************/
/** 
 * @brief	run adc measure on adc[fileid] with p9 client [asynchronous]
 *
 * @param fileid	id of adc 
 * @param callback  callback of measurement
 * 
 * @return 0 succesful, other error 
 */
int adc_aread(int fileid, void (*callback)(unsigned long))
{
	int err=0;
	struct adc_measure *adcmes;


	err = adc_open();
	if (IS_ERR_VALUE(err)){
		adc_printk(KERN_DEBUG, "cannot open adc files err=%d\n",err);
		goto out;
	}

	if ((fileid >= adc_ctx.nb_adcfile) || (fileid < 0)) {
		err=-EINVAL;
		goto out;	
	}
	
	err = p9_client_create_aio(adc_ctx.p9_fid_adcfile[fileid]);
	if (IS_ERR_VALUE(err)) {
		adc_printk(KERN_ERR, "p9_client_create_aio fail err=%d\n",err);
		goto out;
	}

	adcmes = kmalloc(sizeof(*adcmes), GFP_KERNEL);
	if (!adcmes) {
		adc_printk(KERN_ERR, "out of memory\n");
		err = -ENOMEM;
		goto out;
	}

	adcmes->tag = err; /* retval of p9_client_create_aio */
	adcmes->fid = adc_ctx.p9_fid_adcfile[fileid];
	adcmes->callback = callback;
	err = p9_client_post_aio(adcmes->fid, adcmes->tag, 0,
			adcmes->fid->iounit ? adcmes->fid->iounit : 4,
			NULL, adc_rpc_aread, adcmes);

	if (IS_ERR_VALUE(err)) {
		adc_printk(KERN_ERR, "p9_client_post_aio fail err=%d\n",err);
		p9_client_destroy_aio(adcmes->fid, adcmes->tag);
		kfree(adcmes);
	}
out:
	return err;
}
EXPORT_SYMBOL_GPL(adc_aread);

/******************************************************************************
 * adc entry
 *****************************************************************************/
static int __init adc_init(void)
{
	spin_lock_init(&adc_ctx.lock);

	spin_lock(&adc_ctx.lock);
	adc_ctx.p9_fid_status = ADC_FID_CLOSED;
	spin_unlock(&adc_ctx.lock);
	return 0;
}

static void __exit adc_exit(void)
{
	adc_close();
	if (adc_ctx.client) {
		p9_client_destroy(adc_ctx.client);
		adc_ctx.client = NULL;
	}
}

module_init(adc_init);
module_exit(adc_exit);

MODULE_AUTHOR("Michel JAOUEN - Copyright (C) STERICSSON 2009");
MODULE_DESCRIPTION("Linux plug-in for virtual adc read Icebird");
MODULE_LICENSE("GPL");


