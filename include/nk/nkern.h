/*
 ****************************************************************
 *
 * Component = Generic Nano-Kernel Interface (for device drivers)
 *
 * Copyright (C) 2002-2005 Jaluna SA.
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * #ident  "@(#)nkern.h 1.8     05/09/21 Jaluna"
 *
 * Contributor(s):
 *   Vladimir Grouzdev (grouzdev@jaluna.com) Jaluna SA
 *
 ****************************************************************
 */

#ifndef	_D_NKERN_H
#define	_D_NKERN_H

#include <asm/nk/nk_f.h>
#include <nk/nk.h>
#include <nk/nkdev.h>

extern NkDevOps nkops;	/* Nano-Kernel DDI Operations */

extern void ioctl_syscall_install (void* new, void** old);

#endif
