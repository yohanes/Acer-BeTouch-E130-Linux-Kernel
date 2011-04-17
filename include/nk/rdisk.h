/*
 ****************************************************************
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)rdisk.h 1.2     05/09/21 Jaluna"
 *
 * Contributor(s):
 *   Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
 *
 ****************************************************************
 */

#ifndef _D_RDISK_H
#define _D_RDISK_H

#define	RDISK_RING_TYPE	0x44495348	/* DISK */

typedef nku16_f RDiskDevId;
typedef nku32_f RDiskSector;
typedef nku32_f RDiskCookie;
typedef nku8_f  RDiskOp;
typedef nku8_f  RDiskStatus;
typedef nku8_f  RDiskCount;

#define RDISK_OP_INVALID   0
#define RDISK_OP_PROBE     1
#define RDISK_OP_READ      2
#define RDISK_OP_WRITE     3

    //
    // Remote Disk request header.
    // Buffers follow just behind this header.
    //
typedef struct RDiskReqHeader {
    RDiskSector sector[2];	/* sector [0] - low; [1] - high */
    RDiskCookie cookie;		/* cookie to put in the response descriptor */
    RDiskDevId  devid;		/* device ID */
    RDiskOp     op;		/* operation */
    RDiskCount  count;		/* number of buffers which follows */
} RDiskReqHeader;

//
// The buffer address is in fact a page physical address with the first and
// last sector numbers incorporated as less significant bits ([5-9] and [0-4]).
// Note that the buffer address type is the physcial address type and it
// is architecture specific.
//
typedef NkPhAddr RDiskBuffer;
#define	RDISK_FIRST_BUF(req) \
	((RDiskBuffer*)(((nku8_f*)(req)) + sizeof(RDiskReqHeader)))

#define	RDISK_SECT_SIZE_BITS	9	/* sector is 512 bytes   */
#define	RDISK_SECT_SIZE		(1 << RDISK_SECT_SIZE_BITS)

#define	RDISK_SECT_NUM_BITS	5	/* sector number is 0..31 */
#define	RDISK_SECT_NUM_MASK	((1 << RDISK_SECT_NUM_BITS) - 1)

#define	RDISK_BUFFER(paddr, start, end) \
	((paddr) | ((start) << RDISK_SECT_NUM_BITS) | (end))

#define	RDISK_BUF_SSECT(buff) \
	(((buff) >> RDISK_SECT_NUM_BITS) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_ESECT(buff) \
	((buff) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_SECTS(buff) \
	(RDISK_BUF_ESECT(buff) - RDISK_BUF_SSECT(buff) + 1)

#define	RDISK_BUF_SOFF(buff) (RDISK_BUF_SSECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_EOFF(buff) (RDISK_BUF_ESECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_SIZE(buff) (RDISK_BUF_SECTS(buff) << RDISK_SECT_SIZE_BITS)

#define	RDISK_BUF_PAGE(buff) ((buff) & ~((1 << (RDISK_SECT_NUM_BITS*2)) - 1))
#define	RDISK_BUF_BASE(buff) (RDISK_BUF_PAGE(buff) + RDISK_BUF_SOFF(buff))

    //
    // Response descriptor
    //
typedef struct RDiskResp {
    RDiskCookie cookie;		// cookie copied from request
    RDiskOp     op;		// operation code copied from request
    RDiskStatus status;		// operation status
} RDiskResp;

#define RDISK_STATUS_OK	     0
#define RDISK_STATUS_ERROR   0xff

    //
    // Probing record
    //
typedef nku16_f RDiskInfo;

typedef struct RDiskProbe {
    RDiskDevId  devid;    // device ID
    RDiskInfo   info;     // device type & flags
    RDiskSector size[2];  // size in sectors ([0] - low; [1] - high)
} RDiskProbe;

    //
    // Types below match ide_xxx in Linux ide.h
    //
#define RDISK_TYPE_FLOPPY  0x00
#define RDISK_TYPE_TAPE    0x01
#define RDISK_TYPE_CDROM   0x05
#define RDISK_TYPE_OPTICAL 0x07
#define RDISK_TYPE_DISK    0x20

#define RDISK_TYPE_MASK    0x3f
#define RDISK_TYPE(x)      ((x) & RDISK_TYPE_MASK)

#define RDISK_FLAG_RO      0x40
#define RDISK_FLAG_VIRT    0x80

#endif
