/*
 *       Filename:  power_debug.h
 *
 *    Description:
 *        Created:  08/22/2008 09:23:37 AM CEST
 *         Author:   Vincent Guittot
 *      Copyright: (C) 2010 ST-Ericsson
 */

#ifndef  POWER_DEBUG_INC
#define  POWER_DEBUG_INC

#ifdef CONFIG_PNX_POWER_TRACE
/*** Power debug function ***/
extern void pnx_dbg_put_element(char *id, unsigned long element,
		void * function);
#else
#define pnx_dbg_put_element(A,B,C) while(0) {};
#endif
/*** Register function for activate/deactivate feature */
typedef int ( * pnx_dbg_register_t ) ( void );

#define pnx_dbg_register(funk)\
	pnx_dbg_register_t __pnx_dbg_reg_##funk## __attribute_used__ \
	__attribute__((__section__(".pnx_dbg_reg"))) = (pnx_dbg_register_t)funk


#endif   /* ----- #ifndef POWER_DEBUG_INC  ----- */



