#ifndef _LINUX_PCF506XX_H
#define _LINUX_PCF506XX_H


#define ACER_L1_K3
#define ACER_L1_CHANGED


/* public in-kernel pcf506XX api */
/* manage only register not handled by RTK */
enum pcf506XX_regulator_id {
	PCF506XX_REGULATOR_D1REG,
	PCF506XX_REGULATOR_D2REG,
	PCF506XX_REGULATOR_D3REG,
	PCF506XX_REGULATOR_D4REG,
	PCF506XX_REGULATOR_D5REG,
	PCF506XX_REGULATOR_D6REG,
	PCF506XX_REGULATOR_D7REG,
    PCF506XX_REGULATOR_IOREG,
    PCF506XX_REGULATOR_USBREG,
    PCF506XX_REGULATOR_HCREG,
    __NUM_PCF506XX_REGULATORS
};
/* Regulator modes */
enum pcf506XX_regulator_mode {
	PCF506XX_REGU_OFF = 0x00,
	PCF506XX_REGU_ECO,
	PCF506XX_REGU_ON,
#if defined (CONFIG_SENSORS_PCF50616)
	PCF506XX_REGU_ECO_ON_ON_ON,
	PCF506XX_REGU_OFF_ON_ON_OFF,
	PCF506XX_REGU_OFF_OFF_ON_ON,
	PCF506XX_REGU_ECO_ECO_ON_ON,
	PCF506XX_REGU_ECO_ON_ON_ECO,
#else
		/* this mode requires config set in gpiopwn */
	PCF506XX_REGU_ON_OFF,
	PCF506XX_REGU_OFF_ON,
	PCF506XX_REGU_ON_ECO,
	PCF506XX_REGU_ECO_ON,
	PCF506XX_REGU_ECO_OFF,
#endif
    __NUM_PCF506XX_REGULATORS_MODE
};

struct pmu_voltage_rail {
	/* to be set to one , while register is initialized by linux */
	unsigned int used;
	struct {
		unsigned int init;
		unsigned int max;
	} voltage;
	unsigned int mode;
	unsigned int gpiopwn;
};




#if defined CONFIG_SENSORS_PCF50616
struct pcf50616_data;
extern struct pcf50616_data *pcf50616_global;
extern struct pnx_modem_gpio pnx_modem_gpio_assignation;
#define pcf506XX_global pcf50616_global

extern int pcf506XX_voltage_set(struct pcf50616_data *pcf,
                                enum pcf506XX_regulator_id reg,
                                unsigned int millivolts);
extern unsigned int pcf506XX_voltage_get(struct pcf50616_data *pcf,
                                         enum pcf506XX_regulator_id reg);
extern int pcf506XX_onoff_get(struct pcf50616_data *pcf,
                              enum pcf506XX_regulator_id reg);
extern int pcf506XX_onoff_set(struct pcf50616_data *pcf,
                              enum pcf506XX_regulator_id reg, enum pcf506XX_regulator_mode on);

struct pcf50616_platform_data {
	/* general */
	unsigned int used_features;
	unsigned int onkey_seconds_required;

	/* voltage regulator related */
	struct pmu_voltage_rail rails[__NUM_PCF506XX_REGULATORS];
	struct pmu_reg_table *reg_table;
	int reg_table_size;
	int (*reduce_wp)(int val);
	unsigned int usb_suspend_gpio;
};


#else
struct pcf50623_data;
extern struct pcf50623_data *pcf50623_global;
#define pcf506XX_global pcf50623_global

extern int pcf506XX_voltage_set(struct pcf50623_data *pcf,
                                enum pcf506XX_regulator_id reg,
                                unsigned int millivolts);
extern unsigned int pcf506XX_voltage_get(struct pcf50623_data *pcf,
                                         enum pcf506XX_regulator_id reg);
extern int pcf506XX_onoff_get(struct pcf50623_data *pcf,
                              enum pcf506XX_regulator_id reg);
extern int pcf506XX_onoff_set(struct pcf50623_data *pcf,
                              enum pcf506XX_regulator_id reg, enum pcf506XX_regulator_mode on);

enum pcf50623_reg_gpiopwn {
PCF50623_REGU_PWEN1 =0x1,
PCF50623_REGU_PWEN2 =0x2,
PCF50623_REGU_PWEN3 =0x4,
PCF50623_REGU_PWEN4 =0x8,
PCF50623_REGU_GPIO1 =0x10,
PCF50623_REGU_GPIO2 =0x20,
PCF50623_REGU_GPIO3 =0x40,
PCF50623_REGU_GPIO4 =0x80
};
#define PCF50623_REGU_GPIO_MASK 0xf0
#define PCF50623_REGU_PWEN_MASK 0xf

struct pcf50623_platform_data {
	/* general */
	unsigned int used_features;
	unsigned int onkey_seconds_required;
	unsigned int onkey_seconds_poweroff;
#if defined (ACER_L1_CHANGED)
	int hs_irq;
#endif

/* ACER Erace.Ma@20100209, add headset power amplifier in K2*/
#if (defined ACER_L1_K2) || (defined ACER_L1_AS1)
    int hs_amp;
#endif
/* End Erace.Ma@20100209*/

	/* voltage regulator related */
	struct pmu_voltage_rail rails[__NUM_PCF506XX_REGULATORS];
	struct pmu_reg_table *reg_table_ES1;
	int reg_table_size_ES1;
	struct pmu_reg_table *reg_table_ES2;
	int reg_table_size_ES2;
	unsigned int usb_suspend_gpio;
};

#endif


/* FIXME: sharded with pcf50606 */
#define PMU_VRAIL_F_SUSPEND_ON	0x00000001	/* Remains on during suspend */
#define PMU_VRAIL_F_UNUSED	0x00000002	/* This rail is not used */


struct pmu_reg_table {
unsigned char addr;
unsigned char *value;
int size;
};

#define PCF506XX_FEAT_EXTON	0x00000001	/* not yet supported */
#define PCF506XX_FEAT_CBC	0x00000002
#define PCF506XX_FEAT_BBC	0x00000004	/* not yet supported */
#define PCF506XX_FEAT_RTC	0x00000040
#define PCF506XX_FEAT_CHGCUR	0x00000100
#define PCF506XX_FEAT_BATVOLT	0x00000200
#define PCF506XX_FEAT_BATTEMP	0x00000400
#define PCF506XX_FEAT_KEYPAD_BL	0x00000800
/* ACER Jen chang, 2009/06/24, IssueKeys:AU2.B-38, Implement vibrator device driver for HAL interface function { */
#define PCF506XX_FEAT_VIBRATOR	0x00001000
/* } ACER Jen Chang, 2009/06/24*/
/* ACER Jen chang, 2009/07/29, IssueKeys:AU4.B-137, Creat device for controlling charger indicated led { */
#define PCF506XX_FEAT_CHG_BL	0x00002000
/* } ACER Jen Chang, 2009/07/29 */


/* Ioctl interface */
enum {
    PCF506XX_IOCTL_READ_REG,
    PCF506XX_IOCTL_WRITE_REG,
	PCF506XX_IOCTL_BOOTCAUSE,
    //Selwyn 20090908 modified
    PCF50623_IOCTL_HS_PLUG,
    //~Selwyn modified
	PCF506XX_IOCTL_SET_GPIO_DIR,
	PCF506XX_IOCTL_SET_GPIO_MODE,
	PCF506XX_IOCTL_READ_GPIO,
	PCF506XX_IOCTL_WRITE_GPIO,
    PCF506XX_IOCTL_MAX,
};
/* ioctl BOOTCAUSE return a 32 bits whith following enum */
enum pcf506XX_bootcause {
	PCF506XX_ONKEY	= 0x01,	/*ONKEY */
	PCF506XX_ALARM	= 0x02,	/* ALARM*/
	PCF506XX_USB	= 0x04,	/* USB present */
	PCF506XX_USB_GLITCH	= 0x08,	/* USB GLITCH (USB  has been plug/unplugged once) USB is not presetn*/
	PCF506XX_USB_GLITCHES	= 0x010,/*  USB GLITCH (USB  has been plugged at least twice) 
										it can present or not according to PCF506323_USB value*/	
	PCF506XX_MCH = 0x20, /* Main chg present*/
	PCF506XX_MCH_GLITCH = 0x40 ,/* main chg glitch (Main chgt has been unplugged once) Main chgt is not present*/
	PCF506XX_MCH_GLITCHES = 0x80 /* main chg glitch (Main chgt  has been plugged at least twice) 
										 it can be present or not according to PCF50623_MAIN_CHG value*/
};

enum pcf50623_gpiomode {
	PCF50623_FIXED_O,
	PCF50623_LED1,
	PCF50623_LED2,
	PCF50623_PWM1,
	PCF50623_PWM2,
	PCF50623_LOWBAT_CMP,
	PCF50623_CHARGER_STATE,
	PCF50623_HIGH_Z,
};

struct ioctl_write_reg {
    u_int8_t address;
    u_int8_t value;
};

struct pmu_ioctl_gpio {
    int gpio;
	int dir;
    int state;
};

/* Battery Interrupt interface */
enum {
	PCF506XX_LOWBAT    = 0x0001, /* BVM detected low battery voltage */
	PCF506XX_HIGHTMP   = 0x0002, /* THS detected high temperature condition */
	PCF506XX_VMAX      = 0x0004, /* Charger switwhes from constant current to constant voltage */
	PCF506XX_CHGWD     = 0x0008, /* Charger watchdog timer will expire in 10s */
	PCF506XX_CHGRES    = 0x0010, /* battery voltage < autores voltage threshold */
	PCF506XX_THLIMON   = 0x0020, /* charger temperature limiting activated */
	PCF506XX_THLIMOFF  = 0x0040, /* charger temperature limiting deactivated */
	PCF506XX_BATFUL    = 0x0080, /* fully charged battery */
	PCF506XX_MCHINS    = 0x0100, /* Main charger connected */
	PCF506XX_MCHRM     = 0x0200, /* Main charger removed */
	PCF506XX_UCHGINS   = 0x0400, /* USB charger connected */
	PCF506XX_UCHGRM    = 0x0800, /* USB charger removed */
#if defined (CONFIG_SENSORS_PCF50616)
	PCF506XX_OVPCHGOFF = 0x1000, /* CBC in over-voltage protection */
	PCF506XX_OVPCHGON  = 0x2000, /* CBC no longer in over-voltage protection */
#else
	PCF506XX_CBCOVPFLT = 0x1000, /* CBC in over-voltage protection */
	PCF506XX_CBCOVPOK  = 0x2000, /* CBC no longer in over-voltage protection */
#endif
/// +ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
	PCF506XX_OVPFAULT  = 0x4000,
/// -ACER_AUx PenhoYu, enable PMU GPIO4 for detect OVP_FAULT of RT9718
};

/* Vmax configuration */
enum {
	PCF506XX_VMAX_4V00 = 0x08,
	PCF506XX_VMAX_4V02 = 0x10,
	PCF506XX_VMAX_4V04 = 0x18,
	PCF506XX_VMAX_4V06 = 0x20,
	PCF506XX_XVMAX_4V10 = 0x30,
	PCF506XX_VMAX_4V12 = 0x38,
	PCF506XX_VMAX_4V14 = 0x40,
	PCF506XX_VMAX_4V16 = 0x48,
	PCF506XX_VMAX_4V18 = 0x50,
	PCF506XX_VMAX_4V20 = 0x58,
	PCF506XX_VMAX_4V22 = 0x60,
	PCF506XX_VMAX_4V24 = 0x68,
	PCF506XX_VMAX_4V26 = 0x70,
	PCF506XX_VMAX_4V28 = 0x78,
	PCF506XX_VMAX_4V30 = 0x80,
#if defined CONFIG_SENSORS_PCF50616
	PCF506XX_VMAX_4V65 = 0x81, /* NiMH only */
#endif
};


typedef void (* itcallback)(u_int16_t);
extern void pcf506XX_charge_autofast(int);
extern void pcf506XX_setMainChgCur(u_int8_t);
extern void pcf506XX_setUsbChgCur(u_int8_t);
extern void pcf506XX_setTrickleChgCur(u_int8_t);
extern u_int8_t pcf506XX_getIlimit(void);
extern u_int8_t pcf506XX_CVmodeReached(void);
extern void pcf506XX_disableBattfullInt(void);
extern void pcf506XX_enableBattfullInt(void);
extern void pcf506XX_usb_charge_enable(int);
extern void pcf506XX_main_charge_enable(int);
extern void pcf506XX_setAutoStop(u_int8_t);
extern void pcf506XX_setVmax(u_int8_t);
extern void pcf506XX_registerChgFct(itcallback);
extern void pcf506XX_unregisterChgFct(void);
extern void pcf506XX_registerUsbFct(itcallback);
extern void pcf506XX_unregisterUsbFct(void);
extern void pcf506XX_registerOnkeyFct(itcallback);
extern void pcf506XX_unregisterOnkeyFct(void);
extern u_int8_t pcf506XX_mainChgPlugged(void);
extern u_int8_t pcf506XX_usbChgPlugged(void);
extern void pcf506XX_SetUsbSuspend(int val);

#endif /* _PCF50623_H */
