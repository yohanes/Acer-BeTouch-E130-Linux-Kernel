
#ifndef  VMON_H
#define  VMON_H

#ifdef VMON_C

#define VMON_MAJOR          0
#define VMON_MINOR          0
#define VMON_DEVICE_NAME    "vmon"
#define VMON_NAME			"monitor"
#define VMON_FIFO_SIZE		( 48 * 1024 )
#define VMON_EVENT_COUNT	2
#define VMON_STATES_NB		150
#define VMON_STATES_MASK	0xFFFFFFFF /* 32bit state stamp */
#define VMON_NB_OF_INTS		65

#define VMON_NB_OF_RTK_STATES	255
#define VMON_NB_OF_LINUX_STATES	32000

#define VMON_RESULTS_PROCFS_NAME "vmon_results"
#define VMON_ON_OFF_PROCFS_NAME  "vmon_OnOff"
#define VMON_DETAILS_PROCFS_NAME "vmon_details"
#define VMON_PROCFS_MAX_SIZE	10240

#define XOSMON_POWERDOWN_STATE      0
#define XOSMON_RTKE_STATE           1
#define XOSMON_LINUX_STATE          2
#define XOSMON_TASK0_STATE          3
#define XOSMON_CONTEXT_SWI          0xFE
#define XOSMON_STATELESS_NKIDLE     0xFD
#define XOSMON_NKIDLE_RESTART       0xFC
#define XOSMON_INDIRECT_HDL         0xFB
#define XOSMON_XINTRHDL             0xFA

#define XOSMON_NESTED_SHIFT        64
#define XOSMON_INT_SHIFT          128
#define XOSMON_TASK_SHIFT           4
#define XOSMON_LINUX_SHIFT	  256

/* FIXME: to be move into kernel */
#define NK_DEV_ID_VMON_FIFO 0x766D6F6E

/* functions */
extern ssize_t vmon_write_to_buffer(unsigned long long sample);
extern int vmon_p_init(dev_t dev);
extern void vmon_p_cleanup(void);

extern int vmon_kernel_fifo_init(unsigned int *init_base_adr);
extern void vmon_fill_kernel_fifo(pid_t pid);
extern void vmon_kernel_fifo_exit(void);
extern unsigned int vmon_kernel_fifo_len(void);
extern void vmon_kernel_fifo_reset(void);
extern unsigned long long *vmon_kernel_fifo_get(unsigned * len);
extern void probe_sched_switch_vmon(struct rq *__rq, \
		struct task_struct *prev, struct task_struct *next);

#endif /* VMON_C */

#if defined VMON_C || defined VMON_PIPE_C

#define VMON_P_BUFFERSIZE        1024
#define VMON_P_BUFFER_WATERSHED   384 /* from oprofile_files.c */
#include <linux/mutex.h>			/* mutex */

extern unsigned int vmon_debug;
extern unsigned int vmon_major, vmon_minor;
extern struct mutex buffer_mutex;
extern unsigned int *base_adr;

#ifdef CONFIG_VMON_DEBUG
#define PTRACE(fmt, arg...) printk ( fmt, ## arg )
#else
#define PTRACE(fmt, args...) /* nothing, its a placeholder */
#endif /* CONFIG_VMON_DEBUG */

#define prolog(fmt, args...) \
    do \
        if ( vmon_debug & 1 )\
            PTRACE ( KERN_DEBUG "> %s " fmt "\n", __func__, ## args );\
    while ( 0 )

#define epilog(fmt, args...) \
    do \
        if ( vmon_debug & 2 )\
            PTRACE ( KERN_DEBUG "< %s " fmt "\n", __func__, ## args );\
    while ( 0 )

#define critical(fmt, args...) \
    printk ( KERN_ERR "! %s " fmt "\n", __func__, ## args )

#define warning(fmt, args...) \
    printk ( KERN_WARNING "? %s " fmt "\n", __func__, ## args )

#define report(fmt, args...) \
    printk ( KERN_INFO "- %s " fmt "\n", __func__, ## args )

#endif /* VMON_C || defined VMON_PIPE_C */

extern unsigned int vmon_l2m_idle_switch; /* used in <plat-pnx/process_pnx.c> */
extern u64 vmon_stamp(void);

#endif /* VMON_H */


