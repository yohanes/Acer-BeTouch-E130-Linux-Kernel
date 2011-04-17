/******************************************************
** acer_drvchg define
******************************************************/
#undef CHARGE_MAIN_CHG_MAX_TIME
#undef CHARGE_INIT_TIME
#undef CHARGE_TEMP_MIN_TO_CHARGE
#undef CHARGE_TEMP_MAX_TO_CHARGE
#undef CHARGE_TEMP_MIN_TO_RESTART
#undef CHARGE_TEMP_MAX_TO_RESTART
#undef CHARGE_CURRENT_MIN
#undef CHARGE_MAIN_CUR_LOW
#undef CHARGE_MAIN_CUR_HIGH
#undef CHARGE_USB_CUR_HIGH
#undef ICCSET

#define CHARGE_MAIN_CHG_MAX_TIME      21600*HZ // 6hr = 360 min
#define CHARGE_INIT_TIME              15*HZ    // 15s
#define CHARGE_TEMP_MIN_TO_CHARGE     -5000    // en mili °C
#define CHARGE_TEMP_MAX_TO_CHARGE     55000    // en mili °C
#define CHARGE_TEMP_MIN_TO_RESTART     0       // en mili °C
#define CHARGE_TEMP_MAX_TO_RESTART    50000    // en mili °C

#ifdef ACER_ACER_REQ			/* 100/450/750 mA */
#define CHARGE_CURRENT_MIN            0x10
#define CHARGE_MAIN_CUR_LOW  0x22		// 100mA
#define CHARGE_MAIN_CUR_HIGH 0xFF		// 750mA
#define CHARGE_USB_CUR_HIGH  0x99		// 450mA
#define ICCSET					750
#else	// ACER_ACER_REQ		/* 100/450/500 mA */
#define CHARGE_CURRENT_MIN            0x15
#define CHARGE_MAIN_CUR_LOW  0x33		// 100mA
#define CHARGE_MAIN_CUR_HIGH 0xFF		// 500mA
#define CHARGE_USB_CUR_HIGH  0xE5		// 450mA
#define ICCSET					500
#endif	// ACER_ACER_REQ

/******************************************************
** the function from acer_bibatchg
******************************************************/
extern int bibatchg_registerdrvchg(int16_t ** pdevCompVolt, void (*pfusbchg_event)(int, int));
extern void bibatchg_unregisterdrvchg(void);

/******************************************************
** the function for pnx_battery
******************************************************/
int8_t get_trickleCur(void);
ulong isChgFlag(ulong flag);
void ctrlCharge(ulong on, ulong flag);

/******************************************************
** protect charge state access area
******************************************************/
#define INIT_CHG_LOCK(x)	mutex_init(x)
#define CHG_LOCK(x)			mutex_lock(x)
#define CHG_UNLOCK(x)		mutex_unlock(x)

struct mutex g_chg_cc_lock;

/******************************************************
** control the CC current
******************************************************/
enum {
	CC_NOCHG,
	CC_PRECHG,
	CC_ACCHG,
	CC_USBCHG,
	CC_NOSAVE_NOCHG};
#define CC_WQ_SUSPEND	10
#define CC_WQ_RESTART	11
static int g_chg_cc;

static int g_chg_cmd;
static struct work_struct g_acerchg_work;

static void acerchg_proc_work(struct work_struct *work);
static void setCCreg(int mode);

/******************************************************
** [PRM] The AC/USB power supply recognition mechanism
******************************************************/
#define CHG_INRUCH_TIMER		100
#define CHG_INIT_CK_CHGTYPE_TIMER	5000
#define CHG_CK_CHGTYPE_TIMER		500

enum {
	ACER_STA_RM,
	ACER_STA_NONE,
	ACER_STA_INRUCH,
	ACER_STA_DETECT,
	ACER_STA_RUN};
static int g_chg_state = ACER_STA_RM;

enum {
	ACER_CHG_UNKNOW,
	ACER_CHG_INRUCH,
	ACER_CHG_DETECT,
	ACER_CHG_750MA,		// ACER_CHG_AC
	ACER_CHG_AC_S,
	ACER_CHG_450MA,		// ACER_CHG_USB
	ACER_CHG_USB_S};
static ulong g_chg_type = ACER_CHG_UNKNOW;

static struct timer_list chg_ctrl_timer;
static int g_inschg_cmd;
static struct work_struct g_inschg_work;

static void acer_chg_ctrl_func(unsigned long unused);

static void acerchg_insertCharger(int mode);
static void inschg_proc_work(struct work_struct *work);
static void acerchg_insertCharger_work(int mode);
static void acerchg_suspendCharge(void);
static void acerchg_resumeCharge(void);

/******************************************************
** usb charge event
******************************************************/
static struct work_struct g_usbchg_work;
static int g_usbevent;

static void usbchg_proc_work(struct work_struct *work);

/******************************************************
** charge state
******************************************************/
#define CHGF_PAUSE	(CHGF_P_OCP | CHGF_P_OVP | CHGF_P_OTP | CHGF_P_WDT | CHGF_P_THL | CHGF_P_BID | CHGF_P_APC | CHGF_P_USB | CHGF_P_INR)
static ulong g_chg_flag;

inline void ResumeCharge(ulong pause) {g_chg_flag &= ~(pause); if (!(g_chg_flag & CHGF_PAUSE)) setCCreg(g_chg_cc);}

/******************************************************
** detect OVP_FAULT of RT9718
******************************************************/
extern int pnx_read_pmugpio_pin(int gpio);
extern int pnx_set_pmugpio_direction(int gpio, int is_input);

/******************************************************
** drvchg dispatch function
******************************************************/
static void drvchg_dispatch_func(int func, int param);

