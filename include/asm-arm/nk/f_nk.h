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
 * #ident  "@(#)f_nk.h 1.33     06/03/23 Jaluna"
 *
 * Contributor(s):
 *	Vladimir Grouzdev <vladimir.grouzdev@jaluna.com>
 *	Guennadi Maslov <guennadi.maslov@jaluna.com>
 *	Gilles Maigne <gilles.maigne@jaluna.com>
 *
 ****************************************************************
 */

#ifndef _NK_F_NK_H
#define _NK_F_NK_H

#include "nk_f.h"

#define	NK_OSCTX_DATE		0x221105 /* file modification date: DDMMYY */

	/*
	 * Standard ARM vectors
	 */
#define NK_RESET_VECTOR		 0x00
#define NK_UNDEF_INSTR_VECTOR	 0x04
#define NK_SYSTEM_CALL_VECTOR	 0x08
#define NK_PREFETCH_ABORT_VECTOR 0x0c
#define NK_DATA_ABORT_VECTOR	 0x10
#define NK_RESERVED_VECTOR	 0x14
#define NK_IRQ_VECTOR		 0x18
#define NK_FIQ_VECTOR		 0x1c

	/*
	 * Exended NK vectors
	 */
#define NK_XIRQ_VECTOR		0x24	/* cross IRQ vector */
#define NK_IIRQ_VECTOR		0x28	/* indirect IRQ vector */

	/*
	 * Virtual exceptions
	 */
#define	NK_VEX_RUN_BIT		0	/* running/idle event */
#define	NK_VEX_IRQ_BIT		8	/* IRQ event */
#define	NK_VEX_FIQ_BIT		16	/* FIQ event */

#define	NK_VEX_RUN		(1 << NK_VEX_RUN_BIT)
#define	NK_VEX_IRQ		(1 << NK_VEX_IRQ_BIT)
#define	NK_VEX_FIQ		(1 << NK_VEX_FIQ_BIT)
#define	NK_VEX_MASK		(NK_VEX_RUN | NK_VEX_IRQ | NK_VEX_FIQ)

#define NK_XIRQ_IRQ		0xff	/* IRQ assigned to XIRQ */
#define	NK_XIRQ_LIMIT		32

#define NK_VIL_SIZE 		64	/* size of Virtual Interrupts List */
#define NK_COMMAND_LINE_SIZE	1024
#define NK_TAG_LIST_SIZE	(NK_COMMAND_LINE_SIZE + 512)

    /*
     * nk os cmd other than the one provided by standard API
     */
#define NK_OSCMD_RESUME		1
#define NK_OSCMD_STOP		2
#define NK_OSCMD_RESTART	3

    /*
     * performance monitoring command.
     */
#define    NK_PERFMON_STARTMON		0x0
#define    NK_PERFMON_STOPMON		0x1

     /*
      * Priorities for idle OS's
      */

#define NK_PRIO_PRIM_IDLE	1
#define NK_PRIO_SEC_IDLE	0

#if !defined(__ASM__) && !defined(__ASSEMBLY__)

    /*
     * Nano-kernel Types...
     */
typedef nku32_f NkBool;		/* boolean value: true/false (1/0) */
typedef nku32_f NkMagic;	/* OS context magic */
typedef nku32_f NkVexMask;	/* virtual exceptions bit-mask */
typedef nku32_f NkXIrqMask;	/* cross IRQs bit-mask */
typedef nku32_f NkVmAddr;	/* GMv FIXME */
typedef nku32_f	NkOsCmd;	/* Os control cmd restart */
typedef nku32_f	NkPerfCtrlCmd;	/* other performance ctrl */

typedef void (*NkVector) (void);

typedef nku8_f NkTagList[NK_TAG_LIST_SIZE];

typedef struct NkOsCtx NkOsCtx;

    /*
     * Nano-Kernel Debug interface.
     */
typedef int  (*NkRead)	    (NkOsCtx* ctx, char* buf, int size);
typedef int  (*NkWrite)	    (NkOsCtx* ctx, const char* buf, int size);
typedef int  (*NkPoll)	    (NkOsCtx* ctx, char* ch);
typedef int  (*NkTest)	    (NkOsCtx* ctx);
typedef void (*NkInitLevel) (NkOsCtx* ctx, const int level);

typedef struct NkDbgOps {
    NkRead	read;
    NkWrite	write;
    NkPoll	poll;
    NkTest	test;
    NkInitLevel	init_level;
} NkDbgOps;

typedef nku32_f NkPrio;

typedef NkPhAddr (*NkDevAlloc) (NkOsCtx* ctx, NkPhSize size);
typedef void     (*NkXIrqPost) (NkOsCtx* ctx, NkOsId id);
typedef int	 (*NkIdle)     (NkOsCtx* ctx);
typedef int	 (*NkPrioSet)  (NkOsCtx* ctx, NkPrio prio);
typedef NkPrio	 (*NkPrioGet)  (NkOsCtx* ctx);
typedef int      (*NkHistGetc) (NkOsCtx* ctx, nku32_f offset);
typedef void     (*NkReady)    (NkOsCtx* ctx);
typedef void     (*NkOsCtrl)   (NkOsCtx* ctx, NkOsCmd cmd, NkOsId id);
typedef void     (*NkPerfCtrl) (NkOsCtx* ctx, int, ... );

typedef struct {
    nku8_f fifo[NK_VIL_SIZE];
    nku8_f limit;
    nku8_f start;
    nku8_f filler[2];
} NkVIL;

#define  VIL_enqueue(VIL, IRQ) 				        \
	do { 						        \
	    (VIL)->fifo[(VIL)->limit] = (IRQ); 		        \
	    (VIL)->limit = ((VIL)->limit+1) & (NK_VIL_SIZE-1);  \
	} while(0)

#define  VIL_dequeue(VIL, IRQ) 				        \
	do {						        \
	    IRQ = (VIL)->fifo[(VIL)->start];		        \
	    (VIL)->start = ((VIL)->start+1) & (NK_VIL_SIZE-1);  \
	} while (0)

#define  VIL_empty(VIL) ((VIL)->start == (VIL)->limit)
#define  VIL_init(VIL)  ((VIL)->start = (VIL)->limit = 0)



#define LOG_BUFFER_SIZE		0x10000

	/*
	 * log_record can be used in assembly code to log data
	 * rx is supposed contains a pointer to a NkLogBuffer structure
	 * ry and rz must be scratch registers.
	 * state is the new current state.
	 * except the reading of timer register which is slow by nature
	 * this sequence of code should run quickly.
	 */
#define log_record(rx, ry, rz, state, size)				      \
	ldr      ry,[rx, dash 4]					      \
        mov      rz, dash state						      \
        ldr      ry,[ry, dash 0]					      \
        add      ry,rz,ry,lsl dash 4					      \
        ldrh     rz,[rx, dash 2]					      \
        add      rz, rz, dash 1						      \
        strh     rz,[rx, dash 2]					      \
        sub      rz, rz, dash 1						      \
        bic      rz, rz, dash 0xffff - (size - 1)			      \
        add      rz,rx,rz,LSL dash 2					      \
        str      ry,[rz, dash 0x10]					      \

typedef struct {
    nku16_f	reader;
    nku16_f	writer;
    long*	timer;
    nku32_f	pc;
    nku32_f	wraparound;
    nku32_f	logs[LOG_BUFFER_SIZE];
} NkLogBuffer;


struct NkOsCtx {
    NkVector	os_vectors[16];	/* OS vectors */

	/*
	 * Basic OS execution context
	 */
    nku32_f	regs[16];
    nku32_f	cpsr;

	/*
	 * Virtual Exceptions
	 * (WARNING: "pending" MUST follow "cpsr", see vectors.s)
	 * FIXME_LATER_GMv - comment
	 */
    NkVexMask	pending;	/* pending virtual exceptions */
    NkVexMask	enabled;	/* enabled virtual exceptions */

    NkMagic	magic;		/* OS context magic */

	/*
	 * Extended OS execution context
	 */
    nku32_f	sp_svc;		/* supervisor SP */
    nku32_f	pc_svc;		/* supervisor SP */
    void*       nkvector;	/* virt pointer to NK vectors */
    NkPhAddr	root_dir;	/* MMU root dir */
    nku32_f	domain;		/* MMU domain */

    NkVIL	vil;		/* virtual interrupt list */

    nku32_f	arch_id;	/* ARM architecture id */
    NkOsId	id;		/* OS id */
    NkPhAddr	vector;		/* vector physical address */
    NkPhAddr	ram_info;	/* RAM info */
    NkPhAddr	dev_info;	/* device info */
    NkPhAddr	xirqs;		/* table of pending XIRQs */
    NkPhAddr	running;	/* pointer to the mask of running OSes */
    NkOsId	lastid;		/* last OS ID */
    NkPrio	cur_prio;	/* current priority */
    NkPrio	fg_prio;	/* foreground priority */
    NkPrio	bg_prio;	/* foreground priority */

    NkReady	ready;		/* OS is ready */
    NkDbgOps	dops;		/* debug (console) ops	*/
    NkIdle	idle;		/* idle */
    NkPrioSet   prio_set;	/* set current os prio */
    NkPrioGet   prio_get;	/* get current os prio */
    NkXIrqPost	xpost;		/* post XIRQ */
    NkDevAlloc	dev_alloc;	/* allocate memory for device descriptors */
    NkHistGetc	hgetc;		/* get console history character */
    NkOsCtrl	ctrl;		/* Ctrl OS */
    NkPerfCtrl	perfctrl;	/* Performance OS */
    nku32_f	filler[1];

    NkTagList	tags;		/* tag list (boot parameters) */
    NkPhAddr	cmdline;	/* command line pointer       */
};

#endif

#define	NK_OSCTX_MAGIC()	((sizeof(NkOsCtx) << 24) | NK_OSCTX_DATE)

#endif
