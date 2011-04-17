/*
 *  linux/arch/arm/plat-pnx/xos_trace.c
 *  Copyright 2010 ST-Ericsson
 *  Gabriel Fernandez <gabriel.fernandez@stericsson.com>
 *
 *  Mechanism to evacuate pnx modem trace
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>	/* need_resched() */

#include <linux/kernel.h>
#include <linux/smp_lock.h>

#include <linux/types.h>
#include <linux/init.h>

#include <asm/mach/time.h>

#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <asm/uaccess.h>	/* for copy_from_user */

#include <asm/proc-fns.h>
#include <linux/device.h>

#ifdef CONFIG_NKERNEL
#include <nk/xos_area.h>
#include <nk/xos_ctrl.h>
#else
typedef void* xos_area_handle_t;
#endif

#define XOSTRACE_C
#include "xos_trace_ringbuffer.h"

#include <mach/xos_trace.h>




#include <linux/fs.h>
#include <linux/kdev_t.h>	/* needed for MKDEV */
#include <linux/cdev.h>
#include <linux/input.h>
#define PERROR(format, arg...) printk(KERN_ERR format , ## arg)

#undef XOSTRACE_C

/*** Module definition ***/
/*************************/
#define MODULE_NAME "XOSTRACE"
#define PKMOD MODULE_NAME ": "

/*** PNX TRACE driver ***/
/***********************/

#define XOS_TRACE_NAME_LEN	16

struct trace_driver {
	char			name[XOS_TRACE_NAME_LEN];
	struct module 		*owner;
};


static struct xos_trace_ctxt xos_trace_data =
{
	.shared = NULL,
	.base_adr = NULL,
};


enum
{
	MODEM_SPY_MODE_NORMAL,
	MODEM_SPY_MODE_CONFIG,
};

int modem_spy_mode=MODEM_SPY_MODE_NORMAL;


/* proc fs */
#define MODEM_APP_CMD_PROCFS_NAME		"modemApp.cmd"
#define MODEM_SPY_CMD_PROCFS_NAME		"modemSpy.cmd"
#define LINUX_KERNEL_CMD_PROCFS_NAME	"linuxKer.cmd"

#define TIME_STAMP_PROCFS_NAME			"timestamp"

static struct proc_dir_entry *root_proc_file;

static struct proc_dir_entry *modem_spy_cmd_proc_file;
static struct proc_dir_entry *modem_app_cmd_proc_file;
static struct proc_dir_entry *linux_kern_cmd_proc_file;

static struct proc_dir_entry *time_stamp_proc_file;

typedef void (*callback_t)(void);



/* Modem Spytracer */

#define MODEM_SPY_BUFFER_LEN (1024*64*2) 

struct xos_trace_ring_buffer	*modem_spy_ring_buffer;

wait_queue_head_t modem_spy_buffer_empty;
wait_queue_head_t modem_spy_wait_response;

static int modem_spy_buffer_was_full=0;

static unsigned long modem_process_list_idx=0;



/* Modem App */

#define MODEM_APP_BUFFER_LEN (1024*64)

struct xos_trace_ring_buffer	*modem_app_ring_buffer;

wait_queue_head_t modem_app_buffer_empty;
wait_queue_head_t modem_app_wait_response;

static int modem_app_buffer_was_full=0;
int xos_trace_modem_spy_config(const char *buffer, unsigned long count);

struct xos_trace_buffer_mngt
{
	int ready;

	struct xos_trace_ring_buffer *ring_buffer;

	wait_queue_head_t queue_empty;

	spinlock_t        lock;

	int buffer_was_full;

	callback_t full_buffer_call_back;
};

struct xos_trace_buffer_mngt linux_kern_buff_mngt;


/* Linux Kernel trace */
#define LINUX_KERN_BUFFER_LEN (1024*64*2)
wait_queue_head_t linux_kern_buffer_empty;

struct xos_trace_ring_buffer	linux_kern_ring_buffer;


#define XOS_TRACE_MAX_PROCESS_NAME_LENGTH 40

#define MAJOR_NUM 0
//#define DEVICE_NAME "xostrace!modemSpy"
#define DEVICE_NAME "modemSpy"

typedef struct 
{
	unsigned char
		free			:1,
		registered		:1,
		on_off			:1,
		name_in_trace	:1;
} b_field_status_t;

struct xos_trace_process_info_t
{
	char	name[XOS_TRACE_MAX_PROCESS_NAME_LENGTH];
	b_field_status_t bfield_status;
};


struct xos_trace_process_info_t xos_trace_process_list[XOSTRACE_MAX_PROCESS];
spinlock_t xos_trace_process_list_lock;


struct xos_trace_cmd_t xos_trace_cmd;

wait_queue_head_t linux_kernel_cmd_wait_response;


extern u64 pnx_rtke_read(void);




static int my_id;

int xos_trace_get_free_index(void)
{
	int i;
	int id=-1;

	spin_lock(&xos_trace_process_list_lock);

	for(i=0;i<XOSTRACE_MAX_PROCESS;i++)
	{
		if (xos_trace_process_list[i].bfield_status.free == 0)
		{
			id=i;
			break;
		}
	}

	spin_unlock(&xos_trace_process_list_lock);
	
	return id;
}


int xos_trace_is_process_registered(int id)
{
	return(xos_trace_process_list[id].bfield_status.registered);
}


int xos_trace_set_process_registered(int id)
{
	xos_trace_process_list[id].bfield_status.registered=1;
	return 1;
}

int xos_trace_get_process_status(int id)
{
	return(xos_trace_process_list[id].bfield_status.on_off);
}
EXPORT_SYMBOL(xos_trace_get_process_status);

int xos_trace_enable_process(int id)
{
	xos_trace_process_list[id].bfield_status.on_off=1;
	return 1;	
}

int xos_trace_disable_process(int id)
{
	xos_trace_process_list[id].bfield_status.on_off=0;
	return 1;	
}

char *xos_trace_get_process_name(int id)
{
	return (xos_trace_process_list[id].name);
}

int xos_trace_get_process_id(char *name)
{
	int i;
	char *ptr_name;

	for(i=0;i<XOSTRACE_MAX_PROCESS;i++)
	{
		if (xos_trace_process_list[i].bfield_status.free)
		{
			ptr_name = xos_trace_get_process_name(i);
			if (strcmp(name,ptr_name) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

int xos_trace_register(char *name, u8 active, u8 registered)
{
	int id;

	id = xos_trace_get_free_index();

	if (id != -1)
	{

		xos_trace_process_list[id].bfield_status.on_off = active;
		xos_trace_process_list[id].bfield_status.registered = registered;
		xos_trace_process_list[id].bfield_status.free = 1;

		strcpy(xos_trace_process_list[id].name,name);

	}
	return id;
}

int	xos_trace_add_process_req(char *name)
{
	int id;

	id = xos_trace_register(name,1,0);

	return id;
}

void xos_trace_activate_all(void)
{
	int i;

	for(i=0;i<XOSTRACE_MAX_PROCESS;i++)
	{
		if (xos_trace_process_list[i].bfield_status.free)
		{
			xos_trace_enable_process(i);
		}
	}
}

void xos_trace_desactivate_all(void)
{
	int i;

	for(i=0;i<XOSTRACE_MAX_PROCESS;i++)
	{
		if (xos_trace_process_list[i].bfield_status.free)
		{
				xos_trace_process_list[i].bfield_status.on_off=0;
		}
	}
}
int xos_trace_desactivate(char *name)
{
	int id;

	id = xos_trace_get_process_id(name);

	if (id != -1)
	{
		xos_trace_disable_process(id);

		return 1;
	}
	return 0;
}

size_t  xos_trace_get_process_list(char *buf)
{
	size_t size_buf=0;
	char *ptr_buf=buf;
	int id,status,valid;
	char *name;

	*ptr_buf='\0';

	size_buf += sprintf(ptr_buf+size_buf,"Kernel process list\n");
	size_buf += sprintf(ptr_buf+size_buf,
			"id registered status process name\n");


	for(id=0;id<XOSTRACE_MAX_PROCESS;id++)
	{
		if (xos_trace_process_list[id].bfield_status.free)
		{
			name   = xos_trace_get_process_name(id);
			status = xos_trace_get_process_status(id);
			valid  = xos_trace_is_process_registered(id);

			size_buf += sprintf(ptr_buf+size_buf," %d     %d        %d    %s\n"
					,id, valid,status, name);

		}
	}
	size_buf += sprintf(ptr_buf+size_buf,"\n");

	return size_buf;

}



int xos_trace_on_off_management(int cmd, char *buffer, char *ptr_buf)
{
	int size_buf=0;
	int i,idx=0,count;
	char process_name[80];
	int error=1;

	u32 index;
	count = strlen(buffer);

	for(i=0; i<=count; i++)
	{
		switch(buffer[i])
		{
		case 0x0a:
		case 0x0d:
			break;

		case ',':
		case '\0':
			process_name[idx] = '\0';

			if (process_name[0])
			{
				if (cmd == 1)
				{
					index = xos_trace_get_process_id(process_name);

					/* if process is not already registerd by RTK */
					if (index == -1)
					{
						if (xos_trace_add_process_req(process_name) == -1)
							error=0;
					} else
						xos_trace_enable_process(index);
				} else {
					if (xos_trace_desactivate(process_name) == -1)
						error=0;
					}
				}

			idx=0;
			process_name[0]='\0';

			break;

		default:
			process_name[idx++]=buffer[i];
			break;
		}
	}

	if (error)
	{
		size_buf = sprintf(ptr_buf,"ok\n");
	}
	else
	{
		size_buf = sprintf(ptr_buf,"ok\n");
	}

	return size_buf;
}





int xos_Trace_PutBuffer(int id, u32 size, char *buffer)
{

	u32 free_size;
	u8 	buffer_was_empty=0;
	u8 *p_dst_buff;
	u32 idx,Max_Size;
	u32 ret;
	u64 timestamp;
	struct xos_trace_ring_buffer *ring_buffer;

	spin_lock(&linux_kern_buff_mngt.lock);

	if (!linux_kern_buff_mngt.ready)
	{
		printk(KERN_ERR "\nXOS_TRACE NOT READY !!!!!!!!\n\n");
		ret=0;
		goto out;
	}

	ring_buffer=linux_kern_buff_mngt.ring_buffer;

	free_size = xos_trace_get_circular_buffer_free_space(ring_buffer);

	/* check with length of trace structure (START/ID/TIME_STAMP/LENTGH) */
	if (size > (free_size - sizeof(struct xos_trace_app_frame)))
	{
		printk("App trace buffer FULL !!! buffer is lost\n");
		// TODO
		//		xos_ctrl_raise(xos_trace_data.request, XOSTRACE_SPY_BUFFER_FULL );
		//		return 0;
		ret=0;		
		goto out;
	}

	timestamp = pnx_rtke_read();

	/* if buffer was empty */
	if (ring_buffer->rd == ring_buffer->wr)
	{
		buffer_was_empty=1;
	}

	p_dst_buff = ring_buffer->va_buffer;

	idx = ring_buffer->wr;

	Max_Size = ring_buffer->buf_size;

	idx = xos_trace_Put_u8_into_circular_buffer(XOS_TRACE_START,p_dst_buff, idx, Max_Size);

	idx = xos_trace_Put_u32_into_circular_buffer(id | XOS_TRACE_LINUX_KER_MASK, p_dst_buff, idx, Max_Size);

	/* put the timestamp */
	idx = xos_trace_Put_u64_into_circular_buffer(timestamp,p_dst_buff, idx, Max_Size);

	/* put size of the buffer */
	idx = xos_trace_Put_u32_into_circular_buffer(size,p_dst_buff, idx, Max_Size);

	/* put the trace buffer */
	idx = xos_trace_Put_into_circular_buffer(buffer,size, p_dst_buff, idx, Max_Size);

	ring_buffer->wr = idx;

	ret =size;

out:

	spin_unlock(&linux_kern_buff_mngt.lock);

	if (buffer_was_empty)
	{
		wake_up(&linux_kern_buff_mngt.queue_empty);
	}

	return ret;	
}
EXPORT_SYMBOL(xos_Trace_PutBuffer);

void xos_Trace_Put_buffer(u32 id, const char *fmt, ...)
{
	int n;
	va_list args;
	char buf[XOS_TRACE_MAX_MSG_LENGTH];

	va_start(args, fmt);
	n = vscnprintf(buf, XOS_TRACE_MAX_MSG_LENGTH, fmt, args);
	va_end(args);

	xos_Trace_PutBuffer(id,strlen(buf),buf);

}
EXPORT_SYMBOL(xos_Trace_Put_buffer);





int xos_Trace_Register(char *name, u8 active)
{
	int id;


	id = xos_trace_get_process_id(name);

	if (id == -1)
	{
		id = xos_trace_register(name, active, 1);
		if (id ==-1)
		{
			return -1;
		}
	}
	else
	{
		xos_trace_set_process_registered(id);
	}

	/* only one time, just to associate id and process name */
	xos_Trace_PutBuffer(id,strlen(name),name);

	return id;
}
EXPORT_SYMBOL(xos_Trace_Register);







/**
 * Modem send signal when data is received and ring buffer was empty
 *
 *
 */
void xostrace_app_data_ready (unsigned event, void * cookies)
{

	wake_up(&modem_app_buffer_empty);

}

/**
 * Modem send signal when data is received and ring buffer was empty
 *
 *
 */
void xostrace_spy_data_ready (unsigned event, void * cookies)
{
	wake_up(&modem_spy_buffer_empty);

}



/**
 * Modem send signal when data is received and ring buffer was empty
 *
 *
 */
void xostrace_app_response_ready (unsigned event, void * cookies)
{
	xos_trace_data.base_adr->modem_app.cmd.c_ptr  = xos_trace_data.base_adr->modem_app.cmd.buffer;

	xos_trace_data.base_adr->modem_app.cmd.length = strlen(xos_trace_data.base_adr->modem_app.cmd.c_ptr) + 1;

	xos_trace_data.base_adr->modem_app.cmd.status = 2;

	wake_up(&modem_app_wait_response);
}

/**
 * Modem send signal when data is received and ring buffer was empty
 *
 *
 */
void xostrace_spy_response_ready (unsigned event, void * cookies)
{
	xos_trace_data.base_adr->modem_spy.cmd.c_ptr  = xos_trace_data.base_adr->modem_spy.cmd.buffer;

	xos_trace_data.base_adr->modem_spy.cmd.length = strlen(xos_trace_data.base_adr->modem_spy.cmd.c_ptr) + 1;

	xos_trace_data.base_adr->modem_spy.cmd.status = 2;

	wake_up(&modem_spy_wait_response);
}


/**
 * Modem send signal when data is received and ring buffer was empty
 *
 *
 */
void xostrace_linux_kernel_response_ready (void)
{
	xos_trace_cmd.c_ptr  = xos_trace_cmd.buffer;

	xos_trace_cmd.length = strlen(xos_trace_cmd.c_ptr) + 1;

	xos_trace_cmd.status = 2;

	wake_up(&linux_kernel_cmd_wait_response);
}


/**
 * Modem send signal when ring buffer is full
 *
 *
 */

void xostrace_app_buffer_full (unsigned event, void * cookies)
{
	/* if app_device_read has already read all buffer */
	if (modem_app_ring_buffer->rd == modem_app_ring_buffer->wr) {
		xos_ctrl_raise(xos_trace_data.request, \
				XOSTRACE_APP_BUFFER_FULL);
	} else {
	/* modem can't put trace because the buffer is full */
	modem_app_buffer_was_full = 1;

		XOS_TRACE_PUT_BUFFER(my_id, "app modem buffer is full !!! ");
	}

}

/**
 * Modem send signal when ring buffer is full
 *
 *
 */
void xostrace_spy_buffer_full (unsigned event, void * cookies)
{
	/* if spy_device_read has already read all buffer */
	if (modem_spy_ring_buffer->rd == modem_spy_ring_buffer->wr)	{
		/* Send signal to modem that buffer is now not full */
		xos_ctrl_raise(xos_trace_data.request, \
				XOSTRACE_SPY_BUFFER_FULL);
	} else {
	/* modem can't put trace because the buffer is full */
	modem_spy_buffer_was_full = 1;

		XOS_TRACE_PUT_BUFFER(my_id, "spy modem buffer is full !!! ");
	}
	
}



/**
 * This function recieve the list of the process to trace.
 * The list is stored to xos_trace_data.base_adr->modem_spy.process_list[]
 * 
 */

int xos_trace_modem_spy_config(const char *buffer, unsigned long count)
{
	char *process_list;
	unsigned long i;
	int ret=1;

	process_list = xos_trace_data.base_adr->modem_spy.cmd.buffer;

	for (i=0; i<count; i++)
	{
		switch(buffer[i])
		{
		case '\0':
			ret = 0;
			break;

		default:
			if (modem_process_list_idx < MAX_MODEM_CMD_BUFFER_LEN-1)
			{
				process_list[modem_process_list_idx++] = buffer[i];
			}
			else
			{
				printk(KERN_ERR "file is to big!\n");
				i=count;
			}
		}
	}
	process_list[modem_process_list_idx] = '\0';

	return ret;

}




static int create_proc_path(void)
{
	int error=1;

	root_proc_file = proc_mkdir("xos_trace", NULL);
	if (!root_proc_file)
	{
		error=0;
	}

	return error;
}


static struct proc_dir_entry *xos_trace_create_procfs_file(struct proc_dir_entry *root_proc,char *filename, void *proc_read, void *proc_write)
{
	struct proc_dir_entry *proc_fs;

	proc_fs = create_proc_entry(filename, S_IFREG | S_IWUGO | S_IRUGO, root_proc);

	if (proc_fs)
	{
		if (proc_read)
		{
			proc_fs->read_proc = proc_read;
		}
		if (proc_write)
		{
			proc_fs->write_proc = proc_write;
		}
	}
	else
	{
		printk(PKMOD "cant create /proc/xos_trace/%s !!!\n", filename);	
	}

	return(proc_fs);
}




int modem_app_cmd_procfile_read(char *buffer,
		char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{
	size_t c_size;
	size_t r_size;
	int i;

	if (xos_trace_data.base_adr->modem_app.cmd.status == 0)
	{
		*eof=1;
		return 0;
	}

	wait_event(modem_app_wait_response,xos_trace_data.base_adr->modem_app.cmd.status != 1);

	xos_trace_data.base_adr->modem_app.cmd.status = 3;

	c_size = xos_trace_data.base_adr->modem_app.cmd.length;

	if (c_size == 0)
	{
		*eof=1;
		return 0;
	}
	
	r_size = count;

	if (count > c_size)
	{
		r_size = c_size;
	}

	for(i=0; i<r_size; i++)
	{
		buffer[i] = *xos_trace_data.base_adr->modem_app.cmd.c_ptr++;
	}

	xos_trace_data.base_adr->modem_app.cmd.length -= r_size;

	if (xos_trace_data.base_adr->modem_app.cmd.length == 0)
	{
		*eof=1;
		xos_trace_data.base_adr->modem_app.cmd.status = 0;
	}

	*buffer_location = buffer;

	return r_size;

}


int modem_spy_cmd_procfile_read(char *buffer,
		char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{

	size_t c_size;
	size_t r_size;
	int i;

	if (xos_trace_data.base_adr->modem_spy.cmd.status == 0)
	{
		*eof=1;
		return 0;
	}

	wait_event(modem_spy_wait_response,xos_trace_data.base_adr->modem_spy.cmd.status != 1);

	xos_trace_data.base_adr->modem_spy.cmd.status = 3;

	c_size = xos_trace_data.base_adr->modem_spy.cmd.length;

	if (c_size == 0)
	{
		*eof=1;
		return 0;
	}
	
	r_size = count;

	if (count > c_size)
	{
		r_size = c_size;
	}

	for(i=0; i<r_size; i++)
	{
		buffer[i] = *xos_trace_data.base_adr->modem_spy.cmd.c_ptr++;
	}

	xos_trace_data.base_adr->modem_spy.cmd.length -= r_size;

	if (xos_trace_data.base_adr->modem_spy.cmd.length == 0)
	{
		*eof=1;
		xos_trace_data.base_adr->modem_spy.cmd.status = 0;
	}

	*buffer_location = buffer;

	return r_size;

}



int linux_kern_cmd_procfile_read(char *buffer,
		char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{

	size_t c_size;
	size_t r_size;
	int i;

	if (xos_trace_cmd.status == 0)
	{
		*eof=1;
		return 0;
	}

	wait_event(linux_kernel_cmd_wait_response,xos_trace_cmd.status != 1);

	xos_trace_cmd.status = 3;

	c_size = xos_trace_cmd.length;

	if (c_size == 0)
	{
		*eof=1;
		return 0;
	}
	
	r_size = count;

	if (count > c_size)
	{
		r_size = c_size;
	}

	for(i=0; i<r_size; i++)
	{
		buffer[i] = *xos_trace_cmd.c_ptr++;
	}

	xos_trace_cmd.length -= r_size;

	if (xos_trace_cmd.length == 0)
	{
		*eof=1;
		xos_trace_cmd.status = 0;
	}

	*buffer_location = buffer;

	return r_size;
}


int xos_trace_timestamp_procfile_read(char *buffer,
		char **buffer_location,
		off_t offset, int count, int *eof, void *data)
{

	int ret = 0;
	u64 timestamp;
	
	if (offset)
		return ret;

	timestamp = pnx_rtke_read();

	ret = sprintf(buffer,"%016llx\n",timestamp);
	
	if (count > ret)
		count = ret;

	*eof = 1;

	return ret;


}


/**
 * User interface to send command for Modem App trace
 *
 */

int modem_app_cmd_procfile_write(struct file *file, const char *buffer, unsigned long count,
		void *data)
{
	if (strncmp(buffer,"START",strlen("START")) ==0)
	{
		init_ring_buffer(modem_app_ring_buffer,MODEM_APP_BUFFER_LEN);

		xos_ctrl_raise(xos_trace_data.request, XOSTRACE_APP_MODEM_READY );
	}
	else 
	{
		strncpy(xos_trace_data.base_adr->modem_app.cmd.buffer, buffer,count);
		xos_trace_data.base_adr->modem_app.cmd.buffer[count]='\0';

		xos_trace_data.base_adr->modem_app.cmd.status = 1;

		xos_ctrl_raise(xos_trace_data.request, XOSTRACE_APP_SEND_CMD );

	}

	return count;

}

int modem_spy_cmd_procfile_write(struct file *file, const char *buffer, unsigned long count,
		void *data)
{

	if (modem_spy_mode == MODEM_SPY_MODE_NORMAL)
	{
		if (strncmp(buffer,"START",strlen("START")) ==0)
		{
			init_ring_buffer(modem_spy_ring_buffer,MODEM_SPY_BUFFER_LEN);
			xos_ctrl_raise(xos_trace_data.request, XOSTRACE_SPY_MODEM_READY );
		}
		else if (strncmp(buffer,"CONFIG ", strlen("CONFIG ")) ==0)
		{
			xos_trace_modem_spy_config(buffer,count);
			modem_spy_mode=MODEM_SPY_MODE_CONFIG;
		}
		else
		{
			strncpy(xos_trace_data.base_adr->modem_spy.cmd.buffer, buffer,count);
			xos_trace_data.base_adr->modem_spy.cmd.buffer[count]='\0';
			xos_trace_data.base_adr->modem_spy.cmd.status = 1;
				
			xos_ctrl_raise(xos_trace_data.request, XOSTRACE_SPY_SEND_CMD );

		}
	}
	else
	{
		xos_trace_modem_spy_config(buffer,count);
		xos_trace_data.base_adr->modem_spy.cmd.status = 1;

		xos_ctrl_raise(xos_trace_data.request, XOSTRACE_SPY_SEND_CMD );
		modem_spy_mode = MODEM_SPY_MODE_NORMAL;
	}

	return count;

}



/**
 * User interface to send command for Modem App trace
 *
 */

int linux_kern_cmd_procfile_write(struct file *file, const char *buffer, unsigned long count,
		void *data)
{
	int size_buf;

	char *ptr_buf =  xos_trace_cmd.buffer;



	if (strncmp(buffer,"help",strlen("help")) == 0)
	{
		size_buf = sprintf(ptr_buf,"\nlist : list of allowed process to be traced\n");
		ptr_buf += size_buf;

		size_buf = sprintf(ptr_buf,"trace_on  <process name> : enable  trace of <process name>\n");
		ptr_buf += size_buf;
		size_buf = sprintf(ptr_buf,"trace_off <process name> : disable trace of <process name>\n");
		ptr_buf += size_buf;

		size_buf = sprintf(ptr_buf,"trace_all_on  : enable  all traces\n");
		ptr_buf += size_buf;
		size_buf = sprintf(ptr_buf,"trace_all_off : disable all traces\n");
	}

	else if (strncmp(buffer,"list",strlen("list")) ==0)
	{
		size_buf = xos_trace_get_process_list(ptr_buf);
	}

	else if (strncmp(buffer,"trace_all_on",strlen("trace_all_on")) ==0)
	{
		size_buf = sprintf(ptr_buf,"OK\n");

		xos_trace_activate_all();

	}
	else if (strncmp(buffer,"trace_all_off",strlen("trace_all_off")) ==0)
	{
		size_buf = sprintf(ptr_buf,"OK\n");

		xos_trace_desactivate_all();

	}
	else if (strncmp(buffer,"trace_on ",strlen("trace_on ")) ==0)
	{
		strncpy(ptr_buf,buffer,count);
		ptr_buf[count]='\0';
		
		size_buf = xos_trace_on_off_management(1, (char *) (ptr_buf + strlen("trace_on ")), ptr_buf);
	}

	else if (strncmp(buffer,"trace_off ",strlen("trace_off ")) ==0)
	{
		strncpy(ptr_buf,buffer,count);
		ptr_buf[count]='\0';

		size_buf = xos_trace_on_off_management(0, (char *) (ptr_buf + strlen("trace_off ")), ptr_buf);
	}
	else
	{
		size_buf = sprintf(ptr_buf,"UNKNOW CMD !!!\n");
	}
	
	xostrace_linux_kernel_response_ready();

	return count;

}


static int xos_trace_modem_app_create_procfs_files ( void )
{
	int error=1;

	modem_app_cmd_proc_file = xos_trace_create_procfs_file(root_proc_file,MODEM_APP_CMD_PROCFS_NAME, \
			modem_app_cmd_procfile_read, modem_app_cmd_procfile_write);

	if (!modem_app_cmd_proc_file)
	{
		error=0;
	}

	return error;

}

static int xos_trace_modem_spy_create_procfs_files ( void )
{
	int error=1;

	modem_spy_cmd_proc_file = xos_trace_create_procfs_file(root_proc_file,MODEM_SPY_CMD_PROCFS_NAME, \
			modem_spy_cmd_procfile_read, modem_spy_cmd_procfile_write);

	if (!modem_spy_cmd_proc_file)
	{
		error=0;
	}

	return error;
}


static int xos_trace_linux_kern_create_procfs_files ( void )
{
	int error = 1;

	linux_kern_cmd_proc_file = xos_trace_create_procfs_file(root_proc_file,LINUX_KERNEL_CMD_PROCFS_NAME, \
			linux_kern_cmd_procfile_read, linux_kern_cmd_procfile_write);

	if (!linux_kern_cmd_proc_file)
	{
		error=0;
	}

	return error;

}

static int xos_trace_timestamp_create_procfs_files(void)
{
	int error=1;
	
	time_stamp_proc_file = 	xos_trace_create_procfs_file(root_proc_file,TIME_STAMP_PROCFS_NAME, \
			xos_trace_timestamp_procfile_read, NULL);
	if (!time_stamp_proc_file)
	{
		error = 0;
	}
	return error;
}


void xos_trace_kernel_process_init(void)
{
	int i;

	for(i=0; i<XOSTRACE_MAX_PROCESS;i++)
	{
		xos_trace_process_list[i].bfield_status.free=0;
	}
}


int xos_trace_init_modem_app(void)
{

	/* ctrl event registering */
	xos_ctrl_register(xos_trace_data.request,XOSTRACE_APP_DATA_READY, xostrace_app_data_ready, (void*) &xos_trace_data.request, 1);

	/* ctrl event registering */
	xos_ctrl_register(xos_trace_data.request,XOSTRACE_APP_SEND_CMD, xostrace_app_response_ready, (void*) &xos_trace_data.request, 1);


	/* init modem app ring buffer pointer */
	modem_app_ring_buffer = &(xos_trace_data.base_adr->modem_app.ring_buffer);

	/* wake up mechanisme when data are received from modem */
	init_waitqueue_head(&modem_app_buffer_empty); 
	init_waitqueue_head(&modem_app_wait_response); 

	xos_trace_data.base_adr->modem_app.cmd.status = 0;


	/* creation of modem app procfs */
	xos_trace_modem_app_create_procfs_files();

	return 1;
}


int xos_trace_init_modem_spy(void)
{

	/* ctrl event registering */
	xos_ctrl_register(xos_trace_data.request,XOSTRACE_SPY_DATA_READY, xostrace_spy_data_ready, (void*) &xos_trace_data.request, 1);
	xos_ctrl_register(xos_trace_data.request,XOSTRACE_SPY_BUFFER_FULL,xostrace_spy_buffer_full,(void*) &xos_trace_data.request, 1); 
	xos_ctrl_register(xos_trace_data.request,XOSTRACE_SPY_SEND_CMD,xostrace_spy_response_ready,(void*) &xos_trace_data.request, 1); 


	/* init modem spy ring buffer pointer */
	modem_spy_ring_buffer = &(xos_trace_data.base_adr->modem_spy.ring_buffer);

	/* wake up mechanisme when data are received from modem */
	init_waitqueue_head(&modem_spy_buffer_empty); 
	init_waitqueue_head(&modem_spy_wait_response); 
	xos_trace_data.base_adr->modem_spy.cmd.status = 0;

	/* creation of modem spy procfs */
	xos_trace_modem_spy_create_procfs_files();

	return 1;

}

int xos_trace_modem_init(void)
{
	int err=1;

	/* get area */
	xos_trace_data.shared = xos_area_connect(XOS_TRACE_AREA_NAME, sizeof(struct xos_trace_mngt_shared));

	if (xos_trace_data.shared)
	{
		xos_trace_data.base_adr = xos_area_ptr(xos_trace_data.shared);

		/* connection of modem spy xos ctrl */	
		xos_trace_data.request=xos_ctrl_connect(XOS_TRACE_AREA_NAME,MAX_XOSTRACE_EVENT);

		if (xos_trace_data.request == NULL)
		{
			printk(PKMOD "connection of modem spy xos ctrl to wrapper function failed : use xoscore 1.2\n");
			err=0;
		}
		else
		{
			/* init Modem App Trace */
			if (!xos_trace_init_modem_app())
			{
				err=0;
			}

			/* init Modem Spy Trace */
			if (!xos_trace_init_modem_spy())
			{
				err=0;
			}
		}
	}
	else
	{
		printk(PKMOD "failed to connect to xos area\n");
		err=0;
	}

	return err;
}




void linux_kernel_full_buffer_call_back(void)
{
	//	xos_ctrl_raise(xos_trace_data.request, XOSTRACE_APP_BUFFER_FULL );
}



int xos_trace_init_linux_kernel(void)
{

	linux_kern_buff_mngt.ready = 0;
	linux_kern_ring_buffer.rd = 0;
	linux_kern_ring_buffer.wr = 0;
	linux_kern_buff_mngt.ring_buffer = &linux_kern_ring_buffer;

	init_waitqueue_head(&(linux_kern_buff_mngt.queue_empty));
	spin_lock_init(&(linux_kern_buff_mngt.lock));

	linux_kern_buff_mngt.buffer_was_full=0;

	linux_kern_buff_mngt.full_buffer_call_back = linux_kernel_full_buffer_call_back;


	init_ring_buffer(linux_kern_buff_mngt.ring_buffer,LINUX_KERN_BUFFER_LEN);
	linux_kern_buff_mngt.ready=1;

	/* creation of modem app procfs */
	xos_trace_linux_kern_create_procfs_files();
	xos_trace_cmd.status = 0;
	init_waitqueue_head(&linux_kernel_cmd_wait_response); 

	xos_trace_kernel_process_init();

	return 1;
}



/* 
 * This is called whenever a process attempts to open the device file 
 */
static int device_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "device_open(%p)\n", file);
	/* 
	 * We don't want to talk to two processes at the same time 
	 */
	try_module_get(THIS_MODULE);
	return 0;
}


/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t spy_device_read(struct file *file,	/* see include/linux/fs.h   */
			   char __user * buffer,	/* buffer to be
							 * filled with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	/* 
	 * Number of bytes actually written to the buffer 
	 */

	u32 len, idx_rd, nb_read;

	len = xos_trace_get_circular_buffer_length(modem_spy_ring_buffer);

	if (len==0)
	{
		/* if ring buffer is empty, then wait signal from the modem */
		wait_event(modem_spy_buffer_empty, (modem_spy_ring_buffer->rd != modem_spy_ring_buffer->wr) );

		len = xos_trace_get_circular_buffer_length(modem_spy_ring_buffer);
	}

	if (length > len)
	{
		nb_read = len;
	}
	else
	{
		nb_read=length;
	}

	if (nb_read > 0)
	{
		/* copy ring buffer data to user buffer */
		idx_rd = xos_trace_circular_into_linear_buffer(buffer, \
				nb_read,\
				modem_spy_ring_buffer->va_buffer,\
				modem_spy_ring_buffer->rd,\
				modem_spy_ring_buffer->buf_size);


		/* update read index of ring buffer */
		modem_spy_ring_buffer->rd = idx_rd;
	}

	/* if we received full buffer indication */
	if (modem_spy_buffer_was_full)
	{
		/* wait empty buffer TBD */
		if (modem_spy_ring_buffer->rd == modem_spy_ring_buffer->wr)
		{
			/* Send signal to modem that buffer is now not full */
			modem_spy_buffer_was_full=0;
			xos_ctrl_raise(xos_trace_data.request, XOSTRACE_SPY_BUFFER_FULL );
		}

	}
	return nb_read;

}

/**
 * User interface to read Modem App traces
 *
 *
 */

/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t app_device_read(struct file *file,	/* see include/linux/fs.h   */
			   char __user * buffer,	/* buffer to be
							 * filled with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	u32 len, idx_rd, nb_read;

	if (length==0)
	{
		return 0;
	}

	len = xos_trace_get_circular_buffer_length(modem_app_ring_buffer);
	if (len==0)
	{
		wait_event(modem_app_buffer_empty, (modem_app_ring_buffer->rd != modem_app_ring_buffer->wr) );

		len = xos_trace_get_circular_buffer_length(modem_app_ring_buffer);
	}

	nb_read = length;

	if (nb_read > len)
	{
		nb_read = len;
	}

	idx_rd = xos_trace_circular_into_linear_buffer(buffer, \
			nb_read,\
			modem_app_ring_buffer->va_buffer,\
			modem_app_ring_buffer->rd,\
			modem_app_ring_buffer->buf_size);

	/* update read index of ring buffer */
	modem_app_ring_buffer->rd = idx_rd;

	/* if we received buffer indication */
	if (modem_app_buffer_was_full)
	{
		/* wait empty buffer TBD */
		if (modem_app_ring_buffer->rd == modem_app_ring_buffer->wr)
		{
			modem_app_buffer_was_full=0;


			xos_ctrl_raise(xos_trace_data.request, XOSTRACE_APP_BUFFER_FULL );
		}

	}
	return nb_read;

}


/* 
 * This function is called whenever a process which has already opened the
 * device file attempts to read from it.
 */
static ssize_t kernel_device_read(struct file *file,	/* see include/linux/fs.h   */
			   char __user * buffer,	/* buffer to be
							 * filled with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)

{
	u32 len, idx_rd, nb_read;
	struct xos_trace_buffer_mngt *buff_mngt=&linux_kern_buff_mngt;

	if (length==0)
	{
		return 0;
	}
	
	spin_lock(&buff_mngt->lock);

	len = xos_trace_get_circular_buffer_length(buff_mngt->ring_buffer);
	if (len==0)
	{
		spin_unlock(&buff_mngt->lock);

		wait_event(buff_mngt->queue_empty, (buff_mngt->ring_buffer->rd != buff_mngt->ring_buffer->wr) );

		spin_lock(&buff_mngt->lock);

		len = xos_trace_get_circular_buffer_length(buff_mngt->ring_buffer);
	}

	nb_read = length;

	if (nb_read > len)
	{
		nb_read = len;
	}

	idx_rd = xos_trace_circular_into_linear_buffer(buffer, \
			nb_read,\
			buff_mngt->ring_buffer->va_buffer,\
			buff_mngt->ring_buffer->rd,\
			buff_mngt->ring_buffer->buf_size);

	/* update read index of ring buffer */
	buff_mngt->ring_buffer->rd = idx_rd;

	if (buff_mngt->buffer_was_full)
	{
		/* wait empty buffer TBD */
		if (buff_mngt->ring_buffer->rd == buff_mngt->ring_buffer->wr)
		{
			buff_mngt->buffer_was_full=0;
			(buff_mngt->full_buffer_call_back)();
		}
	}
	spin_unlock(&buff_mngt->lock);

	return nb_read;

}


struct file_operations spy_fops = {
	.read = spy_device_read,
	.open = device_open,
};

struct file_operations app_fops = {
	.read = app_device_read,
	.open = device_open,
};

struct file_operations kernel_fops = {
	.read = kernel_device_read,
	.open = device_open,
};

static const struct {
	unsigned int		minor;
	char			*name;
	umode_t			mode;
	const struct file_operations	*fops;
} devlist[] = { /* list of minor devices */
	{1, "modemSpy",     S_IRUSR | S_IWUSR | S_IRGRP, &spy_fops},
	{2, "modemApp",     S_IRUSR | S_IWUSR | S_IRGRP, &app_fops},
	{3, "linuxKer",  S_IRUSR | S_IWUSR | S_IRGRP, &kernel_fops},
//	{1, "xos_trace!modemSpy",     S_IRUSR | S_IWUSR | S_IRGRP, &spy_fops},
//	{2, "xos_trace!modemApp",     S_IRUSR | S_IWUSR | S_IRGRP, &app_fops},
//	{3, "xos_trace!linuxKer",  S_IRUSR | S_IWUSR | S_IRGRP, &kernel_fops},
};


static int xostrace_open(struct inode * inode, struct file * filp)
{
	int ret = 0;

	lock_kernel();
	switch (iminor(inode)) {
		case 0:
			filp->f_op = &spy_fops;
			break;

		case 1:
			filp->f_op = &app_fops;
			break;
		case 2:
			filp->f_op = &kernel_fops;
			break;
		default:
			unlock_kernel();
			return -ENXIO;
	}
	if (filp->f_op && filp->f_op->open)
		ret = filp->f_op->open(inode,filp);
	unlock_kernel();
	return ret;
}


static const struct file_operations xostrace_fops = {
	.open		= xostrace_open,	/* just a selector for the real open */
};


/* 
 * Initialize the module - Register the character device 
 */
int init_xostrace_module(void)
{
	static struct class *mem_class;
	int major;
	
	/* 
	 * Register the character device (atleast try) 
	 */
	major = register_chrdev(MAJOR_NUM, DEVICE_NAME, &xostrace_fops);

	/* 
	 * Negative values signify an error 
	 */
	if (major < 0) {
		printk(KERN_ALERT "%s failed with %d\n",
		       "Sorry, registering the character device ", major);
		return major;
	}
	
	mem_class = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(mem_class, NULL,MKDEV(major, 0), NULL, devlist[0].name);
	device_create(mem_class, NULL,MKDEV(major, 1), NULL, devlist[1].name);
	device_create(mem_class, NULL,MKDEV(major, 2), NULL, devlist[2].name);

	return 0;
}


/* Traces init */
static int xos_init_traces(void)
{
	int err=1;

	/* init proc path */
	if (create_proc_path() )
	{
		/* init Modem trace */
		xos_trace_modem_init();

		/* init Linux kernel Trace */
		xos_trace_init_linux_kernel();

		xos_trace_timestamp_create_procfs_files();

		init_xostrace_module();
	}
	return err;
}



static int __init xos_trace_init(void)
{
	int err;

	err = xos_init_traces();

	printk(PKMOD "xos_trace_init\n");

	my_id = XOS_TRACE_REGISTER("xos_trace",0);

	return 0;
}




module_init(xos_trace_init);
