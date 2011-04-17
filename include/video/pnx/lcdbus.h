/*
 * linux/include/video/pnx/lcdbus.h
 *
 * LCD bus driver
 * Copyright (c) ST-Ericsson 2009
 *
 */

#ifndef __LCDBUS_H
#define __LCDBUS_H

#include <linux/device.h>
#include <linux/list.h>

extern struct bus_type lcdctrl_bustype;


/*
 * To avoid many malloc operations, this value defines the size of
 * the memory pool to be allocated at the driver init. It corresponds to 
 * the maximum number of consecutive LCD/VDE commands.
 * 
 * TODO (Plaform dependent) : Can be customized according to the 
 *  LCD HW specification.
 */
#define LCDBUS_CMDS_LIST_LENGTH  42

/**
 * enum lcdbus_datafmt - general lcdbus data formats
 *
 * These are general data formats. Each lcdbus driver maps them to internal
 * values and decides what modes are supported.
 */
enum lcdbus_datafmt {
    	/* Input format */
	LCDBUS_INPUT_DATAFMT_RGB565,
	LCDBUS_INPUT_DATAFMT_RGB888,
	LCDBUS_INPUT_DATAFMT_TRANSP,

	/* Output format */
	LCDBUS_OUTPUT_DATAFMT_RGB332,
	LCDBUS_OUTPUT_DATAFMT_RGB444,
	LCDBUS_OUTPUT_DATAFMT_RGB565,
	LCDBUS_OUTPUT_DATAFMT_RGB666,
	LCDBUS_OUTPUT_DATAFMT_TRANSP_8_BITS,
	LCDBUS_OUTPUT_DATAFMT_TRANSP_16_BITS,
	
	LCDBUS_NUM_DATAFMTS
};

/**
 * enum lcdbus_cmdtype - type of command to send to the display controller
 * @LCDBUS_CMDTYPE_CMD:  use for a normal command
 * @LCDBUS_CMDTYPE_DATA: use for a command writing to the display ram
 *
 * The lcdbus driver distinguishes between normal commands and commands writing
 * to the display ram. If it's a ram read/write command, the lcdbus driver will
 * set the preconfigured input and output formats, else the transparent data
 * format gets set.
 */
enum lcdbus_cmdtype {
	LCDBUS_CMDTYPE_CMD = 0,
	LCDBUS_CMDTYPE_DATA = 1,
	LCDBUS_CMDTYPE_ENUM_ON_U32 = 0x7FFFFFFF
};

/**
 * struct lcdbus_cmd - a command to be sent to the display using the lcdbus.
 * @data_int: internal data, can be used for commands with only a few bytes
 *   of data. If used, make data_phys point to it.
 * @data_int_phys: physical address of data_int.
 *
 * @data_phys: physical address of the data (could be data_int_phys for cmds).
 *
 * @type: the lcdbus driver distinguishes between normal commands and
 *   commands writing to the display ram. if it's a ram read/write
 *   command, the lcdbus driver will set the preconfigured input
 *   and output formats.
 * @cmd: display command code (register id)
 *
 * @bytes_per_pixel:
 * @w: width of the display area to update with data.
 * @h: height of the display area to update with data.
 * @stride: stride of the display area to update with data.
 *
 * @len: length of data in bytes (only for cmds, linked to data_int)
 *
 * @link: used to add this command to a list
 *
 * This structure is passed between the lcdctrl and lcdbus drivers. It uses the
 * Linux LLI to enable command chaining.
 *
 * NOTE: The structure size is calculated to satisfy these conditions
 *	- size is aligned on 32
 *	- total allocation (sizeof(struct)*LCDBUS_CMDS_LIST_LENGTH <= 4 KBytes 
 *		(because allocated with dma (one chunk))
 */
struct lcdbus_cmd {
	u8  data_int[60];	/* CAUTION This field MUST be the 1st in this structure */
	u32 data_int_phys;
	u32 data_phys;

	u32 len;			/* With LCDBUS_CMDTYPE_CMD only */

	u16 w;
	u16 h;
	u16 stride;
	u16 bytes_per_pixel;  	/* With LCDBUS_CMDTYPE_DATA only */
	
	enum lcdbus_cmdtype type; /* LCDBUS_CMDTYPE_CMD or LCDBUS_CMDTYPE_DATA */

	struct list_head link;

	u8  cmd;
	u8  _reserved[3];
};

/**
 * struct lcdbus_timing - timing for one display channel
 * @mode: bus interface, range 0..3
 * @ser : bus interface, range 0..3
 * @hold: bus interface, range 0..1
 * @rh:   read,          range 0..7
 * @rc:   read,          range 0..7
 * @rs:   read,          range 0..3
 * @wh:   write,         range 0..7
 * @wc:   write,         range 0..7
 * @ws:   write,         range 0..3
 * @ch:   misc,          range 0..1
 * @cs:   misc,          range 0..1
 *
 */
struct lcdbus_timing {
	u8 mode;
	u8 ser;
	u8 hold;
	u8 rh;
	u8 rc;
	u8 rs;
	u8 wh;
	u8 wc;
	u8 ws;
	u8 ch;
	u8 cs;
};


/**
 * struct lcdbus_conf - configuration for one display channel
 * @data_ofmt:  the data output format of the channel
 * @data_ifmt:  the data input format of the channel
 * @cmd_ofmt:   the cmd output format of the channel
 * @cmd_ifmt:   the cmd input format of the channel
 * @cskip:      the cmd command byte skip config
 * @swap:       the cmd byte swap config 
 * @align:      the alignment in 6-6-6 mode 
 * @eofi_pol:   
 * @eofi_skip:  
 * @eofi_del:    
 * @eofi_use_vsync: Indicates if the Tearing PIN have to be synchronized
 */
struct lcdbus_conf {
	enum lcdbus_datafmt data_ofmt;
	enum lcdbus_datafmt data_ifmt;

	enum lcdbus_datafmt cmd_ofmt;
	enum lcdbus_datafmt cmd_ifmt;

	/* 16 bits // mode */
	u16  cskip;
	u16  bswap;

	/* Alignment in 6-6-6 mode */
	u8 align;

	/* Tearing management */
	u8 eofi_pol;
	u8 eofi_skip;
	u8 eofi_del;
	u8 eofi_use_vsync;	
};

/**
 * struct lcdbus_ops - the function call interface to the lcdbus driver
 */
struct lcdbus_ops {
	int (*read)       (const struct device *dev,
		const struct list_head *commands);
	int (*write)      (const struct device *dev,
		const struct list_head *commands);
	int (*get_conf)   (const struct device *dev, struct lcdbus_conf *conf);
	int (*set_conf)   (const struct device *dev,
		const struct lcdbus_conf *conf);
	int (*set_timing) (const struct device *dev,
		const struct lcdbus_timing *timing);
};

#endif /* __LCDBUS_H */

