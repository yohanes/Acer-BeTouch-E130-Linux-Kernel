#include <linux/module.h>

#include <linux/drvchg.h>


#include "drvchg.h"
#include <linux/pcf506XX.h>

/******************************************************
** devCompVolt for drvchg
******************************************************/
static ulong g_dbg_batt = DBGMEG_INIT_VALUE;
static int16_t devCompVolt[DRVCHG_MAX] = {0};

#ifdef ACER_VBAT_OPCOMP_TRY
#include <nk/adcclient.h>
static ulong g_try_vbat = 0;
static void adc_get_vbat(ulong volt)
{
	g_try_vbat = volt;
}
#endif	// ACER_VBAT_OPCOMP_TRY

void setLoadingCompVolt(u_int8_t type, int16_t mvolts)
{
#ifdef ACER_VBAT_OPCOMP_TRY
	adc_aread(FID_VBAT, adc_get_vbat);
	printk(KERN_INFO "<BAT> setLoadingCompVolt : (%d, %d) - %04ld\n", type, mvolts, g_try_vbat);
#endif	// ACER_VBAT_OPCOMP_TRY
	devCompVolt[type] = mvolts;
}
EXPORT_SYMBOL(setLoadingCompVolt);

static ssize_t opvolt_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *s = buf;
	int i, sum = devCompVolt[0];

	s += sprintf(s, "opvlot : %d", devCompVolt[0]);
	for (i = 1; i < DRVCHG_MAX; i++) {
		s += sprintf(s, " + %d", devCompVolt[i]);
		sum += devCompVolt[i];
	}
	s += sprintf(s, " = %d\n", sum);

	return (s - buf);
}

static ssize_t opvolt_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	int val;
	if (('0' <= *buf) && (*buf <= '9')) {
		if (sscanf((buf+2), "%d", &val) == 1) {
			setLoadingCompVolt((DRVCHG_KERNEL_MAX + (*buf - '0')), val);
			return n;
		}
	}
	return -EINVAL;
}

void setdbglv(ulong lv)
{
	g_dbg_batt = lv;
}
EXPORT_SYMBOL(setdbglv);

ulong showdbglv(ulong mask)
{
	return (g_dbg_batt & mask);
}
EXPORT_SYMBOL(showdbglv);

/******************************************************
** usbchg_event for drvchg
******************************************************/
static int g_usbevent = USBCHG_NONE;
static void (*g_pfdrvchg_fun)(int, int) = NULL;

void acerchg_chgins(void)
{
	if (g_pfdrvchg_fun) (*g_pfdrvchg_fun)(DRVCHG_CHGINS, 0);
}
EXPORT_SYMBOL(acerchg_chgins);

static void void_usbchg_event(int event)
{
	if (g_usbevent != USBCHG_DLSHORT) {
		if (event == USBCHG_DLSHORT) g_usbevent = event;				/* USBCHG_DLSHORT */
		else if (event != USBCHG_SUSPEND) g_usbevent = event;			/* USBCHG_DATALINK, USBCHG_RESUME */
		else if (g_usbevent != USBCHG_NONE) g_usbevent = USBCHG_USBSUP;	/* USBCHG_USBSUP */
	}
}

void usbchg_event(int event)
{
	if (g_pfdrvchg_fun) (*g_pfdrvchg_fun)(DRVCHG_USBEVENT, event);
	else void_usbchg_event(event);
}
EXPORT_SYMBOL(usbchg_event);

int bibatchg_registerdrvchg(int16_t ** pdevCompVolt, void (*pfdrvchg_fun)(int, int))
{
#ifdef ACER_VBAT_OPCOMP
	*pdevCompVolt = devCompVolt;
#endif	// ACER_VBAT_OPCOMP
	g_pfdrvchg_fun = pfdrvchg_fun;
	return g_usbevent;	/* ac(USBCHG_NONE, USBCHG_DLSHORT), usb(USBCHG_DATALINK, USBCHG_RESUME, USBCHG_USBSUP) */
}
EXPORT_SYMBOL(bibatchg_registerdrvchg);

void bibatchg_unregisterdrvchg(void)
{
	g_usbevent = USBCHG_NONE;
	g_pfdrvchg_fun = NULL;
}
EXPORT_SYMBOL(bibatchg_unregisterdrvchg);

/******************************************************
** pnx_batt_level for pnx_battery
******************************************************/
static int * g_pnx_batt_level = NULL;

unsigned long get_bat_level(void)
{
	return (g_pnx_batt_level) ? (*g_pnx_batt_level) : 50;
}
EXPORT_SYMBOL(get_bat_level);

void bibatchg_registerbattery(int * ppnx_batt_level)
{
	g_pnx_batt_level = ppnx_batt_level;
}
EXPORT_SYMBOL(bibatchg_registerbattery);

void bibatchg_unregisterbattery(void)
{
	g_pnx_batt_level = NULL;
}
EXPORT_SYMBOL(bibatchg_unregisterbattery);

/******************************************************
** Vbat loading compensate
******************************************************/
static void bibatchg_itcallback(u_int16_t battInt)
{
	if ( battInt & PCF506XX_MCHINS ) {
		pcf506XX_setMainChgCur(0);
		pcf506XX_setTrickleChgCur(0);
	}
}

/******************************************************
** Vbat loading compensate
******************************************************/
extern struct kobject *power_kobj;

static struct kobj_attribute opvolt_attr = {	\
	.attr	= {				\
		.name = __stringify(opvolt),	\
		.mode = 0644,			\
	},					\
	.show	= opvolt_show,			\
	.store	= opvolt_store,		\
};

static struct attribute * g[] = {
	&opvolt_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init acer_bibatchg_init(void)
{
	pcf506XX_setMainChgCur(0);
	pcf506XX_setTrickleChgCur(0);
	pcf506XX_registerChgFct(bibatchg_itcallback);
	if (!power_kobj)
		return -ENOMEM;
	return sysfs_create_group(power_kobj, &attr_group);
}

static void __exit acer_bibatchg_exit(void)
{
	sysfs_remove_group(power_kobj, &attr_group);
}

module_init(acer_bibatchg_init);
module_exit(acer_bibatchg_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ACERTDC");
MODULE_DESCRIPTION("battery charge build in driver");

