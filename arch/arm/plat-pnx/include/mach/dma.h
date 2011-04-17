/*
 *  linux/arch/arm/plat-pnx/include/mach/dma.h
 *
 *  pnx DMA header file
 *
 *  Author:	Vitaly Wool
 *  Copyright:	MontaVista Software Inc. (c) 2005
 *  Copyright:	ST-Ericsson (c) 2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#ifdef CONFIG_NKERNEL

/* #include "kid.h" */

/* Unique ID for DMAU shared device */
#define NK_DEV_ID_DMAU 0x5053444D    /* P(0x50)S(0x53)D(0x44)M(0x4D) */

/* core IDs for KID MCS support */
#define RTKE_CORE_ID    0
#define LINUX_CORE_ID   1

/* Common structure for shared DMA */
typedef struct {
    NkXIrq          linux_xirq;     /* cross interrupt for Linux */
    NkOsId          linux_id;       /* Linux OS identifier */
    uint32_t        ulTCStatus;
    uint32_t        ulErrorStatus;
} tNkDMAU;

#endif /* CONFIG_NKERNEL */

//#include "platform.h"

#define MAX_DMA_ADDRESS		0xffffffff

#define MAX_DMA_CHANNELS_NBR	9
#define MIN_DMA_CHANNELS_NBR	4

/**
 * @brief enum to be used in order to fill swidth,dwidth of struct
 * pnx_dma_ch_ctrl
 */
enum {
	WIDTH_BYTE = 0,
	WIDTH_HWORD,
	WIDTH_WORD
};

/**
 * @brief enum to be used in order to fill flow_cntrl of struct
 * pnx_dma_ch_config
 */
enum {
	FC_MEM2MEM_DMA,
	FC_MEM2PER_DMA,
	FC_PER2MEM_DMA,
	FC_PER2PER_DMA,
	FC_PER2PER_DPER,
	FC_MEM2PER_PER,
	FC_PER2MEM_PER,
	FC_PER2PER_SPER
};

/**
 * @brief
 */
enum {
	DMA_INT_UNKNOWN = 0,
	DMA_ERR_INT = 1,
	DMA_TC_INT = 2,
};

enum {
	DMA_BUFFER_ALLOCATED = 1,
	DMA_HAS_LL = 2,
};

/**
 * @brief enum to be used in order to fill dest_per or src_per in structure
 * channel_config
 */
enum {
	PER_FCI = 0,
	PER_UART1_TX = 1,
	PER_UART1_RX = 2,
	PER_UART2_TX = 3,
	PER_UART2_RX = 4,
	PER_SPI1 = 5,
	PER_SPI2 = 6,
	PER_GIU = 7,
	PER_SIIS = 8,
	PER_USIM_RX_TX = 9,
	PER_USIM_TX = 10,
	/* not usable */
	PER_RESERVED = 11,
	PER_NFI = 12,
	PER_NFI_ECC = 13,
	PER_MSI = 14,
	PER_EXTERNAL = 15,
	PER_GAU_OUT=16,
	PER_GAU_IN=17,
	PER_MAG_WRITE=18,
	PER_MAG_READ=19,
	PER_FIR=20
};
#define DMA_TR_SIZE_MAX 0xfff
struct pnx_dma_ch_ctrl {
	int tc_mask; /* 1 enable terminal interrupt count */
	int cacheable; /* 0 not useful in 5220*/
	int bufferable; /*0 not useful in 5220*/
	int priv_mode;/*0 not useful in 5220  */
	int di; /* 1 dest increment enable*/
	int si; /* 1 source increment enable */
	int dest_ahb1;/* 0 AHB1 / 1 AHB2 */
	int src_ahb1; /* 0 AHB1 / 1 AHB2 */
	int dwidth; /* use the corresponding enum*/
	int swidth;/* use the corresponding enum*/
	int dbsize; /* 1,4,8,16,32,64,128,256 */
	int sbsize; /* 1,4,8,16,32,64,128,256 */
	int tr_size; /*transfersize */
};

struct pnx_dma_ch_config {
	int halt; /* 0 allow dma requests , 1 halt dma request */
	int active; /* read only 0 no data in dma fifo , 1 data in dma fifo */
	int lock; /* O only */
	int itc; /* 0 mask tc interrupt / 1 no mask */
	int ie; /* 1 enable interrupt / 0 disable interrupt */
	int flow_cntrl;
	int dest_per;
	int src_per;
};

struct pnx_dma_ll {
	unsigned long src_addr;
	unsigned long dest_addr;
	u32 next_dma;
	unsigned long ch_ctrl;
	struct pnx_dma_ll *next;
	int flags;
	void *alloc_data;
	int (*free) (void *);
};

struct pnx_dma_config {
	int is_ll;
	unsigned long src_addr;
	unsigned long dest_addr;
	unsigned long ch_ctrl;
	unsigned long ch_cfg;
	struct pnx_dma_ll *ll;
	u32 ll_dma;
	int flags;
	void *alloc_data;
	int (*free) (void *);
};

extern struct pnx_dma_ll *pnx_alloc_ll_entry(dma_addr_t *);
extern void pnx_free_ll_entry(struct pnx_dma_ll *, dma_addr_t);
extern void pnx_free_ll(u32 ll_dma, struct pnx_dma_ll *);

extern int pnx_request_channel(char *, int,
				   void (*)(int, int, void *),
				   void *);
extern void pnx_free_channel(int);
extern int pnx_config_dma(int, int, int);
extern int pnx_dma_pack_control(const struct pnx_dma_ch_ctrl *,
				    unsigned long *);
extern int pnx_dma_parse_control(unsigned long,
				     struct pnx_dma_ch_ctrl *);
extern int pnx_dma_pack_config(const struct pnx_dma_ch_config *,
				   unsigned long *);
extern int pnx_dma_parse_config(unsigned long,
				    struct pnx_dma_ch_config *);
extern int pnx_config_channel(int, struct pnx_dma_config *);
extern int pnx_channel_get_config(int, struct pnx_dma_config *);
extern int pnx_dma_ch_enable(int);
extern int pnx_dma_ch_disable(int);
extern int pnx_dma_ch_enabled(int);
extern int pnx_dma_ch_fifo_empty(int);
extern void pnx_dma_split_head_entry(struct pnx_dma_config *,
					 struct pnx_dma_ch_ctrl *);
extern void pnx_dma_split_ll_entry(struct pnx_dma_ll *,
				       struct pnx_dma_ch_ctrl *);

#endif /* _ASM_ARCH_DMA_H */
