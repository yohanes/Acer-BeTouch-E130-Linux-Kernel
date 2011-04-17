/******************************************************
** the function for pnx_battery
******************************************************/
int8_t get_trickleCur(void)
{
	return trickleCurrent;
}
EXPORT_SYMBOL(get_trickleCur);

bool drvchg_get_charging_state(void)
{
	return (charge->flags & CHARGE_ACER_INITED) ? (charge->flags & CHARGE_CHARGING) : pcf506XX_mainChgPlugged();
}
EXPORT_SYMBOL(drvchg_get_charging_state);

ulong isChgFlag(ulong flag)
{
	return (g_chg_flag & flag);
}
EXPORT_SYMBOL(isChgFlag);

void ctrlCharge(ulong on, ulong flag)
{
	printk(KERN_INFO "<CHG> control charge : %ld - %04lx\n", on, flag);
	if (on) {
		g_chg_flag &= ~(flag);
		if ((g_chg_flag & CHGF_PAUSE) == 0)
			acerchg_insertCharger_work(CC_WQ_RESTART);
	}
	else {
		g_chg_flag |= flag;
		acerchg_insertCharger_work(CC_WQ_SUSPEND);
	}
	if (charge->Online != NULL)
		charge->Online(POWER_SUPPLY_BATTERY_UPDATA);
}
EXPORT_SYMBOL(ctrlCharge);

/******************************************************
** control the CC current
******************************************************/
static char * chg_current_text[] = {
	"0",
	"100",
	"750",
	"450",
	"x0"};

static void setCCreg(int mode)
{
	u_int8_t cur;

	if (mode < CC_NOSAVE_NOCHG)
		g_chg_cc = mode;

	//g_chg_flag &= ~CHGF_750MA;	// reset by acerchg_chgins, avoid clear CHGF_750MA by supsend charge.
	switch (mode) {
	case CC_NOSAVE_NOCHG:
	case CC_NOCHG:		/* no charge	0mA */
		cur = 0;
		trickleCurrent = 0;
		break;
	case CC_PRECHG:		/* pre-charge	100mA */
		cur = CHARGE_MAIN_CUR_LOW;
		trickleCurrent = (128 * CHARGE_MAIN_CUR_LOW / 255) - 1;
		break;
	case CC_ACCHG:		/* AC charge	750 */
		cur = CHARGE_MAIN_CUR_HIGH;
		trickleCurrent = (128 * CHARGE_MAIN_CUR_HIGH / 255) - 1;
		/* Start Max time charge timer */
		if ( !timer_pending(&charge->mainchgtimer) ) {
			init_timer(&charge->mainchgtimer);
			charge->mainchgtimer.function=charge_mainChgtimeout;
			charge->mainchgtimer.expires = round_jiffies(jiffies + CHARGE_MAIN_CHG_MAX_TIME);
			add_timer(&charge->mainchgtimer);
		}
		g_chg_flag |= CHGF_750MA;
		break;
	case CC_USBCHG:		/* USB charge	450mA */
		cur = CHARGE_USB_CUR_HIGH;
		trickleCurrent = (128 * CHARGE_USB_CUR_HIGH / 255) - 1;
		break;
	default:
		return;
	}

	if (g_chg_flag & CHGF_PAUSE) {
		charge->flags &= ~CHARGE_CHARGING;
		if (cur) {
			//if (showdbg())
				printk(KERN_INFO "<CHG> Skip set CC current mode (%d-%smA) at charge pause(%lx).\n", mode, chg_current_text[mode], g_chg_flag);
			return;
		}
	}

	//if (showdbg())
		printk(KERN_INFO "<CHG> set CC current mode : %d-%smA at %lx\n", mode, chg_current_text[mode], g_chg_flag);

	if (cur) charge->flags |= CHARGE_CHARGING;
	else charge->flags &= ~CHARGE_CHARGING;
	pcf506XX_setMainChgCur(cur);
	pcf506XX_setTrickleChgCur(trickleCurrent);
}

static void acerchg_proc_work(struct work_struct *work)
{
	setCCreg(g_chg_cmd);
}

static void setCCmodework(int mod)
{
	g_chg_cmd = mod;
	schedule_work(&g_acerchg_work);
}

/******************************************************
** [Kx] The AC/USB power supply recognition mechanism
** 0. for USB-IF Inruch current (110ms<, 0mA) - ACER_CHG_INRUCH
** 1. D+/D- short (750mA) - ACER_CHG_750MA
**    otherwise (450mA)
******************************************************/
static char * chg_event_text[] = {
	"USBCHG_DLSHORT",
	"USBCHG_DATALINK",
	"USBCHG_SUSPEND",
	"USBCHG_RESUME",
	"USBCHG_USBSUP",
	"USBCHG_NONE"};

static void acerchg_chgins(void)
{
	g_chg_flag = 0;		// CHGF_P_INR;
	g_chg_type = ACER_CHG_UNKNOW;
	g_chg_state = ACER_STA_NONE;
	charge->flags &= ~(CHARGE_MAIN_CHARGER | CHARGE_USB_CHARGER);
}

static void usbchg_event(int event)
{
	//if (showdbg())
		printk(KERN_INFO "<CHG> get usbchg_event : %d.%s at (%d-%ld-%x)\n",
			event, chg_event_text[event], g_chg_state, g_chg_type, charge->flags);

	CHG_LOCK(&g_chg_cc_lock);
	if (!(charge->flags & CHARGE_ACER_INITED)) {
		if (event == USBCHG_DLSHORT) g_chg_type = ACER_CHG_750MA;
		else if ((event != USBCHG_SUSPEND) && (g_chg_type != ACER_CHG_750MA)) g_chg_type = ACER_CHG_450MA;
	}
	else if (g_chg_state != ACER_STA_RM) {
		switch (event) {
		case USBCHG_DLSHORT:
			g_chg_type = ACER_CHG_750MA;
			if (g_chg_state >= ACER_STA_DETECT) {
				setCCmodework(CC_ACCHG);
				charge->flags &= ~CHARGE_USB_CHARGER;
				charge->flags |= CHARGE_MAIN_CHARGER;
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_MAINS);
				power_supply = DRVCHG_MAIN_CHARGER;
			}
			break;
		case USBCHG_DATALINK:
		case USBCHG_RESUME:
			if (g_chg_type == ACER_CHG_750MA) break;
			g_chg_type = ACER_CHG_450MA;		// for (g_chg_state == ACER_STA_NONE)
			if ((g_chg_state >= ACER_STA_DETECT) && (power_supply != DRVCHG_USB_CHARGER)) {
				charge->flags &= ~CHARGE_MAIN_CHARGER;
				charge->flags |= CHARGE_USB_CHARGER;
				if (charge->Online != NULL)
					charge->Online(POWER_SUPPLY_TYPE_USB);
				power_supply = DRVCHG_USB_CHARGER;
			}
			break;
		case USBCHG_SUSPEND:
			break;
		}
	}
	CHG_UNLOCK(&g_chg_cc_lock);
}

static void usbchg_proc_work(struct work_struct *work)
{
	usbchg_event(g_usbevent);
}

static void acer_chg_ctrl_func(unsigned long unused)
{
	switch (g_chg_state) {
	case ACER_STA_INRUCH:
		break;
	case ACER_STA_DETECT:
		if (power_supply == DRVCHG_NONE_CHARGER) {
			charge->flags &= ~CHARGE_USB_CHARGER;
			charge->flags |= CHARGE_MAIN_CHARGER;
			if (charge->Online != NULL)
				charge->Online(POWER_SUPPLY_TYPE_MAINS);
			power_supply = DRVCHG_MAIN_CHARGER;
		}
		g_chg_state = ACER_STA_RUN;
	case ACER_STA_RUN:
		//notifyChargeType();
		break;
	default:
		break;
	}
}

/******************************************************
** charge_startCharge function
******************************************************/
#include <linux/delay.h>

static void acerchg_insertCharger(int mode)
{
	max_current = main_max_current;
    charge->temp.measurenb = 0;
    charge->volt.measurenb = 0;

	charge->flags |= CHARGE_CHARGING;

	/* Charge current is limited by PMU, conpute trickle current to match it */
    //trickleCurrent = (mode == CC_ACCHG) ? ((128 * CHARGE_MAIN_CUR_HIGH / 255) - 1) : ((128 * CHARGE_USB_CUR_HIGH / 255) - 1);
    pcf506XX_usb_charge_enable(0);
    pcf506XX_main_charge_enable(1);
    pcf506XX_setAutoStop(0);
    pcf506XX_setVmax(PCF506XX_VMAX_4V22);
    //pcf506XX_setMainChgCur((mode == CC_ACCHG) ? CHARGE_MAIN_CUR_HIGH : CHARGE_USB_CUR_HIGH);
    //pcf506XX_setTrickleChgCur(trickleCurrent);
    setCCreg(mode);

    charge->temp.suspended = 0;

    /* Start Max time charge timer */
    //if ( !timer_pending(&charge->mainchgtimer) ) {
    //    init_timer(&charge->mainchgtimer);
    //    charge->mainchgtimer.function=charge_mainChgtimeout;
    //    charge->mainchgtimer.expires = round_jiffies(jiffies + CHARGE_MAIN_CHG_MAX_TIME);
    //    add_timer(&charge->mainchgtimer);
    //}

    /* Start scan temperature timer */
    if ( !timer_pending(&charge->scantemptimer) ) {
        init_timer(&charge->scantemptimer);
        charge->scantemptimer.function=charge_scanTemptimeout;
        charge->scantemptimer.expires = jiffies + CHARGE_FAST_TIMER_TEMP;
        charge->scantemptimer.data= (u_long)charge;
        add_timer(&charge->scantemptimer);
    }

    /* Start scan voltage timer */
    if ( !timer_pending(&charge->scanvolttimer) ) {
        init_timer(&charge->scanvolttimer);
        charge->scanvolttimer.function=charge_scanVolttimeout;
        charge->scanvolttimer.expires = jiffies + CHARGE_FAST_TIMER_VOLT;
        charge->scanvolttimer.data= (u_long)charge;
        add_timer(&charge->scanvolttimer);
    }
	else
	{
		mod_timer(&charge->scanvolttimer, jiffies + CHARGE_FAST_TIMER_VOLT);
	}

	if (!(g_chg_flag & CHGF_CVCHG)) {
		int i = 0;
		do {
			if (pcf506XX_getIlimit()) {
				charge_cccvCbk();
				break;
			}
			mdelay(1);
		} while (i++ < 10);
	}

	// Trigger the OTP mechanism to check
	if (charge->Online != NULL)
		charge->Online(POWER_SUPPLY_START_CHARGE);
}

// Note : acerchg_insertCharger_work
//        CC_PRECHG, CC_ACCHG, CC_USBCHG : acerchg_insertCharger
//        CC_NOCHG, CC_NOSAVE_NOCHG : setCCreg
static void inschg_proc_work(struct work_struct *work)
{
	switch (g_inschg_cmd) {
	case CC_PRECHG:
	case CC_ACCHG:
	case CC_USBCHG:
		if ((g_chg_flag & CHGF_PAUSE) == 0) {
			acerchg_insertCharger(g_inschg_cmd);
			break;
		}
	case CC_NOCHG:
	case CC_NOSAVE_NOCHG:
		setCCreg(g_inschg_cmd);
		break;
	case CC_WQ_SUSPEND:
		acerchg_suspendCharge();
		break;
	case CC_WQ_RESTART:
		acerchg_resumeCharge();
		break;
	}
}

// Note : acerchg_insertCharger_work should be CC_PRECHG, CC_ACCHG, CC_USBCHG
//        CC_NOCHG, CC_NOSAVE_NOCHG don't enable acerchg_insertCharger procedure.
static void acerchg_insertCharger_work(int mode)
{
	g_inschg_cmd = mode;
	schedule_work(&g_inschg_work);
}

// static void charge_suspendCharge(void)
static void acerchg_suspendCharge(void)
{
	charge_stopMainCharge();
	setCCreg(CC_NOSAVE_NOCHG);

	/* Reset Voltage Table */
	charge->volt.measurenb = 0;

	charge->scantemptimer.expires = round_jiffies(jiffies + CHARGE_NORM_TIMER_TEMP);
	charge->temp.suspended = 1;
}

static void acerchg_resumeCharge(void)
{
	if (g_chg_flag & CHGF_PAUSE) {
		//if (showdbg())
			printk(KERN_INFO "<CHG> resume the Charge, but have suspend event(%lx).\n", g_chg_flag);
	}
	else {
		acerchg_insertCharger(g_chg_cc);
	}
}

/******************************************************
** drvchg dispatch function
******************************************************/
static void drvchg_dispatch_func(int func, int param)
{
	switch (func) {
	case DRVCHG_CHGINS:
		acerchg_chgins();
		break;
	case DRVCHG_USBEVENT:
		g_usbevent = param;
		schedule_work(&g_usbchg_work);
		break;
	}
}

