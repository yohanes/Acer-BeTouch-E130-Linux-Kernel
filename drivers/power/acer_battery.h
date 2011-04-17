#include <linux/pcf506XX.h>

#define isInCharging ((pcf506XX_mainChgPlugged()) && (drvchg_get_charging_state()))

extern bool drvchg_get_charging_state(void);

extern void bibatchg_registerbattery(int * ppnx_batt_level);
extern void bibatchg_unregisterbattery(void);

/******************************************************
** Battery Voltage convent Battery Capacity procedure
******************************************************/
#include <nk/adcclient.h>

#define BAT_CC_MXTC		0x7F
#define BAT_CC_PV		176013		// 4293 * 41
#define BAT_CC_MNTC		0x10		// (16 + 1) / 128 * 750mA = 99.609mA

static ulong g_batt_volt = 0;

/******************************************************
** poll Battery Voltage routine
******************************************************/
#define TSEC	1000
#define QBAT_INIT_TIMER		29*TSEC
#define QBAT_INIT_LOADING	17172	// 4*4293
#define QBAT_RUN_TIMER		30*TSEC
#define QBAT_SUSPEND_TIMER	120*TSEC

static void battery_pooling_work_proc(struct work_struct *work_timer);
static void battery_polling_timer(unsigned long unused);

static unsigned long g_battpoll_time = QBAT_INIT_TIMER;
static struct timer_list g_battpoll_timer;
static struct work_struct g_battpoll_work;

/******************************************************
** the early suspend function
******************************************************/
#include <linux/earlysuspend.h>
static void pnxbat_early_suspend(struct early_suspend *h);
static void pnxbat_late_resume(struct early_suspend *h);

static struct early_suspend g_bat_early_suspend;

/******************************************************
** Over Temperature Protect
******************************************************/
#define QBID_INIT_TIMER			TSEC/2
#define QBID_NORMAL_TIMER		60*TSEC
#define QBID_NORMAL_FSAT_TIMER	 5*TSEC
#define QBID_CHARGE_FAST_TIMER	 1*TSEC

// NTC unit : LSB
#define NULL_BATTERY_ID 1400	// 1460
#define MAX_CHARGE_TEMP 445		//  0
#define MIN_CHARGE_TEMP 104		// 45
#define RECHARGE_MAX_TEMP 380	//  5
#define RECHARGE_MIN_TEMP 121	// 40
#define MAX_OPERATE_TEMP 688	// -15
#define MIN_OPERATE_TEMP  76	//  55

static ulong g_temp_buf[4], g_temp_index = 6, g_avtemp;

static struct work_struct g_otp_work;
static struct timer_list g_otp_timer;

static void otp_adc_batid(ulong volt);
static void otp_work_proc(struct work_struct *work_timer);
static void otp_timer_func(unsigned long unused);

extern ulong isChgFlag(ulong flag);
extern void ctrlCharge(ulong on, ulong flag);

// for fix A41.B-674, A21.B-358
static int pnx_batt_temp = 300;

/******************************************************
** 1500mAh Battery Voltage Capacity mapping table
******************************************************/
struct tBatVoltTable {
	ulong volt;
	int level;
	ulong bound;
};

const static struct tBatVoltTable g_normalvolttable[] = {
	{4150670, 2100,  69330}, \
	{4093400, 1100, 126600}, /*PMU recharge*/ \
	{4084597, 2095,  66073}, /*95*/ \
	{4041566,   90,  43031}, \
	{3997314,   85,  44252}, \
	{3950926,   80,  46388}, \
	{3924833,   75,  26093}, \
	{3894620,   70,  30213}, \
	{3864711,   65,  29909}, \
	{3830073,   60,  34638}, \
/*	{3800470,   55,  29603}, */ \
	{3781701,   50,  18769}, \
/*	{3766289,   45,  15412}, */ \
	{3753166,   40,  13123}, \
/*	{3742485,   35,  10681}, */ \
	{3736534,   30,   5951}, \
/*	{3729209,   25,   7325}, */ \
	{3716544,   20,  12665}, \
	{3691061,   15,  25483}, \
	{3657186, 2010,  33875}, \
	{3651311, 1010,  39750}, \
	{3645436,    5,   5875}, \
	{3573183,    3,  72253}, \
	{3500931,    0,  72252}, \
	{      0,    0,      0}
};

const static struct tBatVoltTable g_coolvolttable[] = {
	{4165500, 2100,  54500}, \
	{4093400, 1100, 126600}, /*PMU recharge*/ \
	{4026000, 2095, 139500}, /*95*/ \
	{3997500,   90,  28500}, \
	{3941000,   85,  56500}, \
	{3885000,   80,  56000}, \
	{3853000,   75,  32000}, \
	{3826000,   70,  27000}, \
	{3799000,   65,  27000}, \
	{3775000,   60,  24000}, \
/*	{3753500,   55,  21500}, */ \
	{3735000,   50,  18500}, \
/*	{3719500,   45,  15500}, */ \
	{3705500,   40,  14000}, \
/*	{3695500,   35,  10000}, */ \
	{3687000,   30,   8500}, \
/*	{3675500,   25,  11500}, */ \
	{3660500,   20,  15000}, \
	{3637000,   15,  23500}, \
	{3613500, 2010,  23500}, \
	{3599750, 1010,  37250}, \
	{3586000,    5,  13750}, \
	{3543000,    3,  43000}, \
	{3500000,    0,  43000}, \
	{      0,    0,      0}
};

static struct {
	uint range;						// BV_COVER_AREA	// cover the max charge jumping voltage
	ulong lowvolt;
	struct tBatVoltTable * table;
} * g_batparam, g_batdata[] = {
	{20, 3657186, (struct tBatVoltTable *)g_normalvolttable},
	{70, 3613500, (struct tBatVoltTable *)g_coolvolttable}
};

/******************************************************
**  Battery Temperature mapping table
******************************************************/
const static struct {
	int temperature;
	ulong temp_lsb;
} g_battemp_table[] = {
	{-400, 1154}, \
	{-390, 1138}, \
	{-380, 1121}, \
	{-370, 1104}, \
	{-360, 1086}, \
	{-350, 1068}, \
	{-340, 1050}, \
	{-330, 1032}, \
	{-320, 1014}, \
	{-310,  995}, \
	{-300,  976}, \
	{-290,  957}, \
	{-280,  938}, \
	{-270,  918}, \
	{-260,  899}, \
	{-250,  879}, \
	{-240,  860}, \
	{-230,  840}, \
	{-220,  821}, \
	{-210,  802}, \
	{-200,  782}, \
	{-190,  763}, \
	{-180,  744}, \
	{-170,  725}, \
	{-160,  706}, \
	{-150,  688}, \
	{-140,  670}, \
	{-130,  652}, \
	{-120,  634}, \
	{-110,  616}, \
	{-100,  599}, \
	{ -90,  582}, \
	{ -80,  566}, \
	{ -70,  550}, \
	{ -60,  534}, \
	{ -50,  518}, \
	{ -40,  503}, \
	{ -30,  488}, \
	{ -20,  473}, \
	{ -10,  459}, \
	{   0,  445}, /* OTP : Stop Charging */ \
	{  10,  431}, \
	{  20,  418}, \
	{  30,  405}, \
	{  40,  392}, \
	{  50,  380}, /* OTP : resume Charging */ \
	{  60,  368}, \
	{  70,  357}, \
	{  80,  345}, \
	{  90,  334}, \
	{ 100,  324}, \
	{ 110,  313}, \
	{ 120,  303}, \
	{ 130,  294}, \
	{ 140,  284}, \
	{ 150,  275}, \
	{ 160,  266}, \
	{ 170,  258}, \
	{ 180,  249}, \
	{ 190,  241}, \
	{ 200,  233}, \
	{ 210,  226}, \
	{ 220,  218}, \
	{ 230,  211}, \
	{ 240,  205}, \
	{ 250,  198}, \
	{ 260,  192}, \
	{ 270,  185}, \
	{ 280,  179}, \
	{ 290,  174}, \
	{ 300,  168}, \
	{ 310,  163}, \
	{ 320,  157}, \
	{ 330,  152}, \
	{ 340,  147}, \
	{ 350,  143}, \
	{ 360,  138}, \
	{ 370,  134}, \
	{ 380,  130}, \
	{ 390,  125}, \
	{ 400,  121}, /* OTP : resume Charging */ \
	{ 410,  118}, \
	{ 420,  114}, \
	{ 430,  110}, \
	{ 440,  107}, \
	{ 450,  104}, /* OTP : Stop Charging */ \
	{ 460,  100}, \
	{ 470,   97}, \
	{ 480,   94}, \
	{ 490,   92}, \
	{ 500,   89}, \
	{ 510,   86}, \
	{ 520,   83}, \
	{ 530,   81}, \
	{ 540,   78}, \
	{ 550,   76}, \
	{ 560,   74}, \
	{ 570,   72}, \
	{ 580,   70}, \
	{ 590,   68}, \
	{ 600,   66}, /* Power off device */ \
	{ 610,   64}, \
	{ 620,   62}, \
	{ 630,   60}, \
	{ 640,   58}, \
	{ 650,   57}, \
	{ 660,   55}, \
	{ 670,   53}, \
	{ 680,   52}, \
	{ 690,   50}, \
	{ 700,   49}, \
	{ 710,    0}
};

