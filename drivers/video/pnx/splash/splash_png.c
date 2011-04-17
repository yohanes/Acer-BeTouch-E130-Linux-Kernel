/*
 * linux/drivers/video/pnx/splash/splash_png.c
 *
 * PNX PNG Splash 
 * Copyright (c) ST-Ericsson 2009
 *
 */

#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <video/pnx/lcdctrl.h>
#include <linux/zlib.h>

#include "../lcdbus_pagefaultfb.h"
#include "splash_png.h"


/*
 * NOTE - splash_png is under dev
 *
 * TODO implement RGB PNG smaller than screen (only same size is supported!)
 * TODO implement RGBA PNG (only indexed PNG with transparency is supported)
 * TODO understand why vmalloc/vfree were not working too times!
 * TODO implement animation (only with png)
 * TODO implement a better error management system
 * TODO verify CRC in the image
 * TODO add more output format (only 16 and 32 bit for the moment
 *
 *
 * */

/* PNG debug message management */
//#define PNG_DEBUG_MSG
#define PNG_ERROR_MSG


/* due to pb with kmalloc and kfree with the png animation,
 * var below have been exported from the png function.... */
//#define malloc(size)	kmalloc(size, GFP_KERNEL)
//#define free(ptr)		kfree(ptr)

struct timer_list timer_splash;
char nobody_asks_for_fb;
unsigned char *background, *img;
void *workspace;
unsigned char *input_buf;
unsigned char *compressed_buf;
unsigned char *pal;


#ifdef PNG_DEBUG_MSG
	#define png_debug(fmt,arg...) printk(fmt,##arg)
#else
	#define png_debug(fmt,arg...) do { } while (0)
#endif

#ifdef PNG_ERROR_MSG
	#define png_error(fmt,arg...) printk(fmt,##arg)
#else
	#define png_error(fmt,arg...) do { } while (0)
#endif

#define RGB888TO565(r, g, b) ((unsigned short)(((r>>3)<<11) + ((g>>2)<<5) + \
								(b>>3))) // todo no limit checks!!!
#define RGB888TO0888(r, g, b) ((unsigned int)((r<<16) + (g<<8) + (b)))


/*
 * PNG constants and types
 *
 * */
#define PNG_SIGNATURE0	0x89504E47
#define PNG_SIGNATURE1	0x0D0A1A0A
#define PNG_IHDR		0x49484452
#define PNG_PLTE		0x504C5445
#define PNG_IDAT		0x49444154
#define PNG_IEND		0x49454E44

#define PNG_CHRM		0x4348524D // upper case
#define PNG_GAMA		0x47414D41 // upper case
#define PNG_SBIT		0x53424954 // upper case
#define PNG_BKGD		0x424B4744 // upper case
#define PNG_HIST		0x48495354 // upper case
#define PNG_TRNS		0x54524E53 // upper case
#define PNG_PHYS		0x50485953 // upper case
#define PNG_TIME		0x54494D45 // upper case
#define PNG_TEXT		0x54455854 // upper case
#define PNG_ZTXT		0x5A545854 // upper case


enum png_color_type {PNG_CT_GRAYSCALE, PNG_CT_NOTUSED1, PNG_CT_RGB,
	PNG_CT_INDEXED, PNG_CT_GRAYSCALE_ALPHA, PNG_CT_NOTUSED5, PNG_CT_RGBA};

enum png_filter {PNG_FILTER_NONE, PNG_FILTER_SUB, PNG_FILTER_UP,
	PNG_FILTER_AVG, PNG_FILTER_PAETH};

struct struct_IHDR_data {
	unsigned int width;
	unsigned int height;
	unsigned char bit_depth;
	enum png_color_type color_type;
	unsigned char compression_method;
	unsigned char filter_method;
	unsigned char interlace_method;
} IHDR_data;

struct struct_IDAT_data {
	unsigned char compression_method;
	unsigned char flags;
} IDAT_data;


/*
 * Helper functions
 *
 * */
inline unsigned int read32(unsigned char *data, unsigned int *ofs)
{
	unsigned int i;

	i = (data[*ofs] << 24) | (data[*ofs + 1] << 16) |
		(data[*ofs + 2] << 8) | data[*ofs+3];
	*ofs += 4;
	return i;
}

inline unsigned short read16(unsigned char *data, unsigned int *ofs)
{
	unsigned short i;

	i = (data[*ofs] << 8) | data[*ofs + 1];
	*ofs += 2;
	return i;
}

inline unsigned char read8(unsigned char *data, unsigned int *ofs)
{
	unsigned char i;

	i = data[*ofs];
	*ofs += 1;
	return i;
}

inline unsigned char PaethPredictor(int a, int b, int c)
{
	int p, pa, pb, pc;

	// a = left, b = above, c = upper left
	p = a + b - c; // initial estimate
	pa = abs(p - a); // distances to a, b, c
	pb = abs(p - b);
	pc = abs(p - c);
	// return nearest of a,b,c,
	// breaking ties in order a,b,c.
	if ((pa <= pb) && (pa <= pc))
		return a;
	else if (pb <= pc)
		return b;
	else
		return c;
}

/*
 *  is_png -
 *
 *  @return: 1 if the buffer contains a PNG else 0
 *
 */
int is_png(unsigned char *data) {
	unsigned int ofs, pngid0, pngid1;

	ofs = 0;
	pngid0 = read32(data, &ofs);
	pngid1 = read32(data, &ofs);

	if ((pngid0 != PNG_SIGNATURE0) || (pngid1 != PNG_SIGNATURE1))
		return 0;
	else
		return 1;
}




/*
 * decode_png - decode a png file and write it in a 16 or 32bpp output buffer
 * taking into account the transparency or the alpha channel...
 *
 * @input: png data
 * @output: may contain a background image (in case of alpha or transparency)
 * @output_width: png width image has to be smaller than output width
 * @output_height: png height image has to be smaller than output height
 * @output_bpp: only 16 and 32bpp are supported (TODO extend this)
 * @return: return value contains offset to the end of the png image
 * means to the next PNG if several png images are concateneted
 */
signed int decode_png(unsigned char *input, unsigned char *output,
		unsigned int output_width, unsigned int output_height,
		unsigned int output_bpp)
{
	register unsigned int i, x, y;

	/* png chunck management var */
	unsigned int filesize_bytes;
	unsigned char *data;
	signed int ofs, ret;
	unsigned int pngid0, pngid1;
   	unsigned int chunk_size, chunk_type, chunk_crc;

	/* color management var */
	unsigned short *pal16 = NULL;
	unsigned int *pal32 = NULL;
	unsigned int pal_colors, r, g, b;
	unsigned int trans_col_index = 0;
	unsigned int col8;
	unsigned int trans_flag;

	/* filter management var
	 * bpp = bytes per pixel, bpl = bytes per line */
	unsigned int input_buf_size;
	unsigned int input_buf_ofs, input_buf_ofs_wr;
	unsigned char *ibr, *ibw, *ibw_bpp, *ibw_bpl, *ibw_bpp_bpl;
	enum png_filter row_filter;
	unsigned int bytes_per_pixel, bits_per_pixel, coef_per_pixel;
	unsigned int bytes_per_line;

	/* output var */
	unsigned short *output16;
	unsigned int *output32;
	unsigned int output_bytepp;
	unsigned int same_size;
	unsigned int xstride;

	/* zlib management var */
	z_stream stream;
	unsigned int compressed_buf_size;
	unsigned int compressed_buf_ofs;

/*
	data = NULL;
	pal = NULL;
	pal16 = NULL;
	pal32 = NULL;
	input_buf = NULL;
	compressed_buf = NULL;*/

	data = input;
	filesize_bytes = SPLASH_MAXPNGFILESIZE;

/* PNG IDENTIFICATION */
	ofs = 0;
	pngid0 = read32(data, &ofs);
	if (pngid0 != PNG_SIGNATURE0) goto not_a_png;

	pngid1 = read32(data, &ofs);
	if (pngid1 != PNG_SIGNATURE1) goto not_a_png;

	png_debug("PNG SIGNATURE FOUND\n");

/* IHDR */
	chunk_size = read32(data, &ofs);
	chunk_type = read32(data, &ofs) & (~0x20202020); /* upper case */
	if (chunk_type != PNG_IHDR) goto bad_png;
	png_debug("IHDR FOUND\n");

	IHDR_data.width = read32(data, &ofs);
	IHDR_data.height = read32(data, &ofs);
	IHDR_data.bit_depth = read8(data, &ofs);
	IHDR_data.color_type = (enum png_color_type) (read8(data, &ofs));
	IHDR_data.compression_method = read8(data, &ofs);
	IHDR_data.filter_method = read8(data, &ofs);
	IHDR_data.interlace_method = read8(data, &ofs);

	png_debug("%dx%d, ", IHDR_data.width, IHDR_data.height);

	/* Check the image size compared to the output buffer */
	if ((IHDR_data.width == output_width) &&
		(IHDR_data.height == output_height))
		same_size = 1;
	else if ((IHDR_data.width > output_width) ||
			(IHDR_data.height > output_height)) {
		png_error("PNG image (%dx%d) is bigger than output buffer (%dx%d)\n",
			IHDR_data.width, IHDR_data.height, output_width, output_height);
		goto format_not_implemented;
	} else
		same_size = 0;

	/* Color format verification (not all a supported here) */
	coef_per_pixel = 1;
	switch(IHDR_data.color_type) {
	case PNG_CT_GRAYSCALE:
		png_debug("G (Gray) %dbit/pixel\n", IHDR_data.bit_depth);
		if (IHDR_data.bit_depth != 8) {
			png_error("G (Gray) %dbit/pixel\n", IHDR_data.bit_depth);
			goto format_not_implemented;
		}
		coef_per_pixel *= 1; // FIXME maybe not compatible with bit_depth != 8
		break;

	case PNG_CT_RGB:
		png_debug("RBG %dbit/pixel\n", IHDR_data.bit_depth * 3);
		if (IHDR_data.bit_depth != 8) {
			png_error("RGB %dbit/pixel\n", IHDR_data.bit_depth);
			goto format_not_implemented;
		}
		coef_per_pixel *= 3; // FIXME maybe not compatible with bit_depth != 8
		break;

	case PNG_CT_INDEXED:
		png_debug("Palette %dbit/index\n", IHDR_data.bit_depth);
		if (IHDR_data.bit_depth != 8) {
			png_error("Palette %dbit/pixel\n", IHDR_data.bit_depth);
			goto format_not_implemented;
		}
		coef_per_pixel *= 1; // FIXME maybe not compatible with bit_depth != 8
		break;

	case PNG_CT_GRAYSCALE_ALPHA:
		png_debug("GA (Gray+Alpha) %dbit/pixel\n", IHDR_data.bit_depth * 2);
		if (IHDR_data.bit_depth != 8) {
			png_error("GA (Gray+Alpha) %dbit/pixel\n", IHDR_data.bit_depth);
			goto format_not_implemented;
		}
		coef_per_pixel *= 2; // FIXME maybe not compatible with bit_depth != 8
		break;

	case PNG_CT_RGBA:
		png_debug("RBGA %dbit/pixel\n", IHDR_data.bit_depth * 4);
		if (IHDR_data.bit_depth != 8) {
			png_error("RGBA %dbit/pixel\n", IHDR_data.bit_depth);
			goto format_not_implemented;
		}
		coef_per_pixel *= 4; // FIXME maybe not compatible with bit_depth != 8
		break;

	default:
		png_error("Color type (%d) not compatible with PNG standard\n",
				IHDR_data.color_type);
		goto bad_png;
		break;
	}

	bits_per_pixel = IHDR_data.bit_depth * coef_per_pixel;
	bytes_per_pixel = ((IHDR_data.bit_depth * coef_per_pixel) + 7)  >> 3;
	bytes_per_line = IHDR_data.width * coef_per_pixel;

	png_debug("bits_per_pixel=%d, bytes_per_pixel=%d, bytes_per_line=%d\n",
			bits_per_pixel, bytes_per_pixel, bytes_per_line);

	/* Check data compression, filter and interlace methods */
	if (IHDR_data.compression_method != 0) {
		png_error("Compression method (%d) not compatible PNG standard\n",
			   IHDR_data.compression_method);
		goto bad_png;
	}

	if (IHDR_data.filter_method != 0) {
		png_error("Filter method (%d) not compatible PNG standard\n",
			   IHDR_data.filter_method);
		goto bad_png;
	}

	if (IHDR_data.interlace_method != 0) {
		png_error("Adam7 interlace not implemented\n");
		goto format_not_implemented;
	}

	/* size of the decompressed image buffer
	 * +1 because of the filter byte before each line */
	input_buf_size = IHDR_data.height * (bytes_per_line + 1);
	//input_buf = (unsigned char*)vmalloc(input_buf_size);
	input_buf_ofs = 0;
	/*png_debug("input_buf_size=%d", input_buf_size);*/
	//if (input_buf == NULL)
	//	goto not_enough_memory_input_buf;
	//memset(input_buf, 0, input_buf_size);// TODO is it really necessary?

	chunk_crc = read32(data, &ofs); // TODO CRC are not verified

	/* size of the compressed image buffer */
	compressed_buf_size = filesize_bytes; // FIXME too big margin for malloc?
	compressed_buf_ofs = 0;
	//compressed_buf = (unsigned char *)kmalloc(compressed_buf_size,
	//                  GFP_KERNEL);
	//if (compressed_buf == NULL)
	//	goto not_enough_memory_compressed_buf;


/* PLTE / TRNS / IDAT / IEND chunk checks */
	trans_flag = 0;
	do {
		chunk_size = read32(data, &ofs);
		chunk_type = read32(data, &ofs) & (~0x20202020); /* upper case */

		switch (chunk_type) {
		case PNG_PLTE:
			png_debug("PLTE FOUND\n");
			if ((chunk_size % 3) != 0)
				goto bad_png;

			pal_colors = chunk_size / 3;
			//pal = (unsigned char *)kmalloc(chunk_size, GFP_KERNEL);
			//if (pal == NULL)
			//	goto not_enough_memory_pal;

			/* Compute the palette in function of the output format */
			if (output_bpp == 16) {
				pal16 = (unsigned short *)(pal);
				for(i = 0; i < pal_colors; i++) {
					r = read8(data, &ofs);
					g = read8(data, &ofs);
					b = read8(data, &ofs);
					pal16[i] = RGB888TO565(r, g, b);
				}
			} else if (output_bpp == 32) {
				pal32 = (unsigned int *)(pal);
				for(i = 0; i < pal_colors; i++) {
					r = read8(data, &ofs);
					g = read8(data, &ofs);
					b = read8(data, &ofs);
					pal32[i] = RGB888TO0888(r, g, b);
				}
			}
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;

		case PNG_TRNS:
			png_debug("tRNS FOUND\n");
			if ((IHDR_data.color_type == PNG_CT_GRAYSCALE_ALPHA) ||
				(IHDR_data.color_type == PNG_CT_RGBA))
				goto bad_png;
			if ((IHDR_data.color_type == PNG_CT_GRAYSCALE) ||
				(IHDR_data.color_type == PNG_CT_RGB)) {
				png_error("Transparency not yet implemented for GRAYSCALE "
						  "and PNG_CT_RGB\n");
				goto format_not_implemented;
			}
			if (chunk_size != 1) {
				png_error("Several transparency index, not yet implemented\n");
				goto format_not_implemented;
			}
			trans_col_index = (unsigned int) (read8(data, &ofs));
			trans_flag = 1;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;

		case PNG_IDAT:
			png_debug("IDAT FOUND\n");
			memcpy(&compressed_buf[compressed_buf_ofs], &data[ofs], chunk_size);
			compressed_buf_ofs += chunk_size;
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;

		case PNG_IEND:
			png_debug("IEND FOUND\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;


		case PNG_CHRM:
			png_debug("cHRM FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_GAMA:
			png_debug("gAMA FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_SBIT:
			png_debug("sBIT FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_BKGD:
			png_debug("bKGD FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_HIST:
			png_debug("hIST FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_PHYS:
			png_debug("pHYS FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_TIME:
			png_debug("tIME FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_TEXT:
			png_debug("tEXT FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		case PNG_ZTXT:
			png_debug("zTXT FOUND, but not used\n");
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;

		default:
			png_debug("CHUNK 0x%X FOUND but not used in this sw\n", chunk_type);
			ofs += chunk_size;
			chunk_crc = read32(data, &ofs); // TODO CRC are not verified
			break;
		}

		/* detect if a pb is in the png and go out the loop if necessary */
		if ((ofs >= filesize_bytes) ||
			(compressed_buf_ofs >= compressed_buf_size)) {
				png_debug("ofs=%d, filesize_bytes=%d, compressed_buf_ofs=%d, "
						"compressed_buf_size=%d\n",
						ofs, filesize_bytes, compressed_buf_ofs,
						compressed_buf_size);
				break;
		}

	} while(chunk_type != PNG_IEND);

	png_debug("ofs=%d, filesize_bytes=%d, compressed_buf_ofs=%d, "
			"compressed_buf_size=%d\n",
			ofs, filesize_bytes, compressed_buf_ofs, compressed_buf_size);

	/* return value contains offset to next PNG */
	ret = ofs;

/* ZLIB inflate */
	stream.next_in = compressed_buf;
	stream.avail_in = compressed_buf_ofs;
	stream.next_out = input_buf;
	stream.avail_out = input_buf_size;
	stream.data_type = Z_BINARY;

	/* The memory requirements for inflate are (in bytes) 1 << windowBits
	 * that is, 32K for windowBits=15 (default value) plus a few kilobytes
	 * for small objects.
	 * see zconf.h
	 * */
	stream.workspace = workspace;
	//stream.zalloc = Z_NULL;
	//stream.zfree = Z_NULL;

	i = zlib_inflateInit(&stream);
	if (i != Z_OK) {
		png_error("inflateInit (output code %d)\n", i);
		goto error_inflate;
	}

	do {
		i = zlib_inflate(&stream, Z_SYNC_FLUSH);
		if (i == Z_STREAM_END) {
			if (stream.avail_out || stream.avail_in) {
				png_error("Extra compressed data\n");
				//goto error_inflate;
			}
			break;
		}
		if (i != Z_OK) {
			png_error("Decompression error\n");
			goto error_inflate;
		}
   } while (stream.avail_out);

	png_debug("inflate (output code %d, msg=%s, total_in=%ld, total_out=%ld, "
			"avail_in=%d, avail_out=%d)\n",
			i, stream.msg, stream.total_in, stream.total_out, stream.avail_in,
			stream.avail_out);

	if (i != Z_STREAM_END) {
		png_error("inflate (output code %d)\n", i);
		goto error_inflate;
	}

	i = zlib_inflateEnd(&stream);
	if (i != Z_OK) {
		png_error("inflateEnd (output code %d)\n", i);
		goto error_inflate;
	}

	png_debug("Inflate done\n");

	input_buf_ofs = 0;
	input_buf_ofs_wr = 0;

/* FILTER management */
	/* TODO maybe it is faster to work with int + 0xFF mask instead of bytes */
	for(y = 0; y < IHDR_data.height; y++) {
		row_filter = (enum png_filter) (input_buf[input_buf_ofs]);
		input_buf_ofs++;

		switch (row_filter) {
			case PNG_FILTER_NONE:
				png_debug("%3d - PNG_FILTER_NONE\n", y);
				memcpy(&input_buf[input_buf_ofs_wr],
						&input_buf[input_buf_ofs], bytes_per_line);

				input_buf_ofs_wr += bytes_per_line;
				input_buf_ofs += bytes_per_line;
				break;

			case PNG_FILTER_SUB:
				png_debug("%3d - PNG_FILTER_SUB\n", y);
				/* 1st pixel */
				ibr = &input_buf[input_buf_ofs];
				ibw = &input_buf[input_buf_ofs_wr];
				ibw_bpp = &input_buf[input_buf_ofs_wr];
				/* 1st "for" will increase ibw so ibw_bpp will be correctly
				 * setup compared to ibw */
				for (x = 0; x < bytes_per_pixel; x++)
					*ibw++ = *ibr++;
				for (x = bytes_per_pixel; x < bytes_per_line; x++)
					*ibw++ = (*ibr++) + (*ibw_bpp++);

				input_buf_ofs_wr += bytes_per_line;
				input_buf_ofs += bytes_per_line;
				break;

			case PNG_FILTER_UP:
	      		png_debug("%3d - PNG_FILTER_UP\n", y);
				/* 1st line */
				ibr = &input_buf[input_buf_ofs];
				ibw = &input_buf[input_buf_ofs_wr];
				ibw_bpl = &input_buf[input_buf_ofs_wr - bytes_per_line];

				if (y == 0)
					for (x = 0; x < bytes_per_line; x++)
						*ibw++ = *ibr++;
				else
					for (x = 0; x < bytes_per_line; x++)
						*ibw++ = (*ibr++) + (*ibw_bpl++);

				input_buf_ofs_wr += bytes_per_line;
				input_buf_ofs += bytes_per_line;
				break;

			case PNG_FILTER_AVG:
				png_debug("%3d - PNG_FILTER_AVG\n", y);
				ibr = &input_buf[input_buf_ofs];
				ibw = &input_buf[input_buf_ofs_wr];
				ibw_bpp = &input_buf[input_buf_ofs_wr];
				ibw_bpl = &input_buf[input_buf_ofs_wr - bytes_per_line];
				/* 1st "for" will increase ibw so ibw_bpp will be correctly
				 * setup compared to ibw */

				if (y == 0) {
					for (x = 0; x < bytes_per_pixel; x++)
						*ibw++ = *ibr++;
					for (x = bytes_per_pixel; x < bytes_per_line; x++)
						*ibw++ = (*ibr++) + ((*ibw_bpp++) >> 1);
				} else {
					for (x = 0; x < bytes_per_pixel; x++)
						*ibw++ = (*ibr++) + ((*ibw_bpl++) >> 1);
					for (x = bytes_per_pixel; x < bytes_per_line; x++)
						*ibw++ = (*ibr++) + (unsigned char)
							((int)((int)((*ibw_bpp++) +
									(*ibw_bpl++)) >> 1));
				}

				input_buf_ofs_wr += bytes_per_line;
				input_buf_ofs += bytes_per_line;
				break;

			case PNG_FILTER_PAETH:
				png_debug("%3d - PNG_FILTER_PAETH\n", y);
				ibr = &input_buf[input_buf_ofs];
				ibw = &input_buf[input_buf_ofs_wr];
				ibw_bpp = &input_buf[input_buf_ofs_wr];
				ibw_bpl = &input_buf[input_buf_ofs_wr - bytes_per_line];
				ibw_bpp_bpl = &input_buf[input_buf_ofs_wr - bytes_per_line];
				/* 1st "for" will increase ibw so ibw_bpp will be correctly
				 * setup compared to ibw (same comment for ibw_bpp_bpl) */

				if (y == 0) {
					for (x = 0; x < bytes_per_pixel; x++)
						*ibw++ = *ibr++;

					for (x = bytes_per_pixel; x < bytes_per_line; x++)
						*ibw++ = (*ibr++) + (*ibw_bpp++);

				} else {
					for (x = 0; x < bytes_per_pixel; x++)
						*ibw++ = (*ibr++) + (*ibw_bpl++);

					for (x = bytes_per_pixel; x < bytes_per_line; x++)
						*ibw++ = (*ibr++) + PaethPredictor(*ibw_bpp++,
								*ibw_bpl++, *ibw_bpp_bpl++);
				}

				input_buf_ofs_wr += bytes_per_line;
				input_buf_ofs += bytes_per_line;
				break;

			default:
				png_error("%3d - BAD PNG FILTER!\n", y);
				goto bad_png;
				break;
		} /* switch case */
	} /* for */

	png_debug("Filtering done\n");


/* BLIT THE PNG IN THE OUTPUT BUFFER */
	input_buf_ofs = 0;
	ibr = input_buf;
	// FIXME ok pour 16&32, but compatible with 18bit?
	output_bytepp = output_bpp >> 3;

/* 16 BPP*/
	if (output_bpp == 16) {
		output16 = (unsigned short *)(output);
		switch(IHDR_data.color_type) {
		case PNG_CT_GRAYSCALE:
			png_error("PNG_CT_GRAYSCALE output not implemented\n");
			break;

		case PNG_CT_RGB:
			if (same_size) {
				for(i = 0; i < output_height * output_width; i++) {
					*output16++ = RGB888TO565(*ibr, *(ibr+1), *(ibr+2));
					ibr += 3;
				}
			} else {
				png_error("image smaller than the screen in PNG_CT_RGB "
						"not supported yet\n");
			}
			break;

		case PNG_CT_INDEXED:
			if (same_size) {
				/* decoded image and background image have the same size */
				if (trans_flag) {
					/* transparency in the image */
					for(i = 0; i < output_width * output_height; i++) {
						col8 = *ibr++;
						if (col8 == trans_col_index)
							output16++;
						else
							*output16++ = pal16[col8];
					}
				} else {
					for(i = 0; i < output_width * output_height; i++)
						*output16++ = pal16[*ibr++];
				}
			} else {
				/* image has to be centered */
				output16 += ((output_width - IHDR_data.width) / 2 +
						((output_height - IHDR_data.height) / 2) *
						output_width);
				xstride = output_width - IHDR_data.width;
				if (trans_flag) {
					/* transparency in the image */
					for(y = 0; y < IHDR_data.height; y++) {
						for(x = 0; x < IHDR_data.width; x++) {
							col8 = *ibr++;
							if (col8 == trans_col_index)
								output16++;
							else
								*output16++ = pal16[col8];
						}
						output16 += xstride;
					}
				} else {
					for(y = 0; y < IHDR_data.height; y++) {
						for(x = 0; x < IHDR_data.width; x++)
							*output16++ = pal16[*ibr++];
						output16 += xstride;
					}
				}

			}
			break;

		case PNG_CT_GRAYSCALE_ALPHA:
			png_error("PNG_CT_GRAYSCALE_ALPHA output not implemented\n");
			break;

		case PNG_CT_RGBA:
			png_error("PNG_CT_RGBA output not implemented\n");
			break;

		default:
			png_error("Format not correct!\n");
			break;
		}

	} else if (output_bpp == 32) {
/* 32 BPP */
		output32 = (unsigned int *)(output);
		switch(IHDR_data.color_type) {
		case PNG_CT_GRAYSCALE:
			png_error("PNG_CT_GRAYSCALE output not implemented\n");
			break;

		case PNG_CT_RGB:
			if (same_size) {
				for(i = 0; i < output_height * output_width; i++) {
					*output32++ = RGB888TO0888(*ibr, *(ibr+1), *(ibr+2));
					ibr += 3;
				}
			} else {
				png_error("image smaller than the screen in PNG_CT_RGB "
						"not supported\n");
			}
			break;

		case PNG_CT_INDEXED:
			if (same_size) {
				/* decoded image and background image have the same size */
				if (trans_flag) {
					/* transparency in the image */
					for(i = 0; i < output_width * output_height; i++) {
						col8 = *ibr++;
						if (col8 == trans_col_index)
							output32++;
						else
							*output32++ = pal32[col8];
					}
				} else {
					for(i = 0; i < output_width * output_height; i++)
						*output32++ = pal32[*ibr++];
				}
			} else {
				/* image has to be centered */
				output32 += ((output_width - IHDR_data.width) / 2 +
						((output_height - IHDR_data.height) / 2) *
						output_width);
				xstride = output_width - IHDR_data.width;
				if (trans_flag) {
					/* transparency in the image */
					for(y = 0; y < IHDR_data.height; y++) {
						for(x = 0; x < IHDR_data.width; x++) {
							col8 = *ibr++;
							if (col8 == trans_col_index)
								output32++;
							else
								*output32++ = pal32[col8];
						}
						output32 += xstride;
					}
				} else {
					for(y = 0; y < IHDR_data.height; y++) {
						for(x = 0; x < IHDR_data.width; x++)
							*output32++ = pal32[*ibr++];
						output32 += xstride;
					}
				}

			}
			break;

		case PNG_CT_GRAYSCALE_ALPHA:
			png_error("PNG_CT_GRAYSCALE_ALPHA output not implemented\n");
			break;

		case PNG_CT_RGBA:
			png_error("PNG_CT_RGBA output not implemented\n");
			break;

		default:
			png_error("Format not correct!\n");
			break;
		}

	} else {
		png_error("Only 16 and 32 bpp outputs are supported\n");
	}

/* Everything goes well... */
	goto out;

/* Error management and messages */

not_a_png:
	png_error("Not a PNG file.\n");
	ret = (-1);
	goto out;

bad_png:
	png_error("Bad PNG (CRC error, format error...).\n");
	ret = (-1);
	goto out;

format_not_implemented:
	png_error("\nPNG Format not implemented.\n");
	ret = (-1);
	goto out;

/*
not_enough_memory_input_buf:
	png_error("\nNot enough memory.\n");
	ret = (-1);
	goto out;

not_enough_memory_compressed_buf:
	png_error("\nNot enough memory.\n");
	ret = (-1);
	goto out;

not_enough_memory_pal:
	png_error("\nNot enough memory.\n");
	ret = (-1);
	goto out;
*/
error_inflate:
	png_error("\nError during inflate.\n");
	ret = (-1);
	goto out;

out:
	return (ret);
}



/*
 * lcdfb_run_timer_splash - OLD FUNCTION!!!, used only with animation
 * @timer:
 *
 * */
#if 0
static void lcdfb_run_timer_splash(struct timer_list *timer)
{
	if (!timer_pending(timer)) {
//		mod_timer(timer, jiffies + ((HZ * SPLASH_SPEED_UPDATE_IN_MSEC) / 1000));
	}
}
#endif


/*
 * lcdfb_timer_splash -
 * TODO function to be clean (no more animation)
 * @drvdata
 *
 * */
static void lcdfb_timer_splash(unsigned long data)
{
	struct lcdfb_drvdata *drvdata = (struct lcdfb_drvdata *)data;
	unsigned int ofs, background_ofs;
	unsigned int img_count = 0;
	signed int ret;
	unsigned int y_ofs;

	ofs = 0;
   	background_ofs = 0;

	if (nobody_asks_for_fb) {
		if (img_count == 0) {
			/* background img 1st (same size of the screen) */
			ret = decode_png(&drvdata->splash_info.data[0], background,
					drvdata->fb_info->var.xres,
					drvdata->fb_info->var.yres,
					drvdata->fb_info->var.bits_per_pixel);
			if (ret < 0) {
				printk("ERROR in the png splash data (image number %d)\n",
						img_count);
				ret = 0; // FIXME not a good patch!
			}
			ofs = ret;
			background_ofs = ret;
			memcpy(drvdata->fb_mem, background, drvdata->fb_info->fix.line_length *
					drvdata->fb_info->var.yres);
			img_count++;
		} else {
			memcpy(img, background, drvdata->fb_info->fix.line_length *
					drvdata->fb_info->var.yres);
			ret = decode_png(&drvdata->splash_info.data[ofs], img,
					drvdata->fb_info->var.xres,
					drvdata->fb_info->var.yres,
					drvdata->fb_info->var.bits_per_pixel);
			if (ret < 0) {
				printk("ERROR in the png splash data (image number %d)\n",
						img_count);
				ret = 0; // FIXME not a good patch!
			}
			ofs += ret;

			/* copy only udpated data lines in the fb */
			if (IHDR_data.height < drvdata->fb_info->var.yres) {
				/* image is smaller than screen */
				y_ofs = ((drvdata->fb_info->var.yres - IHDR_data.height) / 2) *
					drvdata->fb_info->var.xres *
					(drvdata->fb_info->var.bits_per_pixel >> 3);
				memcpy(&drvdata->fb_mem[y_ofs], &img[y_ofs],
						IHDR_data.height * drvdata->fb_info->fix.line_length);
			} else {
				/* same size */
				memcpy(drvdata->fb_mem, img, drvdata->fb_info->fix.line_length *
						drvdata->fb_info->var.yres);
			}

			img_count++;
			if (img_count == drvdata->splash_info.images) {
				img_count = 1;
				ofs = background_ofs;
#if 0
#ifndef SPLASH_LOOP_ANIM
				/* Stop the animation */
				nobody_asks_for_fb = 0;
#endif // SPLASH_LOOP_ANIM
#endif
			}
		}

		/* wake up the fb refresh thread */
		drvdata->need_refresh = 1;
		wake_up(&drvdata->wq);

#if 0
#if (SPLASH_IMAGES > 1)
		lcdfb_run_timer_splash(&timer_splash);
#else
		/* Free mem ressources asap */
		animated_splash_screen_stop();
#endif
#endif
	} /* nobody_asks_for_fb */

}



/*
 * splash_start -
 * @drvdata:
 * @return: 0 if no problem.
 * */
int splash_start(struct lcdfb_drvdata *drvdata)
{
	if (is_png(drvdata->splash_info.data)) {
		/* mem alloc only if png, nothing to do if RAW LCD FORMAT */
		background = (unsigned char*)vmalloc(drvdata->fb_info->fix.line_length *
				drvdata->fb_info->var.yres);
		if (background == NULL)
			return -ENOMEM;

		img = (unsigned char*)vmalloc(drvdata->fb_info->fix.line_length *
				drvdata->fb_info->var.yres);
		if (img == NULL)
			return -ENOMEM;

		workspace = kmalloc(zlib_inflate_workspacesize(), GFP_KERNEL);
		if (workspace == NULL)
			return -ENOMEM;

		input_buf = vmalloc(SPLASH_MAXIMGMEM);
		if (input_buf == NULL)
			return -ENOMEM;

		compressed_buf = kmalloc(SPLASH_MAXPNGFILESIZE, GFP_KERNEL);
		/* FIXME SPLASH_MAXPNGFILESIZE */
		if (compressed_buf == NULL)
			return -ENOMEM;

		/* FIXME pal with more than 256 entries */
		pal = kmalloc(256*3, GFP_KERNEL);
		if (pal == NULL)
			return -ENOMEM;

		nobody_asks_for_fb = 1;

		lcdfb_timer_splash((unsigned long)drvdata);

		nobody_asks_for_fb = 0;

		vfree(background);
		vfree(img);
		kfree(workspace);
		vfree(input_buf);
		kfree(compressed_buf);
		kfree(pal);

	} else {
		/* RAW IMAGE, not a PNG */
		nobody_asks_for_fb = 0;
		memcpy(drvdata->fb_mem, &drvdata->splash_info.data[0],
				drvdata->fb_info->fix.line_length * drvdata->fb_info->var.yres);

		drvdata->need_refresh = 1;
		wake_up(&drvdata->wq);
	}

	return 0;
}


/*
 * splash_stop - OLD FUNCTION!!!, used only with animation
 * @drvdata:
 *
 * */
void splash_stop(struct lcdfb_drvdata *drvdata)
{
	static int first_time = 1;

	if (first_time) {
		nobody_asks_for_fb = 0;
//#if (SPLASH_IMAGES > 1)
		del_timer(&timer_splash);
//#endif
		vfree(background);
		vfree(img);
		kfree(workspace);
		vfree(input_buf);
		kfree(compressed_buf);
		kfree(pal);
		first_time = 0;
	}
}
