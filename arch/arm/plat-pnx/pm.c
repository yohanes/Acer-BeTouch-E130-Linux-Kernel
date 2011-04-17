/*
 * linux/arch/arm/plat-pnx/pm.c
 *
 * OMAP Power Management Routines
 *
 * Original code for the SA11x0:
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * Modified for the PXA250 by Nicolas Pitre:
 * Copyright (c) 2002 Monta Vista Software, Inc.
 *
 * Modified for the OMAP1510 by David Singleton:
 * Copyright (c) 2002 Monta Vista Software, Inc.
 *
 * Modified for the PNX67XX by Viincent Guittot
 * Copyright (C) 2010 ST-Ericsson
 *
 * Cleanup 2004 for OMAP1510/1610 by Dirk Behme <dirk.behme@de.bosch.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/module.h>

#include <linux/cpuidle.h>

extern int pnx_idle_suspend(void);

void pnx_pm_suspend(void)
{
	printk("PM: pnx67xx is trying to enter deep sleep...\n");

	pnx_idle_suspend();

	printk("PM: pnx67xx is re-starting from deep sleep...\n");
}


//static void (*saved_idle)(void) = NULL;

/*
 *	pnx_pm_prepare - Do preliminary suspend work.
 *
 */
extern int set_deepest_idle_state(unsigned long state);

static int pnx_pm_prepare(void)
{
	/* We cannot sleep in idle until we have resumed */
	set_deepest_idle_state(1);
//	saved_idle = pm_idle;
//	pm_idle = NULL;

	return 0;
}


/*
 *	pnx_pm_enter - Actually enter a sleep state.
 *	@state:		State we're entering.
 *
 */

static int pnx_pm_enter(suspend_state_t state)
{
	switch (state)
	{
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		pnx_pm_suspend();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


/**
 *	pnx_pm_finish - Finish up suspend sequence.
 *
 *	This is called after we wake back up (or if entering the sleep state
 *	failed).
 */

static void pnx_pm_finish(void)
{
	set_deepest_idle_state(3);
//	pm_idle = saved_idle;
}


static struct platform_suspend_ops pnx_pm_ops ={
	.prepare	= pnx_pm_prepare,
	.enter		= pnx_pm_enter,
	.finish		= pnx_pm_finish,
	.valid		= suspend_valid_only_mem,
};


static int __init pnx_pm_init(void)
{
	printk("Power Management for PNX67xx\n");

	suspend_set_ops(&pnx_pm_ops);

	return 0;
}
__initcall(pnx_pm_init);
