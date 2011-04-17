/*
 ****************************************************************
 *
 * Component = Nano Kernel - Generic DDI, shared device data.
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)nkuart.h 1.2     05/09/21 Jaluna"
 *
 * Contributor(s):
 *   Guennadi Maslov <guennadi.maslov@jaluna.com>
 *
 ****************************************************************
 */

#ifndef _NK_NKUART_H
#define _NK_NKUART_H

#define NK_UART_CONF_CS5      0x0001   /* 5-bits */
#define NK_UART_CONF_CS6      0x0002   /* 6-bits */
#define NK_UART_CONF_CS7      0x0003   /* 7-bits */
#define NK_UART_CONF_CS8      0x0004   /* 8-bits */
#define NK_UART_CONF_CSIZE    0x0007   /* Mask */
#define NK_UART_CONF_CSTOPB   0x0010   /* Send 2 stop bits else none. */
#define NK_UART_CONF_PARENB   0x0020   /* Parity enable */
#define NK_UART_CONF_PARODD   0x0040   /* Odd parity, else even */
#define NK_UART_CONF_RXHDF    0x0100   /* Enable Rx hardware flow control */
#define NK_UART_CONF_TXHDF    0x0200   /* Enable Tx hardware flow control */

typedef struct NkUartConf {
    unsigned int speed;          /* baud rate */
    unsigned int mode;           /* transfer mode */
} NkUartConf;

#endif
