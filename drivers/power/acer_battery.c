/******************************************************
** Battery Voltage convent Battery Capacity procedure
******************************************************/
#include <linux/pcf506XX.h>

#define	VBAT_CONTINUE_READ_CNT	4

static ulong g_invbat_buf[VBAT_CONTINUE_READ_CNT];
static ulong g_invbat_index = 0;

static void get_battvolt_adc(unsigned long volt)
{
	if (volt > 4300000) {
		printk(KERN_INFO "<BAT> abnormal ADC(%8ld) at (%8ld) : abandon.\n", volt, g_batt_volt);
		return;
	}

	g_invbat_buf[g_invbat_index++] = volt;

	if (showdbg())
		printk(KERN_INFO "<BAT> rd : chg(%d) i%7ld\n", pcf506XX_mainChgPlugged(), volt);
}

static int read_battery_volt(void)
{
	int i;
	ulong v, sum = 0;

	g_invbat_index = 0;
	for (i = 0; i < VBAT_CONTINUE_READ_CNT; i++) {
		adc_aread(FID_VBAT, get_battvolt_adc);
		mdelay(10);
	}

	if (g_invbat_index < 2) {
		printk(KERN_INFO "<BAT> can't get normal ADC, try again.\n");
		mod_timer(&g_battpoll_timer,
		          jiffies + msecs_to_jiffies(5000));
		return -1;
	}

	for (v = g_invbat_buf[0], i = 1; i < g_invbat_index; i++) {
		if (g_invbat_buf[i] < v) {
			sum += v;
			v = g_invbat_buf[i];
		}
		else
			sum += g_invbat_buf[i];
	}
	sum /= (g_invbat_index - 1);

	return sum;
}

#define BV_ALL_TIME		2		// battery voltage accept large leap time : 1.5 min
#define BV_AUD_TIME		5		// battery voltage accept up time at dischage : 2.5 min

static ulong g_oldvolt = 0;
static int g_adc_large_leap_cnt = 1111;
static ulong g_upcnt = 0;

static bool check_leap_cnt(ulong volt, int delta)
{
	if (g_adc_large_leap_cnt < BV_ALL_TIME) {
		printk(KERN_INFO "<BAT> ADC form %7ld to %7ld : (%d, %d) abandon.\n", g_batt_volt, volt, delta, g_adc_large_leap_cnt);
		printk(KERN_INFO "<BAT> buf[%ld] : %7ld, %7ld, %7ld, %7ld - o(%7ld)\n",
			g_invbat_index, g_invbat_buf[0], g_invbat_buf[1], g_invbat_buf[2], g_invbat_buf[3], g_oldvolt);
		g_adc_large_leap_cnt++;
		return true;
	}
	else {
		printk(KERN_INFO "<BAT> ADC form %7ld to %7ld : (%d, %d) accept.\n", g_batt_volt, volt, delta, g_adc_large_leap_cnt);
		// g_loadvolt = g_loadcnt = 0;
		g_upcnt = 0;
	}
	g_adc_large_leap_cnt = 0;
	return false;
}

#define BB_REDUCE_TIME	180		// talk time : 90 min * 2

static ulong g_loadvolt = 0, g_loadcnt = 0;

static ulong getLoadVoltage(void)
{
	if (g_loadcnt > (BB_REDUCE_TIME - 1)) return 0;
	else if (g_loadcnt) return g_loadvolt * g_loadcnt * g_loadcnt / BB_REDUCE_TIME * g_loadcnt / (BB_REDUCE_TIME * BB_REDUCE_TIME);
	else return g_loadvolt;
}

#define ADC_DIV			4293
#define ADC_ADMIT_DIV	2
#define BB_MAX_SIZE		20

static ulong g_avbat_buf[16], g_avbat_index = 0, g_avbat_volt;
static ulong g_batvolt_index = 12;
static int g_pnx_batt_level;

extern int8_t get_trickleCur(void);

static void cal_battery_volt(ulong volt)
{
	ulong i, v = volt;

	if (isInCharging) {
		int delta;
		g_loadvolt = g_loadcnt = g_upcnt = 0;
		if (get_trickleCur() == BAT_CC_MXTC) {
			v = v - BAT_CC_PV;
		}
		else if (get_trickleCur() > BAT_CC_MNTC) {
			v = v - (BAT_CC_PV * get_trickleCur() / BAT_CC_MXTC);
		}

		delta = (int)v - (int)g_batt_volt;
		if ((delta > (ADC_DIV * g_batparam->range)) && (check_leap_cnt(volt, delta))) {
			return;
		}

		if (v > g_batt_volt) g_batt_volt = v;
	}
	else {
		int delta = (int)volt - (int)g_batt_volt;

		if ((delta < (-ADC_DIV * g_batparam->range)) || ((ADC_DIV * g_batparam->range) < delta)) {
			if (check_leap_cnt(volt, delta)) return;
			else {
				g_batt_volt = volt;
				delta = 0;
			}
		}

		if ((((-ADC_DIV * ADC_ADMIT_DIV) < delta) && (delta < (ADC_DIV * 4))) || (delta == 0)) {	// d <= 3 : 1.5 min_pre_scale
			if (delta < 0) {
				// *** The voltage in permissible variation area
				g_upcnt = 0;
				g_batt_volt = volt;
			}
			else {
				// *** The voltage in drift stage. (g_upcnt <= 5) include (g_batt_volt == 0)
				// g_upcnt keep
				// g_batt_volt don't chang
			}
			v = g_loadvolt = g_loadcnt = 0;		// v=0 : trace
		}
		else {
			if (delta > 0) {
				// *** (battery voltage leap)
				if (g_upcnt > (BV_AUD_TIME - 1)) {
					// *** the leap status still over 2 min
					g_batt_volt = volt;
					g_upcnt = 0;
				}
				else {
					g_upcnt++;
					// g_batt_volt don't chang
				}
				g_loadvolt = g_loadcnt = 0;
			}
			else {
				// *** battery voltage fall.
				g_upcnt = 0;

				if (g_loadvolt) {
					g_loadcnt++;
					if ((v = getLoadVoltage()) == 0) {
						g_loadvolt = g_loadcnt = 0;
					}
				}

				if (g_loadvolt) {
					delta = (int)(volt + v) - (int)g_batt_volt;
					if (((-ADC_DIV * ADC_ADMIT_DIV) <= delta) && (delta < (ADC_DIV * ADC_ADMIT_DIV))) {
						if (delta < 0)
							g_batt_volt = volt + v;
						// g_loadcnt keep
						// g_batt_volt don't chang
					}
					else {
						// *** the voltage drop or leap
						if (delta > 0) {
							// *** the load reduces
							g_loadvolt = (((v - delta) / 8) * g_loadvolt) / (v / 8);	// may overflow!!!
							// g_loadcnt keep
							// g_batt_volt = volt + getLoadVoltage();
						}
						else {
							// *** add new load
							g_loadvolt = v - delta;	// add port
							if (g_loadvolt > (ADC_DIV * BB_MAX_SIZE))
								g_loadvolt = ADC_DIV * BB_MAX_SIZE;
							g_loadcnt = 0;
							g_batt_volt = volt + g_loadvolt;	// getLoadVoltage()
						}
					}
				}
				else {
					// *** 1st big fall, delta < (-ADC_DIV * ADC_ADMIT_DIV)
					if (delta < (-ADC_DIV * BB_MAX_SIZE)) {		// max between talk and standby
						// *** biggest fall
						g_loadvolt = ADC_DIV * BB_MAX_SIZE;
						g_batt_volt = volt + g_loadvolt;
					}
					else {
						g_loadvolt = g_batt_volt - volt;		// -delta
						// g_batt_volt don't chang
					}
					g_loadcnt = 0;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// average battery voltage
	v = g_batt_volt;
	if (g_avbat_index > 17) {
		// initial battery buffer
		g_avbat_index = 0;
		for (i = 0; i < 16; i++) {
			g_avbat_buf[i] = v;
		}
		g_avbat_volt = v;
	}
	else {
		/////////////////////////////////////////////////////////////////////
		// bound drop voltage
		if ((g_avbat_volt > v) && (g_batparam->table == g_normalvolttable)) {
			i = g_avbat_volt - v;
			if ((i > g_batparam->table[g_batvolt_index].bound) && (g_battpoll_time == QBAT_RUN_TIMER))
				v = g_avbat_volt - g_batparam->table[g_batvolt_index].bound;
			// speed up when level < 20%
			if (g_avbat_volt < 3716544) {
				for (i = 0; i < 3; i++) {
					g_avbat_buf[g_avbat_index++] = v;
					g_avbat_index &= 0x0f;
				}
			}
		}
// don't bound vbat rise.
//		else {
//			i = v - g_avbat_volt;
//			if ((i > g_batparam->table[g_batvolt_index].bound) && (3645436 < v) && (v < 3984000))
//				v = g_avbat_volt + g_batparam->table[g_batvolt_index].bound;
//		}

		g_avbat_buf[g_avbat_index++] = v;
		g_avbat_index &= 0x0f;
		for (g_avbat_volt = g_avbat_buf[0], i = 1; i < 16; i++) {
			g_avbat_volt += g_avbat_buf[i];
		}
		g_avbat_volt /= 16;
	}

	if (showdbg())
		printk(KERN_INFO "<BAT> cp : chg(%d:%d) i%7ld - v%7ld - u %ld -  l(%7ld, %3ld) [%7lduV - %7lduV - %d%%]\n",
		       pcf506XX_mainChgPlugged(), drvchg_get_charging_state(), volt, v, g_upcnt, g_loadvolt, g_loadcnt, g_batt_volt, g_avbat_volt, g_pnx_batt_level);

	g_oldvolt = volt;
}

static void mapping_battery_level(void)
{
	int i;
	for (i = 0, g_pnx_batt_level = 0; (g_batparam->table[i].volt); i++) {
		if (g_batparam->table[i].level > 2000) {
			if (isInCharging) {
				if (g_avbat_volt >= g_batparam->table[i].volt) {
					g_pnx_batt_level = g_batparam->table[i].level - 2000;
					break;
				}
			}
		}
		else if (g_batparam->table[i].level > 1000) {
			if (!isInCharging) {
				if (g_avbat_volt >= g_batparam->table[i].volt) {
					g_pnx_batt_level = g_batparam->table[i].level - 1000;
					break;
				}
			}
		}
		else {
			if (g_avbat_volt >= g_batparam->table[i].volt) {
				g_pnx_batt_level = g_batparam->table[i].level;
				break;
			}
		}
	}
	g_batvolt_index = i;
}

/******************************************************
** poll Battery Voltage routine
******************************************************/
#include <linux/delay.h>

static void temp_sel_table(ulong lsb)
{
	printk(KERN_INFO "<BAT> **************************************************\n");
	if (lsb > 445) {
		printk(KERN_INFO "<BAT> set mapping table to cool table at %ld.\n", lsb);
		g_batparam = &g_batdata[1];
	}
	else {
		printk(KERN_INFO "<BAT> set mapping table to normal table at %ld.\n", lsb);
		g_batparam = &g_batdata[0];
	}
	printk(KERN_INFO "<BAT> **************************************************\n");
}

static int g_k_lv_cnt = 0;
static int g_lowbatoffcnt = 0;
static ulong g_bat_screenoff = 0;

extern void pcf50623_poweroff(void);

static void battery_pooling_work_proc(struct work_struct *work_timer)
{
	int volt = 0;
	int i = 0;
	if ((volt = read_battery_volt()) < 0)
		return;

	if (g_battpoll_time == QBAT_INIT_TIMER) {
		adc_aread(FID_BATPACK, temp_sel_table);
		volt += QBAT_INIT_LOADING;
		if (isInCharging) {
			if (get_trickleCur() == BAT_CC_MXTC) {
				volt = volt - BAT_CC_PV;
			}
			else if (get_trickleCur() > BAT_CC_MNTC) {
				volt = volt - (BAT_CC_PV * get_trickleCur() / BAT_CC_MXTC);
			}
		}
		for (i = 0; i < 16; i++) {
			g_avbat_buf[i] = volt;
		}
		g_avbat_index = 0;
		g_avbat_volt = volt;
		mapping_battery_level();
		pnx_bat_level(g_pnx_batt_level);
		g_battpoll_time = QBAT_RUN_TIMER;
	}
	else {
		int level = g_pnx_batt_level;

		cal_battery_volt((ulong)volt);
		mapping_battery_level();

		if (isInCharging) {
			// charging
			if (g_pnx_batt_level < pnx_batt_level) {
				if (g_pnx_batt_level < (pnx_batt_level - 40)) {
					g_k_lv_cnt = 0;
					//level = g_pnx_batt_level;
				}
				else {
					if (g_k_lv_cnt > 15) {
						g_k_lv_cnt = 0;
						//level = g_pnx_batt_level;
					}
					else {
						if (g_pnx_batt_level >= (pnx_batt_level - 10)) g_k_lv_cnt = 0;
						else g_k_lv_cnt++;
						level = pnx_batt_level;
					}
				}
			}
			else {
				if (g_pnx_batt_level > (pnx_batt_level + 10)) {
					level = pnx_batt_level;
					if (g_k_lv_cnt < 0) {
						if (g_k_lv_cnt < -3) {
							level += 5;
							g_k_lv_cnt = -1;
						}
						else
							g_k_lv_cnt--;
					}
					else
						g_k_lv_cnt = -1;
				}
				else {
					g_k_lv_cnt = 0;
					//level = g_pnx_batt_level;
				}
			}
		}
		else {
			// discharging
			level = ((g_pnx_batt_level < pnx_batt_level) && (g_upcnt == 0)) ? g_pnx_batt_level : pnx_batt_level;
			g_k_lv_cnt = 0;
		}

		if ((showdbg()) && (pnx_batt_level != level))
			printk(KERN_INFO "<BAT> update battery level (%3d-%3d) : %d%%\n", pnx_batt_level, g_pnx_batt_level, level);

		pnx_bat_level(level);
	}

	// [backup solution] resolve the android middle ware problem that
	// the low battery state sometimes can't power off in suspend.
	if ((!isInCharging) && (pnx_batt_level < 5)) {
		printk(KERN_INFO "<BAT> detect pnx_batt_level is %d%% at %7ld : %d\n", pnx_batt_level, g_batt_volt, g_lowbatoffcnt);
		if ((g_bat_screenoff) && (g_lowbatoffcnt > 3)) {
			printk(KERN_INFO "<BAT> Low Battery Power Off at suspend.\n");
			pcf50623_poweroff();
		}
		else g_lowbatoffcnt++;
	}
	else g_lowbatoffcnt = 0;

	mod_timer(&g_battpoll_timer,
	          jiffies + msecs_to_jiffies((volt < g_batparam->lowvolt) ? (g_battpoll_time = QBAT_RUN_TIMER) :
	              (g_loadvolt) ? QBAT_RUN_TIMER : g_battpoll_time));		// (g_battpoll_time == QBAT_RUN_TIMER)
}

static void battery_polling_timer(unsigned long unused)
{
	schedule_work(&g_battpoll_work);
}

/******************************************************
** the early suspend function
******************************************************/
static void pnxbat_early_suspend(struct early_suspend *h)
{
	g_bat_screenoff = 1;
	g_battpoll_time = QBAT_SUSPEND_TIMER;
}

static void pnxbat_late_resume(struct early_suspend *h)
{
	g_bat_screenoff = 0;
	g_battpoll_time = QBAT_RUN_TIMER;

	if (showdbg())
		printk(KERN_INFO "<BAT> pnxbat_late_resume : %8ld\n", g_batt_volt);

	mod_timer(&g_battpoll_timer,
	          jiffies + msecs_to_jiffies(1000));
}

/******************************************************
** Battery ID Routine
** 1. Null Battery ID, Stop Charge
** 2. Over Temperature Protect
**    - normal : T > 60 , Power Off device
**    - charging : T < 0 && 45 < T, Stop Charge
******************************************************/
static void mapping_battery_temp(void)
{
	int i;
	for (i = 0, pnx_batt_temp = 715; (g_battemp_table[i].temp_lsb); i++) {
		if (g_avtemp >= g_battemp_table[i].temp_lsb) {
			pnx_batt_temp = g_battemp_table[i].temperature;
			break;
		}
	}
}

static void otp_adc_batid(ulong volt)
{
#if defined(ACER_K2_PR1) || defined(ACER_K3_PR1)
	if (showdbglv(DBGMSG_TEMP))
		printk(KERN_INFO "<BAT> skip battery id (%4ld). [%ld]\n", volt, drvchg_get_temperature()/100);
	volt = 198;		// 25
#endif	// defined(ACER_K2_PR1) || defined(ACER_K3_PR1)
	if (volt > NULL_BATTERY_ID) {
		if (g_temp_index < 9) {
			g_temp_index = 9;
			if ((pcf506XX_mainChgPlugged()) && (!isChgFlag(CHGF_P_BID))) {
				printk(KERN_INFO "<BAT> detect NULL_BATTERY_ID, stop Charging : (%ld, %4ld) %4ld\n", g_temp_index, g_avtemp, volt);
				ctrlCharge(0, CHGF_P_BID);
			}
		}
		else {
			g_temp_index = 10;
		}
		g_avtemp = volt;
		mod_timer(&g_otp_timer,
		          jiffies + msecs_to_jiffies(QBID_NORMAL_FSAT_TIMER));
	}
	else {
		if (g_temp_index > 4) {
			if (g_temp_index >= 9) {
				if (pcf506XX_mainChgPlugged())
					ctrlCharge(1, CHGF_P_BID);
			}
			g_avtemp = g_temp_buf[3] = g_temp_buf[2] = g_temp_buf[1] = g_temp_buf[0] = volt;
			g_temp_index = 0;
		}
		else {
			g_temp_buf[g_temp_index] = volt;
			g_temp_index++;
			g_temp_index &= 0x03;
			g_avtemp = (g_temp_buf[0] + g_temp_buf[1] + g_temp_buf[2] + g_temp_buf[3]) / 4;
		}

		if (pcf506XX_mainChgPlugged()) {
			if (!showdbglv(DBGFLAG_MONKEY)) {
				if ((!isChgFlag(CHGF_P_OTP)) && ((g_avtemp < MIN_CHARGE_TEMP) || (MAX_CHARGE_TEMP < g_avtemp))) {
					printk(KERN_INFO "<BAT> Stop charge -----> %lx, %x\n", isChgFlag(0xffffffff), ((g_avtemp < MIN_CHARGE_TEMP) || (MAX_CHARGE_TEMP < g_avtemp)));
					ctrlCharge(0, CHGF_P_OTP);
				}
				else if ((isChgFlag(CHGF_P_OTP)) && ((RECHARGE_MIN_TEMP < g_avtemp) && (g_avtemp < RECHARGE_MAX_TEMP))) {
					printk(KERN_INFO "<BAT> Resume charge -----> %lx, %x\n", isChgFlag(0xffffffff), ((RECHARGE_MIN_TEMP < g_avtemp) && (g_avtemp < RECHARGE_MAX_TEMP)));
					ctrlCharge(1, CHGF_P_OTP);
				}
			}

			mod_timer(&g_otp_timer,
			          jiffies + msecs_to_jiffies(((isChgFlag(CHGF_CVCHG)) ||
			          	((volt < RECHARGE_MIN_TEMP) || (RECHARGE_MAX_TEMP < volt))) ? QBID_CHARGE_FAST_TIMER : QBID_NORMAL_TIMER));
		}
		else {
			mod_timer(&g_otp_timer, jiffies + msecs_to_jiffies(
				((volt < MIN_OPERATE_TEMP) || (MAX_OPERATE_TEMP < volt)) ? QBID_NORMAL_FSAT_TIMER : QBID_NORMAL_TIMER));
		}
	}

	mapping_battery_temp();
	if ((!showdbglv(DBGFLAG_MONKEY)) && ((pnx_batt_temp <= -200) || (600 <= pnx_batt_temp)))
		pnx_bat_update(&bat_ps);
	if (showdbglv(DBGMSG_TEMP))
		printk(KERN_INFO "<BAT> otp_adc_batid : [%ld] (%ld - %lx) %4ld -> %4ld : %d\n",
			drvchg_get_temperature()/100, g_temp_index, isChgFlag(0xffffffff), volt, g_avtemp, pnx_batt_temp);
}

static void otp_work_proc(struct work_struct *work_timer)
{
	adc_aread(FID_BATPACK, otp_adc_batid);
}

static void otp_timer_func(unsigned long unused)
{
	schedule_work(&g_otp_work);
}

