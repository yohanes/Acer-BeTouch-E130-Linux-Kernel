#ifndef _DRVCHG_H
#define _DRVCHG_H

typedef void (* iconmgt)(u_int8_t, int8_t, ulong, int8_t, u_int16_t, u_int16_t);
void drvchg_registerChgFct(iconmgt);
void drvchg_unregisterChgFct(void);
void drvchg_addCompensation(u_int8_t type, int16_t mvolts);
unsigned long drvchg_get_voltage(void);
unsigned long drvchg_get_temperature(void);
unsigned long drvchg_get_current(void);
u_int8_t drvchg_get_charge_state(void);
void drvchg_registerOnlineFct(void (*callback)(unsigned int) );
void drvchg_unregisterOnlineFct(void);

/// +ACER_AUx
#define ACER_CHG_BAT_FUN
#define ACER_CUST_0011

#ifdef ACER_CHG_BAT_FUN
#define POWER_SUPPLY_BATTERY_UPDATA	(POWER_SUPPLY_TYPE_USB + 4)
#define POWER_SUPPLY_START_CHARGE	(POWER_SUPPLY_TYPE_USB + 5)
#define POWER_SUPPLY_CK_BATID		(POWER_SUPPLY_TYPE_USB + 6)

// for drvchg call back function
enum {
	DRVCHG_CHGINS,
	DRVCHG_USBEVENT,
};

// for g_chg_flag bitmap define
enum {
	CHGF_750MA =	0x0001,
	CHGF_P_OCP =	0x0002,		// HW OCP/OVP
	CHGF_P_OVP =	0x0004,		// SW OVP
	CHGF_P_OTP =	0x0008,		// SW OTP
	CHGF_P_THL =	0x0010,		// PCF50623_THLIMON
	CHGF_P_WDT =	0x0020,		// CC Watchdog
	CHGF_P_BID =	0x0040,		// Battery ID
	CHGF_P_APC =	0x0080,		// Application command
	CHGF_CVCHG =	0x0100,
	CHGF_P_USB =	0x0200,		// USB suspend mode
	CHGF_P_INR =	0x0400,		// Inrush stage
};

enum {
	DBGMSG_TEMP		= 0x10,
	DBGMSG_NOTTIFY	= 0x20,
	DBGFLAG_MONKEY	= 0x10000,
};
#define DBGMEG_INIT_VALUE (0)

extern void setdbglv(ulong lv);
extern ulong showdbglv(ulong mask);
#define showdbg() (showdbglv(0x0000ffff))

#ifdef ACER_CUST_0011
#define ACER_ACER_REQ
#define ACER_VBAT_OPCOMP
#endif	// ACER_CUST_0011
#endif	// ACER_CHG_BAT_FUN
/// -ACER_AUx

#endif //_DRVCHG_H
