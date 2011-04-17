
struct xos_trace_ring_buffer {
	void	*pa_buffer;			/* Physical adress */
	u8		*va_buffer;			/* Virtual adress  */
	u8		*va_buffer_rtk;		/* Virtual adress  */

	u32		buf_size;           /* size of the buffer*/
	u32		rd;					/* index for reading */
	u32		wr;					/* index for writing */
};

/*
 *  linux/arch/arm/plat-pnx/xos_trace_ringbuffer.h
 *  Gabriel Fernandez <gabriel.fernandez@stericsson.com>
 *
 *  Mechanism to evacuate pnx modem trace
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



int init_ring_buffer(struct xos_trace_ring_buffer *ring_buffer,
		unsigned int size);
unsigned int xos_trace_get_circular_buffer_length(
		struct xos_trace_ring_buffer *ring_buffer);
int xos_trace_is_cicular_buffer_is_empty(
		struct xos_trace_ring_buffer *ring_buffer);
unsigned int xos_trace_get_circular_buffer_free_space(
		struct xos_trace_ring_buffer *ring_buffer);
unsigned int xos_trace_linear_to_circular_buffer(u8 *src_buffer,
		unsigned int size, struct xos_trace_ring_buffer *ring_buffer);
int xos_trace_insert_to_circular_buffer(u8 *src_buffer,
		unsigned int size, struct xos_trace_ring_buffer *ring_buffer);
u32 xos_trace_circular_into_linear_buffer(u8 * p_dst_Buffer, u32 size,
		u8 *p_circular_buffer, u32 idx_rd, u32 Max_Size);
u32 get_next_index(struct xos_trace_ring_buffer *buffer_mngt, u32 idx);
u32 get_next_u32(struct xos_trace_ring_buffer *buffer_mngt, u32 *idx);
u64 get_next_u64(struct xos_trace_ring_buffer *buffer_mngt, u32 *idx);
u32 xos_trace_Put_u8_into_circular_buffer(u8 car, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size);
u32 xos_trace_Put_u32_into_circular_buffer(u32 uu32, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size);
u32 xos_trace_Put_u64_into_circular_buffer(u64 uu64, u8 *p_dst_buffer,
		u32 idx_wr, u32 Max_Size);
u32 xos_trace_Put_into_circular_buffer(u8 * p_src_Buffer, u32 size,
		u8 *p_dst_buffer, u32 idx_wr, u32 Max_Size);

