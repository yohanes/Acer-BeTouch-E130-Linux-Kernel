/*
 *  linux/arch/arm/plat-pnx/xos_trace_ringbuffer.c
 *  Gabriel Fernandez <gabriel.fernandez@stericsson.com>
 *  Copyright (C) 2010 ST-Ericsson
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

#include <linux/types.h>
#include <linux/init.h>

#include <asm/mach/time.h>

#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <asm/uaccess.h>	/* for copy_from_user */

#include <asm/proc-fns.h>

#include "xos_trace_ringbuffer.h"


int
init_ring_buffer(struct xos_trace_ring_buffer *ring_buffer, unsigned int size)
{
	/* init read & write index of the ring buffer */
	ring_buffer->rd=0;
	ring_buffer->wr=0;
	ring_buffer->buf_size = size;

	/* ring buffer allocation */
	if ((ring_buffer->va_buffer = kmalloc(size, GFP_KERNEL)) == NULL)
	{
		printk(KERN_WARNING "init_ring_buffer : kmalloc problem!\n");
		return -1;
	}

	/* set physical adress */
	ring_buffer->pa_buffer =(void *) __pa(ring_buffer->va_buffer);

	return 0;
}

unsigned int
xos_trace_get_circular_buffer_length(struct xos_trace_ring_buffer *ring_buffer)
{
	unsigned int length;

	if (ring_buffer->rd == ring_buffer->wr)
	{
		length = 0;
	}
	else if (ring_buffer->rd < ring_buffer->wr)
	{
		length = ring_buffer->wr - ring_buffer->rd;
	}
	else
	{
		length = ring_buffer->buf_size - ring_buffer->rd + ring_buffer->wr;
	}
	return length;
}

int
xos_trace_is_cicular_buffer_is_empty(struct xos_trace_ring_buffer *ring_buffer)
{
	return (ring_buffer->rd == ring_buffer->wr);
}

unsigned int
xos_trace_get_circular_buffer_free_space(
		struct xos_trace_ring_buffer *ring_buffer)
{
	unsigned int length = xos_trace_get_circular_buffer_length(ring_buffer);
	return (ring_buffer->buf_size -1 - length) ;
}

unsigned int
xos_trace_linear_to_circular_buffer(u8 *src_buffer, unsigned int size,
		struct xos_trace_ring_buffer *ring_buffer)
{
	unsigned len;

	if ( (ring_buffer->wr + size) <= ring_buffer->buf_size)
	{
		memcpy(ring_buffer->va_buffer + ring_buffer->wr, src_buffer, size);
		return ( (ring_buffer->wr + size) % ring_buffer->buf_size);
	}
	else
	{
		len = ring_buffer->buf_size - size;
		memcpy(ring_buffer->va_buffer + ring_buffer->wr, src_buffer, len);
		memcpy(ring_buffer->va_buffer, src_buffer + len, size - len);
		return (size - len);
	}

}
int
xos_trace_insert_to_circular_buffer(u8 *src_buffer, unsigned int size,
		struct xos_trace_ring_buffer *ring_buffer)
{
	unsigned int free;

	free = xos_trace_get_circular_buffer_free_space(ring_buffer);
	if (size > free)
	{
		return 0;
	}
	ring_buffer->wr = xos_trace_linear_to_circular_buffer(src_buffer,
			size,ring_buffer);
	return 1;
}

u32
xos_trace_circular_into_linear_buffer(u8 * p_dst_Buffer, u32 size,
		u8 *p_circular_buffer, u32 idx_rd, u32 Max_Size)
{
	u32 len;

	if ( (idx_rd + size) <= Max_Size)
	{
		memcpy(p_dst_Buffer, p_circular_buffer + idx_rd, size);
		return ( (idx_rd + size) % Max_Size );
	}
	else
	{
		len = Max_Size - idx_rd;

		memcpy(p_dst_Buffer, p_circular_buffer + idx_rd, len);
		memcpy(p_dst_Buffer+len, p_circular_buffer, size-len);
		return (size-len);
	}
}

u32 get_next_index(struct xos_trace_ring_buffer *buffer_mngt, u32 idx)
{
	return ( (idx + 1) % buffer_mngt->buf_size);
}

u32 get_next_u32(struct xos_trace_ring_buffer *buffer_mngt, u32 *idx)
{
	u32 index,val;
	char *buffer;
	u8 i;
	u8 buff[4];
	u32 *ptr_u32;

	index= *idx;
	buffer=buffer_mngt->va_buffer;
	for (i=0;i<sizeof(u32);i++)
	{
		buff[i] = buffer[index];
		index = get_next_index(buffer_mngt, index);
	}
	ptr_u32 = (u32 *) buff;

	val = *ptr_u32;
	*idx = index;
	return val;
}

u64 get_next_u64(struct xos_trace_ring_buffer *buffer_mngt, u32 *idx)
{
	u32 index;
	char *buffer;
	u8 i;
	u8 buff[8];
	u64 *ptr_u64;
	u64 val;

	index= *idx;
	buffer=buffer_mngt->va_buffer;
	for (i=0;i<sizeof(u64);i++)
	{
		buff[i] = buffer[index];
		index = get_next_index(buffer_mngt, index);
	}
	ptr_u64 = (u64 *) buff;
	val = *ptr_u64;
	*idx = index;
	return val;
}




u32
xos_trace_Put_u8_into_circular_buffer(u8 car, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size)
{
	p_dst_buffer[idx_wr] = car;

	return ( (idx_wr + 1) % Max_Size );
}


u32
xos_trace_Put_u32_into_circular_buffer(u32 uu32, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size)
{
	u8 *ptr_car;
	u8 i;

	ptr_car = (u8 *) &uu32;

	for (i=0;i<sizeof(u32);i++)
	{
		p_dst_buffer[idx_wr] = *ptr_car++;

		idx_wr = (idx_wr + 1) % Max_Size;
	}
	return idx_wr;
}


u32
xos_trace_Put_u64_into_circular_buffer(u64 uu64, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size)
{
	u8 *ptr_car;
	u8 i;

	ptr_car = (u8 *) &uu64;

	for (i=0;i<sizeof(u64);i++)
	{
		p_dst_buffer[idx_wr] = *ptr_car++;
		idx_wr = (idx_wr + 1) % Max_Size;
	}
	return idx_wr;
}


u32
xos_trace_Put_into_circular_buffer(u8 * p_src_Buffer, u32 size,
		u8 *p_dst_buffer, u32 idx_wr, u32 Max_Size)
{
	u32 len;

	if ( (idx_wr + size) <= Max_Size)
	{
		memcpy(&(p_dst_buffer[idx_wr]),p_src_Buffer, size);

		return ( (idx_wr + size) % Max_Size );
	}
	else
	{
		len = Max_Size - idx_wr;
		memcpy(p_dst_buffer + idx_wr, p_src_Buffer, len);

		memcpy(p_dst_buffer, p_src_Buffer+len, size-len);

		return (size-len);
	}
}

