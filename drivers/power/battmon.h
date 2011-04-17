/*
 * ============================================================================
 *
 *       Filename:  battmon.h
 *
 *    Description:  battery monitoring interface 
 *
 *        Version:  1.0
 *        Created:  07/01/2009 12:12:49 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  O. Clergeaud (OCL), olivier.clergeaud@stericsson.com
 *        Company:  ST-Ericsson Le Mans
 *
 * ============================================================================
 */

#ifndef _BATTMON_H
#define _BATTMON_H

typedef void (* levelmgt)(int);
extern void battmon_registerLevelFct(levelmgt);
extern void battmon_unregisterLevelFct(void);


#endif //_BATTMON_H
