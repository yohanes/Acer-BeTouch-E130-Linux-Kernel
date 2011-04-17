/**
 *
 * radio-tea599x.c: Implements TEA599x FM ST-Ericsson Chips Interface for 
 * ST-Ericsson PNX platform
 *
 * I2C is used to communicate with chip.
 * Driver is registered to V4L for user access.
 *
 * Copyright (c) ST-Ericsson 2010
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#define MODULE_NAME "radio-tea599x"
#define RADIO_TEA599X_C

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/pcf506XX.h>
#ifdef CONFIG_RADIO_TEA599X_NEW_API
#include <media/radio-tea599x.h>
#endif

/* ****************************** VERSIONS ***************************************** */

#ifndef CONFIG_RADIO_TEA599X_NEW_API
#define TEA599X_RADIO_VERSION KERNEL_VERSION(0,1,0)
#else
#define TEA599X_RADIO_VERSION KERNEL_VERSION(0,2,0)
#endif

/* ################################################################################# */
/*                                                                                   */
/* CUSTOMIZATION                                                                     */
/*                                                                                   */
/* ################################################################################# */

/* I2C address */
#define TEA599X_SLAVE_ADDR      0x22

/* Noise & RSSI */
#define TEA599X_SEARCH_STOP_LEVEL               0x0100//0x0D00//0x0035 /* RSSI SSL */
#define TEA599X_STOP_NOISE_CONTINUOUS           0x0035 /* Peak noise level allowed */
#define TEA599X_STOP_NOISE_FINAL                0x0030 /* Max avrg noise lev allowed */

#define TEA599X_SEARCH_SCAN_PER_CH_GRID         120  /* 120ms max */

/* Scan */
#define TEA599X_SCAN_MAX_CHANNELS             32 /* max no of channels to scan for */
#define TEA599X_TIMER_SCAN_TIME               200 /* 200ms scan polling period */
#define TEA599X_MAX_SCAN_TIME_E_US            TEA599X_SEARCH_SCAN_PER_CH_GRID * 204 /* Europe band 100KHz grid max 25s */
#define TEA599X_MAX_SCAN_TIME_CHINA           TEA599X_SEARCH_SCAN_PER_CH_GRID * 380 /* China band 100KHz grid max 44s */

/* search */
#define TEA599X_TIMER_SEARCH_TIME             200  /* 200ms search polling period */
#define TEA599X_MAX_SEARCH_TIME               TEA599X_SEARCH_SCAN_PER_CH_GRID * 204 /* Europe band 100KHz grid max 25s */

/* Timers configuration */
#define TEA599X_POLLING_STEP_PERIOD             20  /* step on grid polling period */
#define TEA599X_MAX_STEP_TIME                   200 /* 200ms polling time */

#define TEA599X_POLLING_PRESET_PERIOD           20  /* 20ms preset polling period */
#define TEA599X_MAX_PRESET_TIME                 200 /* 200ms polling time */

#define TEA599X_POLLING_GOTO_FM_PERIOD          15  /* goto fm mode polling period */
#define TEA599X_MAX_GOTO_FM_TIME                100 /* 100ms polling time */

#define TEA599X_POLLING_MUTE_PERIOD             8   /* 8ms mute polling period */
#define TEA599X_MAX_MUTE_TIME                   40  /* 40ms polling time */

#define TEA599X_POLLING_AF_RSSI_UPDATE_PERIOD   1   /* 1ms AF RSSI update polling period */
#define TEA599X_MAX_AF_RSSI_UPDATE_TIME         20  /* 20ms polling time */

#define TEA599X_POLLING_AF_RSSI_SWITCH_PERIOD   100  /* 100 ms AF switch polling period */
#define TEA599X_POLLING_AF_RSSI_SWITCH_TIME     2000 /* 2s polling time */


/* Frequency ranges/bands */
#define TEA599X_US_EU_BAND_FREQ_LOW             87500000
#define TEA599X_US_EU_BAND_FREQ_HI              108000000

#define TEA599X_JAPAN_BAND_FREQ_LOW             76000000
#define TEA599X_JAPAN_BAND_FREQ_HI              90000000

#define TEA599X_CHINA_BAND_FREQ_LOW             70000000
#define TEA599X_CHINA_BAND_FREQ_HI              108000000
 
#define TEA599X_CUSTOM_BAND_FREQ_LOW            76000000
#define TEA599X_CUSTOM_BAND_FREQ_HI             108000000

#define TEA599X_BAND_CHANNEL_LOW                120     /* 76Mhz needed => (76M - 70M)/50K = 120 */
#define TEA599X_BAND_CHANNEL_HI                 760     /* 108Mhz needed => (108M - 70M)/50K = 760 */

#define TEA599X_DRIVER_TIMEOUT                  10000   /* Timeout on fops call to have access to driver: 10 s */

//#define CONFIG_POLL_TIMERS_IN_KERNEL

/* ################################################################################# */
/*                                                                                   */
/* DEBUG                                                                             */
/*                                                                                   */
/* ################################################################################# */

//KERN_WARNING

#if 0
#define DBG(format, arg...) printk(KERN_ALERT "%s - " format "\n", __FUNCTION__, ## arg)
#else
#define DBG(format, arg...)
#endif

#if 1
#define DBG_ERR(format, arg...) printk(KERN_ALERT "%s - " format "\n", __FUNCTION__, ## arg)
//#define DBG_ERR(format, arg...) printk(KERN_ERR "%s - " format "\n", __FUNCTION__, ## arg)
//#define DBG(format, arg...) printk(KERN_ALERT "%s - " format "\n", __FUNCTION__, ## arg)
//#define DBG(format, arg...) printk(KERN_ERR "%s - " format "\n", __FUNCTION__, ## arg)
#else
#define DBG_ERR(format, arg...)
#endif

#define DBG_FUNC_ENTRY	DBG(" ")


/* ################################################################################# */
/*                                                                                   */
/* LOCALS                                                                            */
/*                                                                                   */
/* ################################################################################# */

/* ****************************** CONSTANTS **************************************** */

#define TEA599X_EVENT_NO_EVENT                      1
#define TEA599X_EVENT_SEARCH_CHANNEL_FOUND          2
#define TEA599X_EVENT_SCAN_CHANNELS_FOUND           3
#define TEA599X_EVENT_SEARCH_NO_CHANNEL_FOUND       4
#define TEA599X_EVENT_SCAN_NO_CHANNEL_FOUND         5

#define TEA599X_FMR_SEEK_NONE                       0
#define TEA599X_FMR_SEEK_IN_PROGRESS                1
#define TEA599X_FMR_SCAN_IN_PROGRESS                2
#define TEA599X_FMR_SCAN_NONE                       3

#define TEA599X_FMR_SWITCH_OFF                      0
#define TEA599X_FMR_SWITCH_ON                       1
#define TEA599X_FMR_STANDBY                         2

#define TEA599X_HRTZ_MULTIPLIER                 1000000 
//TODO: check why not correlated with BAND
#define TEA599X_FREQ_LOW                        87.5 * TEA599X_HRTZ_MULTIPLIER 
#define TEA599X_FREQ_HIGH                       108 * TEA599X_HRTZ_MULTIPLIER  


/* ****************************** STRUCTURES *************************************** */

struct tea599x_FmrDevice
{
  unsigned char           v_state;
  int                     v_Volume;
  unsigned long           v_Frequency;
  unsigned char           v_Muted;
  unsigned char           v_SeekStatus;
  unsigned char           v_ScanStatus;
  unsigned long           v_AudioPath;
  unsigned char           v_band;
  unsigned char           v_grid;
  unsigned char           v_mode;
  unsigned char           v_dac;
  unsigned int            v_RssiThreshold;
  unsigned char           v_rdsState;
};

/* ****************************** PROTOTYPES *************************************** */

static void tea599x_stnConvertEndianness(unsigned short *buffer, unsigned long size);
static signed long tea599x_sleep(unsigned int msdelay);


/* ****************************** DATA ********************************************* */

unsigned short const tea599x_volume_table[21] ={
            /* Level 
            (+/- 0.1dB) */
    0x20,   /* -60 */
    0x2E,   /* -57 */
    0x41,   /* -54 */
    0x5C,   /* -51 */
    0x82,   /* -48 */
    0xB8,   /* -45 */
    0x104,  /* -42 */
    0x16F,  /* -39 */
    0x207,  /* -36 */
    0x2DD,  /* -33 */
    0x40C,  /* -30 */
    0x5B7,  /* -27 */
    0x813,  /* -24 */
    0xB68,  /* -21 */
    0x101D, /* -18 */
    0x16C2, /* -15 */
    0x2026, /* -12 */
    0x2D6A, /* -9 */
    0x4026, /* -6 */
    0x5A9D, /* -3 */
    0x7FFF, /* 0 */
};

#ifdef CONFIG_RADIO_TEA599X_NEW_API
unsigned short const tea599x_balance_table[21] ={
    0x8021,	/* Right off */
    0x8042,	/*  */
    0x8083,	/*  */
    0x8105,	/*  */
    0x8208,	/*  */
    0x840D,	/*  */
    0x8814,	/*  */
    0x901E,	/*  */
    0xA027,	/*  */
    0xC027,	/*  */
    0x0000,	/* center, index 10 */
    0x3FD8,	/*  */
    0x5FD8,	/*  */
    0x6FE1,	/*  */
    0x77EB,	/*  */
    0x7BF2,	/*  */
    0x7DF7,	/*  */
    0x7EFA,	/*  */
    0x7F7C,	/*  */
    0x7FBD,	/*  */
    0x7FDE,	/* Left off */
};
#endif

static struct tea599x_FmrDevice tea599x_FmrData;

static int tea599x_users = 0;
static unsigned char tea599x_globalEvent;

static unsigned long tea599x_searchFreq = 0;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
static unsigned long tea599x_search_timeout;
#endif

static unsigned short tea599x_noOfScanFreq = 0;
static unsigned long tea599x_scanFreqRssiLevel[TEA599X_SCAN_MAX_CHANNELS];
static unsigned long tea599x_scanFreq[TEA599X_SCAN_MAX_CHANNELS];
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
static unsigned long tea599x_scan_timeout;
#endif

/* Manage driver access */
static spinlock_t tea599x_lock = SPIN_LOCK_UNLOCKED;
static unsigned char tea599x_busy = false;
DECLARE_WAIT_QUEUE_HEAD(tea599x_queue);

/* ****************************** FUNCTIONS **************************************** */

//Freq in Hz to V4l2 freq (units of 62.5Hz)
#define HZ_2_V4L2(X)  (2*(X)/125)
//V4l2 freq (units of 62.5Hz) to Freq in Hz
#define V4L2_2_HZ(X)  (((X)*125)/(2))

static int tea599x_lock_driver(void)
{
    int status = 0;
    unsigned char avail = false;
    do
    {
        spin_lock(&tea599x_lock);
        if (tea599x_busy == false)
            tea599x_busy = true;
        avail = tea599x_busy;
        spin_unlock(&tea599x_lock);
        if (avail == false)
            status = wait_event_interruptible_timeout(tea599x_queue,
                                                      (tea599x_busy == false),
                                                      msecs_to_jiffies(TEA599X_DRIVER_TIMEOUT));
    } while ((!status) && (avail == false));
    if (status)
        DBG("Failed[%d]", status);
    return status;
}

static void tea599x_unlock_driver(void)
{
    tea599x_busy = false;
    wake_up(&tea599x_queue);
}

static void tea599x_stnConvertEndianness(unsigned short *buffer, unsigned long size) 
{
    unsigned short *tmp = buffer;

    while(size--) {
        *tmp = *tmp << 8 | *tmp >> 8;
        tmp ++;
    }
}

static signed long tea599x_sleep(unsigned int msdelay)
{
    set_current_state(TASK_INTERRUPTIBLE);
    return schedule_timeout(msecs_to_jiffies(msdelay));
}


/* ################################################################################# */
/*                                                                                   */
/* I2C I/F                                                                           */
/*                                                                                   */
/* ################################################################################# */

/* ****************************** STRUCTURES *************************************** */

/* ****************************** PROTOTYPES *************************************** */

static int tea599x_i2c_write(unsigned short add_to_wr, void * buffer, unsigned int len);
static int tea599x_i2c_read(unsigned short add_to_rd, void * buffer, unsigned int len);
static int __devinit tea599x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit tea599x_i2c_remove(struct i2c_client *client);
static int tea599x_i2c_init(void);
static void tea599x_i2c_close(void);


/* ****************************** DATA ********************************************* */

static struct i2c_client *tea599x_i2c_client = NULL;

static struct i2c_device_id tea599x_id_table[] = {
    {"tea599x", 0},
    {/* end of list */}
};

static struct i2c_driver tea599x_i2c_driver = {
    .driver = {
        .name   = "tea599x",
    },
    .id_table = tea599x_id_table,
    .probe = tea599x_i2c_probe,
    .remove = tea599x_i2c_remove,
};


/* ****************************** FUNCTIONS **************************************** */

static int tea599x_i2c_write(unsigned short add_to_wr, void * buffer, unsigned int len)
{
    int err = 0;
    unsigned char *tmpWriteBuf;
    DBG_FUNC_ENTRY;

    /* Send Higher Byte First */
    tea599x_stnConvertEndianness(buffer, len/2);

    if (tea599x_i2c_client)
    {
        struct i2c_msg msgs[1] = {
            { tea599x_i2c_client->addr, 0, len+1, (void *)NULL },
        };

        tmpWriteBuf = kmalloc(len+1, GFP_KERNEL);
        memcpy(&tmpWriteBuf[1], buffer, len);
        tmpWriteBuf[0] = add_to_wr;

        msgs[0].buf = (void *)tmpWriteBuf;

        err = i2c_transfer(tea599x_i2c_client->adapter, msgs, 1);
        if (err != 1)
        {
            DBG_ERR("Failed[%d]", err);
            err = -EIO;
        }
        else
            err = 0;
        kfree(tmpWriteBuf);
    }
    else
    {
        DBG_ERR("Driver Not initialized\n");
        err = -EPERM;
    }
    return err;
}

static int tea599x_i2c_read(unsigned short add_to_rd, void * buffer, unsigned int len)
{
    unsigned char tmpWriteArr[1];
    unsigned char read_buf[1 + 9u * sizeof(unsigned short)];/* dev addr, 1 addr, 8 data words */
    int err = 0;
    DBG_FUNC_ENTRY;

    if (tea599x_i2c_client)
    {
        struct i2c_msg msgs[2] = {
            { tea599x_i2c_client->addr, 0, 1, (void *)tmpWriteArr},
            { tea599x_i2c_client->addr, I2C_M_RD, len, (void *)read_buf},
        };

        tmpWriteArr[0] = add_to_rd;
        read_buf[0] = TEA599X_SLAVE_ADDR | 0x01;
        err = i2c_transfer(tea599x_i2c_client->adapter, msgs, 2);
        if (err != 2)
        {
            DBG_ERR("Failed[%d]", err);
            return -EIO;
        }
        else
            err = 0;

        memcpy(buffer, &read_buf[0], len); 
        /* Receive Lower Byte First */
        tea599x_stnConvertEndianness(buffer, len/2);
    }
    else
    {
        DBG_ERR("Driver Not initialized");
        err = -EPERM;
    }
    return err;
}

static int __devinit tea599x_i2c_probe(struct i2c_client *client,
                                        const struct i2c_device_id *id)
{
    DBG_FUNC_ENTRY;
    tea599x_i2c_client = client;
    i2c_set_clientdata(client, NULL);
    return 0;
}

static int __devexit tea599x_i2c_remove(struct i2c_client *client)
{
    DBG_FUNC_ENTRY;
    tea599x_i2c_client = NULL;
    return 0;
}

static int tea599x_i2c_init(void)
{
    int result = 0;
    DBG_FUNC_ENTRY;
    result = i2c_add_driver(&tea599x_i2c_driver);
    DBG("Result[%d]", result);
    return result;
}

static void tea599x_i2c_close(void)
{
    DBG_FUNC_ENTRY;
    i2c_del_driver(&tea599x_i2c_driver);
}


/* ################################################################################# */
/*                                                                                   */
/* FM APIs                                                                           */
/*                                                                                   */
/* ################################################################################# */

/* ****************************** CONSTANTS **************************************** */

#define TEA599X_GEN_POWERUP_SAFETY_WORD  (0x375Du)

/* TEA599X registers */
#define TEA599X_REG_CMD_REG          0x00u
#define TEA599X_REG_RSP_REG          0x08u
#define TEA599X_REG_IRPT_SRC_REG     0x10u

/* Interrupt bit masks for interrupt register */
#define TEA599X_IRPT_OPERATIONSUCCEEDED      0x0001        /* (1u << 0) */
#define TEA599X_IRPT_OPERATIONFAILED         0x0002        /* (1u << 1) */
#define TEA599X_IRPT_COLDBOOTREADY           0x8000        /* (1u << 15) */

/************************************************************************/
/* Command/Reply identifiers                                            */
/************************************************************************/
/* Generic commands */
#define TEA599X_CMD_GEN_GOTOMODE                             (0x0041u << 3)
#define TEA599X_CMD_GEN_GOTOSTANDBY                          (0x0061u << 3)
#define TEA599X_CMD_GEN_GOTOPOWERDOWN                        (0x0081u << 3)
#define TEA599X_CMD_GEN_GETVERSION                           (0x00C1u << 3)
#define TEA599X_CMD_GEN_POWERUP                              (0x0141u << 3)
/* FMR */
#define TEA599X_CMD_FMR_TN_SETBAND                           (0x0023u << 3)
#define TEA599X_CMD_FMR_TN_SETGRID                           (0x0043u << 3)
#define TEA599X_CMD_FMR_RP_GETRSSI                           (0x0083u << 3)
#define TEA599X_CMD_FMR_RP_STEREO_SETMODE                    (0x0123u << 3)
#define TEA599X_CMD_FMR_SP_STOP                              (0x0383u << 3)
#define TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL                   (0x03C3u << 3)
#define TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START             (0x03E3u << 3)
#define TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START               (0x0403u << 3)
#define TEA599X_CMD_FMR_SP_SCAN_GETRESULT                    (0x0423u << 3)
#define TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL                  (0x04C3u << 3)
#define TEA599X_CMD_FMR_GETSTATE                             (0x04E3u << 3)
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUP                   (0x0303u << 3)
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT              (0x0323u << 3)
#define TEA599X_CMD_FMR_DP_SETCONTROL                        (0x02A3u << 3)
#define TEA599X_CMD_FMR_SP_AFUPDATE_START                    (0x0463u << 3)
#define TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT                (0x0483u << 3)
#define TEA599X_CMD_FMR_SP_AFSWITCH_START                    (0x04A3u << 3)
#define TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT                (0x0603u << 3)
/* AUP */
#define TEA599X_CMD_AUP_SETVOLUME                            (0x0022u << 3)
#define TEA599X_CMD_AUP_SETBALANCE                           (0x0042u << 3)
#define TEA599X_CMD_AUP_SETMUTE                              (0x0062u << 3)
#define TEA599X_CMD_AUP_SETAUDIODAC                          (0x0082u << 3)

/************************************************************************/
/* Commands                                                             */
/************************************************************************/
/* GEN */
/* GEN mode, config, etc */
#define TEA599X_CMD_GEN_GOTOMODE_CMD                 (TEA599X_CMD_GEN_GOTOMODE                            | 0x01u)
#define TEA599X_CMD_GEN_GETVERSION_CMD               (TEA599X_CMD_GEN_GETVERSION                          | 0x00u)
/* GEN power */
#define TEA599X_CMD_GEN_GOTOSTANDBY_CMD              (TEA599X_CMD_GEN_GOTOSTANDBY                         | 0x00u)
#define TEA599X_CMD_GEN_GOTOPOWERDOWN_CMD            (TEA599X_CMD_GEN_GOTOPOWERDOWN                       | 0x00u)
#define TEA599X_CMD_GEN_POWERUP_CMD                  (TEA599X_CMD_GEN_POWERUP                             | 0x01u)
/* AUP commands */
#define TEA599X_CMD_AUP_SETVOLUME_CMD                (TEA599X_CMD_AUP_SETVOLUME                           | 0x01u)
#define TEA599X_CMD_AUP_SETBALANCE_CMD               (TEA599X_CMD_AUP_SETBALANCE                          | 0x01u)
#define TEA599X_CMD_AUP_SETMUTE_CMD                  (TEA599X_CMD_AUP_SETMUTE                             | 0x02u)
#define TEA599X_CMD_AUP_SETAUDIODAC_CMD              (TEA599X_CMD_AUP_SETAUDIODAC                         | 0x01u)
/* FMR */
/* FMR TN */
#define TEA599X_CMD_FMR_TN_SETBAND_CMD               (TEA599X_CMD_FMR_TN_SETBAND                          | 0x03u)
#define TEA599X_CMD_FMR_TN_SETGRID_CMD               (TEA599X_CMD_FMR_TN_SETGRID                          | 0x01u)
/* FMR RP */
#define TEA599X_CMD_FMR_RP_GETRSSI_CMD               (TEA599X_CMD_FMR_RP_GETRSSI                          | 0x00u)
#define TEA599X_CMD_FMR_RP_STEREO_SETMODE_CMD        (TEA599X_CMD_FMR_RP_STEREO_SETMODE                   | 0x01u)
/* FMR SP */
#define TEA599X_CMD_FMR_SP_STOP_CMD                  (TEA599X_CMD_FMR_SP_STOP                             | 0x00u)
#define TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL_CMD       (TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL                  | 0x01u)
#define TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START_CMD (TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START            | 0x04u)
#define TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START_CMD   (TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START              | 0x04u)
#define TEA599X_CMD_FMR_SP_SCAN_GETRESULT_CMD        (TEA599X_CMD_FMR_SP_SCAN_GETRESULT                   | 0x01u)
#define TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL_CMD      (TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL                 | 0x01u)
#define TEA599X_CMD_FMR_SP_AFUPDATE_START_CMD        (TEA599X_CMD_FMR_SP_AFUPDATE_START                   | 0x01u)
#define TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT_CMD    (TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT               | 0x00u)
#define TEA599X_CMD_FMR_SP_AFSWITCH_START_CMD        (TEA599X_CMD_FMR_SP_AFSWITCH_START                   | 0x05u)
#define TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT_CMD    (TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT               | 0x00u)
/* FMR DP */
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUP_CMD       (TEA599X_CMD_FMR_DP_BUFFER_GETGROUP                  | 0x00u)
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT_CMD  (TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT             | 0x00u)
#define TEA599X_CMD_FMR_DP_SETCONTROL_CMD            (TEA599X_CMD_FMR_DP_SETCONTROL                       | 0x01u)
/* FMR other */
#define TEA599X_CMD_FMR_GETSTATE_CMD                 (TEA599X_CMD_FMR_GETSTATE                            | 0x00u)

/************************************************************************/
/* Replies                                                              */
/************************************************************************/
/* GEN */
/* GEN mode, config, etc */
#define TEA599X_CMD_GEN_GOTOMODE_RSP                 (TEA599X_CMD_GEN_GOTOMODE                            | 0x00u)
#define TEA599X_CMD_GEN_GETVERSION_RSP               (TEA599X_CMD_GEN_GETVERSION                          | 0x06u)
/* AUP */
#define TEA599X_CMD_AUP_SETVOLUME_RSP                (TEA599X_CMD_AUP_SETVOLUME                           | 0x00u)
#define TEA599X_CMD_AUP_SETBALANCE_RSP               (TEA599X_CMD_AUP_SETBALANCE                          | 0x00u)
#define TEA599X_CMD_AUP_SETMUTE_RSP                  (TEA599X_CMD_AUP_SETMUTE                             | 0x00u)
#define TEA599X_CMD_AUP_SETAUDIODAC_RSP              (TEA599X_CMD_AUP_SETAUDIODAC                         | 0x00u)
/* FMR */
/* FMR TN */
#define TEA599X_CMD_FMR_TN_SETBAND_RSP               (TEA599X_CMD_FMR_TN_SETBAND                          | 0x00u)
#define TEA599X_CMD_FMR_TN_SETGRID_RSP               (TEA599X_CMD_FMR_TN_SETGRID                          | 0x00u)
/* FMR RP */
#define TEA599X_CMD_FMR_RP_GETRSSI_RSP               (TEA599X_CMD_FMR_RP_GETRSSI                          | 0x01u)
#define TEA599X_CMD_FMR_RP_STEREO_SETMODE_RSP        (TEA599X_CMD_FMR_RP_STEREO_SETMODE                   | 0x00u)
/* FMR SP */
#define TEA599X_CMD_FMR_SP_STOP_RSP                  (TEA599X_CMD_FMR_SP_STOP                             | 0x00u)
#define TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL_RSP       (TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL                  | 0x00u)
#define TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START_RSP (TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START            | 0x00u)
#define TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START_RSP   (TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START              | 0x00u)
#define TEA599X_CMD_FMR_SP_SCAN_GETRESULT_RSP        (TEA599X_CMD_FMR_SP_SCAN_GETRESULT                   | 0x07u)
#define TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL_RSP      (TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL                 | 0x00u)
#define TEA599X_CMD_FMR_SP_AFUPDATE_START_RSP        (TEA599X_CMD_FMR_SP_AFUPDATE_START                   | 0x00u)
#define TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT_RSP    (TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT               | 0x01u)
#define TEA599X_CMD_FMR_SP_AFSWITCH_START_RSP        (TEA599X_CMD_FMR_SP_AFSWITCH_START                   | 0x00u)
#define TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT_RSP    (TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT               | 0x03u)
/* FMR DP */
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUP_RSP       (TEA599X_CMD_FMR_DP_BUFFER_GETGROUP                  | 0x06u)
#define TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT_RSP  (TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT             | 0x01u)
#define TEA599X_CMD_FMR_DP_SETCONTROL_RSP            (TEA599X_CMD_FMR_DP_SETCONTROL                       | 0x00u)
/* FMR other */
#define TEA599X_CMD_FMR_GETSTATE_RSP                 (TEA599X_CMD_FMR_GETSTATE                            | 0x04u)

#ifdef CONFIG_RADIO_TEA599X_NEW_API
#define V4L2_RADIO_SEEK_NONE			0x00
#define V4L2_RADIO_SEEK_IN_PROGRESS		0x01
#define V4L2_RADIO_SCAN_NONE			0x02
#define V4L2_RADIO_SCAN_IN_PROGRESS		0x03
#endif

/* ****************************** STRUCTURES *************************************** */

typedef enum 
{
    TEA599X_POWER_DOWN_TO_POWER_ON,
    TEA599X_POWER_ON_TO_STAND_BY,
    TEA599X_STANDBY_TO_POWER_ON,
    TEA599X_TO_POWER_DOWN
}tea599x_powermode;

#ifndef CONFIG_RADIO_TEA599X_NEW_API
typedef enum
{
    TEA599X_FM_BAND_US_EU   = 0,
    TEA599X_FM_BAND_JAPAN   = 1,
    TEA599X_FM_BAND_CHINA   = 2,
    TEA599X_FM_BAND_CUSTOM  = 3
} tea599x_FM_band_enum;

typedef enum
{
    TEA599X_FM_GRID_50  = 0,
    TEA599X_FM_GRID_100 = 1,
    TEA599X_FM_GRID_200 = 2
} tea599x_FM_grid_enum;
#endif

typedef enum
{
    TEA599X_DIR_UP      = 0,
    TEA599X_DIR_DOWN    = 1
} tea599x_dir_enum;

#ifndef CONFIG_RADIO_TEA599X_NEW_API
typedef enum
{
    TEA599X_STEREO_MODE_STEREO     = 0,
    TEA599X_STEREO_MODE_FORCE_MONO = 1,
} tea599x_stereo_mode_enum;

typedef enum
{
    TEA599X_MUTE_STATE_OFF = 0,
    TEA599X_MUTE_STATE_ON  = 1
} tea599x_mute_state_enum;
#endif

typedef enum
{
    TEA599X_AUP_FADER_STATE_OFF = 0,
    TEA599X_AUP_FADER_STATE_ON  = 1
} tea599x_AUP_fader_state_enum;

typedef enum
{
    TEA599X_GEN_MODE_PDN     = -3,
    TEA599X_GEN_MODE_BOOT    = -1,
    TEA599X_GEN_MODE_IDLE    = 0,
    TEA599X_GEN_MODE_FMR     = 1,
} tea599x_mode_enum;

#ifndef CONFIG_RADIO_TEA599X_NEW_API
typedef enum
{
    TEA599X_DAC_DISABLE = 0,
    TEA599X_DAC_ENABLE = 1,
    TEA599X_DAC_RIGHT_MUTE = 2,
    TEA599X_DAC_LEFT_MUTE = 3
} tea599x_power_state;
#endif

typedef struct
{
    unsigned short manufacturer_id;
    unsigned short product_id;
    unsigned short hardware_version;
    unsigned short firmware_release;
    unsigned short firmware_version;
    unsigned short firmware_application;
} tea599x_version_struct;

#ifndef CONFIG_RADIO_TEA599X_NEW_API
typedef enum
{
    TEA599X_FMR_STEREO_STATE_NO  = 0,
    TEA599X_FMR_STEREO_STATE_YES = 1
} tea599x_FMR_stereo_state_enum;
#endif

typedef struct
{
    unsigned short    channel_number;
    unsigned short    rf_level;
} tea599x_FMR_channel_result_struct;

typedef struct
{
    tea599x_FMR_channel_result_struct    channel_info;
#ifndef CONFIG_RADIO_TEA599X_NEW_API
    tea599x_FMR_stereo_state_enum        stereo_state;
#else
    unsigned int                         stereo_state;
#endif
} tea599x_FMR_state_struct;

typedef enum
{
    TEA599X_FMR_RDS_STATE_OFF = 0,
    TEA599X_FMR_RDS_STATE_ON  = 1,
    TEA599X_FMR_RDS_STATE_ON_ENHANCED  = 2
} tea599x_FMR_rds_state_enum;

/* ****************************** PROTOTYPES *************************************** */

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SwitchOn(unsigned long freq, unsigned char band, unsigned char grid);
#else
static int tea599x_fm_SwitchOn( unsigned long freq,
                                unsigned char band,
                                unsigned char grid,
                                unsigned char vol,
                                unsigned char dac,
                                unsigned char bal,
                                unsigned char mute,
                                unsigned char mode);
#endif
static int tea599x_fm_SwitchOff(void);
static int tea599x_fm_SetPowerMode(tea599x_powermode setMode);
static int tea599x_fm_get_interrupt_register(unsigned short * pevent);
static int tea599x_fm_GoToFmMode(void);
static int tea599x_fm_SetBand(tea599x_FM_band_enum band);
static int tea599x_fm_SetGrid(tea599x_FM_grid_enum grid);
static int tea599x_fm_Stereo_SetMode(tea599x_stereo_mode_enum mode);
static int tea599x_fm_PresetFreq(unsigned long freq);
static int tea599x_fm_GetFrequency(unsigned long * freq);
static int tea599x_fm_FMR_GetState(tea599x_FMR_state_struct *state);
static int tea599x_fm_SetAudioDAC(tea599x_power_state power_state);
static int tea599x_fm_SetVolume(unsigned short vol);
static int tea599x_fm_clearInterrupts(void);
static int tea599x_fm_GotoMode(tea599x_mode_enum mode);
static int tea599x_fm_FMR_SP_Tune_SetChannel(unsigned short channel);
static int tea599x_fm_GetBandFreqHiLo(tea599x_FM_band_enum band, unsigned long * pFreqHi, unsigned long * pFreqLo);
static int tea599x_fm_GetVersion(tea599x_version_struct * pversion);
#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SetMute(tea599x_mute_state_enum mute_state);
#else
static int tea599x_fm_SetMute(unsigned int mute_state);
#endif
static int tea599x_fm_SetAudioBalance(unsigned short balance);
static int tea599x_fm_GetSignalStrength(unsigned short * rssi);
static int tea599x_fm_StepFreq(tea599x_dir_enum dir);
static int tea599x_fm_StartScan(void);
static int tea599x_fm_StopScan(void);
static void tea599x_fm_ScanWorkCb(struct work_struct *work);
static int tea599x_fm_ScanGetResults(void);
static int tea599x_fm_SearchFreq(tea599x_dir_enum dir);
static void tea599x_fm_SearchWorkCb(struct work_struct *work);
#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_StopSearch(void);
#endif
#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SwitchRdsState(tea599x_FMR_rds_state_enum state);
static int tea599x_fm_RdsBlocksGetCnt(unsigned int * count);
static int tea599x_fm_RdsBlocksGetData(struct v4l2_rds_block * block);
static unsigned int tea599x_fm_GetAltFreqRssi(unsigned long altFreq, unsigned short * rssi);
static int tea599x_fm_AfStartSwitch(unsigned long freq,
                                    unsigned int pi,
                                    unsigned int pimask,
                                    unsigned int wait_time,
                                    unsigned int rssi,
                                    unsigned short * conclusion,
                                    unsigned short * newRssi,
                                    unsigned short * newPi);
#endif

/* ****************************** DATA ********************************************* */

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
DECLARE_DELAYED_WORK(tea599x_fm_ScanWork, tea599x_fm_ScanWorkCb);
DECLARE_DELAYED_WORK(tea599x_fm_SearchWork, tea599x_fm_SearchWorkCb);
#endif

/* ****************************** FUNCTIONS **************************************** */

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SwitchOn(unsigned long freq, unsigned char band, unsigned char grid)
#else
static int tea599x_fm_SwitchOn( unsigned long freq,
                                unsigned char band,
                                unsigned char grid,
                                unsigned char vol,
                                unsigned char dac,
                                unsigned char bal,
                                unsigned char mute,
                                unsigned char mode)
#endif
{
    int result;
    unsigned short events = 0x0000;
    tea599x_version_struct version;
    DBG_FUNC_ENTRY;

    DBG("Freq[%d] - Band[%d] - Grid[%d]", (int)freq, band, grid);

    result = pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_D7REG,PCF506XX_REGU_ON);
    if (!result)
        tea599x_sleep(10);
    
    result = tea599x_fm_SetPowerMode(TEA599X_POWER_DOWN_TO_POWER_ON);

    if (!result)
        result = tea599x_fm_get_interrupt_register(&events);

    if (!result)
    {
        result = ((TEA599X_IRPT_COLDBOOTREADY & events) && (events != 0XFFFF))?0:(-EBUSY);
        if (result == -EBUSY)
        {
            DBG_ERR("Power on failed[%d]", events);
        }
    }

    if (!result)
        result = tea599x_fm_GoToFmMode();

    if (!result)
        result = tea599x_fm_SetBand(band);

    if (!result)
        result = tea599x_fm_SetGrid(grid);

#ifndef CONFIG_RADIO_TEA599X_NEW_API
    if (!result)
        result = tea599x_fm_Stereo_SetMode(TEA599X_STEREO_MODE_STEREO);
    if (!result)
        tea599x_FmrData.v_mode = TEA599X_STEREO_MODE_STEREO;
    else
        tea599x_FmrData.v_mode = TEA599X_STEREO_MODE_FORCE_MONO;
#else
    if (!result)
        result = tea599x_fm_Stereo_SetMode(mode);
#endif

    if (!result)
        result = tea599x_fm_PresetFreq(freq);

#ifndef CONFIG_RADIO_TEA599X_NEW_API
    if (!result)
        result = tea599x_fm_SetAudioDAC(TEA599X_DAC_ENABLE);
#else
    if (!result)
        result = tea599x_fm_SetAudioDAC(dac);
#endif

    if (!result)
        result = tea599x_fm_GetVersion(&version);

#ifndef CONFIG_RADIO_TEA599X_NEW_API
    if (!result)
        result = tea599x_fm_SetVolume(5);
    if (!result)
        tea599x_FmrData.v_Volume = 5;
    else
        tea599x_FmrData.v_Volume = 0;
#else
    if (!result)
        result = tea599x_fm_SetVolume(vol);
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
    if (!result)
        result = tea599x_fm_SetAudioBalance(bal);

    if (!result)
        result = tea599x_fm_SetMute(mute);
#endif

    return result;
}

static int tea599x_fm_SwitchOff(void)
{
    int result;

    DBG_FUNC_ENTRY;

    result = tea599x_fm_SetPowerMode(TEA599X_TO_POWER_DOWN);

    if (!result) {
        tea599x_sleep(10);
        result = pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_D7REG,PCF506XX_REGU_OFF);
    } else {
        pcf506XX_onoff_set(pcf506XX_global,PCF506XX_REGULATOR_D7REG,PCF506XX_REGU_OFF);
    }

    return result;
}

static int tea599x_fm_SetPowerMode(tea599x_powermode setMode)
{
    int result = 0;
    unsigned short buffer[2]; 
    DBG_FUNC_ENTRY;

    switch(setMode)
    {
        case TEA599X_POWER_DOWN_TO_POWER_ON :
        {
            buffer[0] = TEA599X_CMD_GEN_POWERUP_CMD;
            buffer[1] = TEA599X_GEN_POWERUP_SAFETY_WORD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buffer, 2*sizeof(unsigned short));
            if (!result)
                tea599x_sleep(100);
        }
        break;

        case TEA599X_STANDBY_TO_POWER_ON:
        {
            buffer[0] = TEA599X_CMD_GEN_POWERUP_CMD;
            buffer[1] = TEA599X_GEN_POWERUP_SAFETY_WORD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buffer, 2*sizeof(unsigned short));
            if (!result)
                tea599x_sleep(100);
        }
        break;

        case TEA599X_POWER_ON_TO_STAND_BY:
        {
            buffer[0] = TEA599X_CMD_GEN_GOTOSTANDBY_CMD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buffer, 1*sizeof(unsigned short));
            if (!result)
                tea599x_sleep(100);
        }
        break;

        case TEA599X_TO_POWER_DOWN:
        {
            buffer[0] = TEA599X_CMD_GEN_GOTOPOWERDOWN_CMD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buffer, 1*sizeof(unsigned short));
            if (!result)
                tea599x_sleep(100);
        }
        break;
   }
   return result;
}

static int tea599x_fm_get_interrupt_register(unsigned short * pevent)
{
    int result;
    DBG_FUNC_ENTRY;
    result = tea599x_i2c_read(TEA599X_REG_IRPT_SRC_REG, pevent, 1*sizeof(unsigned short));
    DBG("Result[%d] Events[0x%04X]", result, *pevent);
    return result;
}

static int tea599x_fm_GoToFmMode(void)
{
    int result;
    unsigned char timeout = (unsigned char)(TEA599X_MAX_GOTO_FM_TIME/TEA599X_POLLING_GOTO_FM_PERIOD);
    unsigned short interrupts = 0;
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
        result = tea599x_fm_GotoMode(TEA599X_GEN_MODE_FMR);

    while ((!result) && (--timeout) && (!(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED)))
    {
        tea599x_sleep(TEA599X_POLLING_GOTO_FM_PERIOD);
        result = tea599x_fm_get_interrupt_register(&interrupts);
    }
    if(timeout == 0)
    {
        DBG_ERR("Timeout");
        result = -ETIMEDOUT;
    }
    return result;
}

static int tea599x_fm_SetBand(tea599x_FM_band_enum band)
{
    int result = 0;
    unsigned short band_ch_low = 0;
    unsigned short band_ch_high = 0;
    unsigned short buff[4] = {0, 0, 0, 0};
    DBG_FUNC_ENTRY;

    switch(band)
    {
        case TEA599X_FM_BAND_US_EU:
        case TEA599X_FM_BAND_JAPAN:
        case TEA599X_FM_BAND_CHINA:
            break;
        case TEA599X_FM_BAND_CUSTOM:
            band_ch_low = TEA599X_BAND_CHANNEL_LOW;
            band_ch_high = TEA599X_BAND_CHANNEL_HI;
        break;
        default:
            DBG_ERR("Band[%d] not supported", band);
            result = -ERANGE;
        break;
    }

    if (!result)
    {
        buff[0] = TEA599X_CMD_FMR_TN_SETBAND_CMD;
        buff[1] = (unsigned short)band;
        buff[2] = band_ch_low;
        buff[3] = band_ch_high;
    
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 4*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_FMR_TN_SETBAND_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
    }
    }

    return result;
}

static int tea599x_fm_SetGrid(tea599x_FM_grid_enum grid)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_TN_SETGRID_CMD;
    buff[1] = (unsigned short)grid;

    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_TN_SETGRID_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_Stereo_SetMode(tea599x_stereo_mode_enum mode)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_RP_STEREO_SETMODE_CMD;
#ifndef CONFIG_RADIO_TEA599X_NEW_API
    buff[1] = (unsigned short)mode;
#else
    buff[1] = (unsigned short)((mode==TEA599X_STEREO_MODE_STEREO)?0:1);
#endif

    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_RP_STEREO_SETMODE_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_PresetFreq(unsigned long freq)
{
    int result;
    unsigned short channel_number = 0;
    unsigned short interrupts = 0;
    unsigned long FreqHi, FreqLo;
    unsigned char timeout = (unsigned char)(TEA599X_MAX_PRESET_TIME/TEA599X_POLLING_PRESET_PERIOD);
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
    {
        result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &FreqHi, &FreqLo);
        if (!result)
        {
            /* calculate channel from frequency */
            channel_number = (unsigned short)((freq - FreqLo)/50000);
            result = tea599x_fm_FMR_SP_Tune_SetChannel(channel_number);
        }
    }

    while ((!result) && (--timeout) && 
        (!(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED)) && (!(interrupts & TEA599X_IRPT_OPERATIONFAILED)))
    {
        tea599x_sleep(TEA599X_POLLING_PRESET_PERIOD);
        result = tea599x_fm_get_interrupt_register(&interrupts);
    }
    if(timeout == 0)
    {
        DBG_ERR("Time out");
        result = -ETIMEDOUT;
    }

    return result;
}

static int tea599x_fm_GetFrequency(unsigned long * freq)
{
    int result;
    tea599x_FMR_state_struct state;
    unsigned long FreqHi, FreqLo;
    DBG_FUNC_ENTRY;

    *freq = 0;
    result = tea599x_fm_FMR_GetState(&state);
    if (!result)
    {
        result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &FreqHi, &FreqLo);
        if (!result)
        {
            *freq = FreqLo + (state.channel_info.channel_number)*50000;
            DBG("Channel[%d] Freq[%09d]", state.channel_info.channel_number, (int)(*freq));
        }
    }

    return result;
}

static int tea599x_fm_FMR_GetState(tea599x_FMR_state_struct *state)
{
    int result;
    unsigned short buff[5] = {0, 0, 0, 0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_GETSTATE_CMD;

    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 5*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_GETSTATE_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    if (!result)
    {
        state->channel_info.channel_number  = buff[1];
        state->channel_info.rf_level        = buff[2];
#ifndef CONFIG_RADIO_TEA599X_NEW_API
        state->stereo_state     = (tea599x_FMR_stereo_state_enum)buff[3];
#else
        state->stereo_state                 = (buff[3]==0?TEA599X_STEREO_MODE_STEREO:TEA599X_STEREO_MODE_FORCE_MONO);
#endif
    }

    return result;
}

static int tea599x_fm_SetAudioDAC(tea599x_power_state power_state)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_AUP_SETAUDIODAC_CMD;
    buff[1] = (unsigned short)power_state;

    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_AUP_SETAUDIODAC_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_SetVolume(unsigned short vol)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_AUP_SETVOLUME_CMD;
    buff[1] = tea599x_volume_table[vol];

    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_AUP_SETVOLUME_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_clearInterrupts(void)
{
    int result;
    unsigned short dummy;
    DBG_FUNC_ENTRY;
    /* Clear interrupt register by reading it */
    result = tea599x_fm_get_interrupt_register(&dummy);
    return result;
}

static int tea599x_fm_GotoMode(tea599x_mode_enum mode)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_GEN_GOTOMODE_CMD;
    buff[1] = (unsigned short)mode;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_GEN_GOTOMODE_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_FMR_SP_Tune_SetChannel(unsigned short channel)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL_CMD;
    buff[1] = (unsigned short)channel;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_TUNE_SETCHANNEL_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_GetBandFreqHiLo(tea599x_FM_band_enum band, unsigned long * pFreqHi, unsigned long * pFreqLo)
{
    int result = 0;
    DBG_FUNC_ENTRY;

    switch(band)
    {
        case TEA599X_FM_BAND_US_EU:
            *pFreqLo = TEA599X_US_EU_BAND_FREQ_LOW;
            *pFreqHi = TEA599X_US_EU_BAND_FREQ_HI;
            break;
        case TEA599X_FM_BAND_JAPAN:
            *pFreqLo = TEA599X_JAPAN_BAND_FREQ_LOW;
            *pFreqHi = TEA599X_JAPAN_BAND_FREQ_HI;
            break;
        case TEA599X_FM_BAND_CHINA:
            *pFreqLo = TEA599X_CHINA_BAND_FREQ_LOW;
            *pFreqHi = TEA599X_CHINA_BAND_FREQ_HI;
            break;
        case TEA599X_FM_BAND_CUSTOM:
            *pFreqLo = TEA599X_CUSTOM_BAND_FREQ_LOW;
            *pFreqHi = TEA599X_CUSTOM_BAND_FREQ_HI;
            break;
        default:
            DBG_ERR("Band[%d] not supported", band);
            result = -ERANGE;
            break;
    }

    return result;
}

static int tea599x_fm_GetVersion(tea599x_version_struct * pversion)
{
    int result;
    unsigned short buff[7] = {0, 0, 0, 0, 0, 0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_GEN_GETVERSION_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 7*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_GEN_GETVERSION_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    if (!result)
    {
        pversion->manufacturer_id       = buff[1];
        pversion->product_id            = buff[2];
        pversion->hardware_version      = buff[3];
        pversion->firmware_release      = buff[4];
        pversion->firmware_version      = buff[5];
        pversion->firmware_application  = buff[6];

        DBG("ManufId[0x%04X] ProductId[0x%04X] HWVers[0x%04X] FWRel[0x%04X] FWVers[0x%04X] FWApp[0x%04X]",
            pversion->manufacturer_id, pversion->product_id, pversion->hardware_version,
            pversion->firmware_release, pversion->firmware_version, pversion->firmware_application);
    }

    return result;
}

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SetMute(tea599x_mute_state_enum mute_state)
#else
static int tea599x_fm_SetMute(unsigned int mute_state)
#endif
{
    int result;
    unsigned short interrupts = 0;
    unsigned char timeout = (unsigned char)(TEA599X_MAX_MUTE_TIME/TEA599X_POLLING_MUTE_PERIOD);
    unsigned short buff[3] = {0, 0, 0};
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
    {
        buff[0] = TEA599X_CMD_AUP_SETMUTE_CMD;
#ifndef CONFIG_RADIO_TEA599X_NEW_API
        buff[1] = (unsigned short)mute_state;
#else
	if (mute_state == TEA599X_MUTE_ON)
            buff[1] = 1;
	else
	    buff[1] = 0;
#endif
        buff[2] = (unsigned short)TEA599X_AUP_FADER_STATE_ON;
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 3*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_AUP_SETMUTE_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
    }
    }

    while ((!result) && (--timeout) && 
        !(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED))
    {
        tea599x_sleep(TEA599X_POLLING_MUTE_PERIOD);
        result = tea599x_fm_get_interrupt_register(&interrupts);
    }
    if(timeout == 0)
    {
        DBG_ERR("Time out");
        result = -ETIMEDOUT;
    }

    return result;
}

static int tea599x_fm_SetAudioBalance(unsigned short balance)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_AUP_SETBALANCE_CMD;
#ifndef CONFIG_RADIO_TEA599X_NEW_API
    buff[1] = balance;
#else
    buff[1] = tea599x_balance_table[balance];
#endif
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_AUP_SETBALANCE_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}

static int tea599x_fm_GetSignalStrength(unsigned short * rssi)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_RP_GETRSSI_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 2*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_RP_GETRSSI_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }
    if (!result)
        *rssi = buff[1];
    else
        *rssi = 0;

    DBG("Rssi[%d]", *rssi);

    return result;
}

static int tea599x_fm_StepFreq(tea599x_dir_enum dir)
{
    int result;
    unsigned short interrupts = 0;
    unsigned char timeout = (unsigned char)(TEA599X_MAX_STEP_TIME/TEA599X_POLLING_STEP_PERIOD);
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
    {
        buff[0] = TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL_CMD;
        buff[1] = (unsigned short)dir;
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_TUNE_STEPCHANNEL_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
    }
    }

    while ((!result) && (--timeout) && 
        !(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED) && !(interrupts & TEA599X_IRPT_OPERATIONFAILED))
    {
        tea599x_sleep(TEA599X_POLLING_STEP_PERIOD);
        result = tea599x_fm_get_interrupt_register(&interrupts);
    }
    if(timeout == 0)
    {
        DBG_ERR("Time out");
        result = -ETIMEDOUT;
    }

    return result;
}

static int tea599x_fm_StartScan(void)
{
    int result;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    int status;
#endif
    unsigned short buff[5] = {0, 0, 0, 0, 0};
    DBG_FUNC_ENTRY;

        buff[0] = TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START_CMD;
        buff[1] = (unsigned short)TEA599X_SCAN_MAX_CHANNELS;
        buff[2] = (unsigned short)tea599x_FmrData.v_RssiThreshold;
        buff[3] = (unsigned short)TEA599X_STOP_NOISE_CONTINUOUS;
        buff[4] = (unsigned short)TEA599X_STOP_NOISE_FINAL;
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 5*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_SCAN_EMBEDDED_START_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
    }

    /* Avoid having stop command issued within 12ms after start scan */
    tea599x_sleep(20);

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    if (!result)
    {
        if (tea599x_FmrData.v_band == TEA599X_FM_BAND_CHINA)
            tea599x_scan_timeout = TEA599X_MAX_SCAN_TIME_CHINA/TEA599X_TIMER_SCAN_TIME;
        else
            tea599x_scan_timeout = TEA599X_MAX_SCAN_TIME_E_US/TEA599X_TIMER_SCAN_TIME;
        /* schedule_delayed_work return 1 if success, 0 if work already on queue. We don't really care of it. */
        status = schedule_delayed_work(&tea599x_fm_ScanWork, msecs_to_jiffies(TEA599X_TIMER_SCAN_TIME));
        DBG("schedule_delayed_work[%d]", status);
    }
#endif

    return result;
}

static int tea599x_fm_StopScan(void)
{
    int result;
    unsigned short buff[1] = {0};
    DBG_FUNC_ENTRY;

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    cancel_delayed_work(&tea599x_fm_ScanWork);
#endif

    buff[0] = TEA599X_CMD_FMR_SP_STOP_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_STOP_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    /* If scan is ongoing and Stop command is sent, an interrupt will occure.
       If scan is not ongoing and Stop command is sent, int will not occure.
       Since we are not sure is scan is ongoing on not, 200ms delay is 
        introduced, instead of flags polling. */
    tea599x_sleep(200);

    DBG("Result[%d]", result);

    return result;
}

static void tea599x_fm_ScanWorkCb(struct work_struct *work)
{
    int result;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    int status;
#endif
    unsigned short interrupts = 0;
    DBG_FUNC_ENTRY;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    result = tea599x_lock_driver();
    if (result)
        DBG("Failed[%d]", result);
#endif

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    if (!result)
#endif
    result = tea599x_fm_get_interrupt_register(&interrupts);
    if (!result)
    {
        if(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED)
        {
            /* TEA599X_SCAN_DONE_CHANNELS_FOUND */
                result = tea599x_fm_ScanGetResults();
            if (!result)
                tea599x_globalEvent = TEA599X_EVENT_SCAN_CHANNELS_FOUND;
            else
            {
                tea599x_noOfScanFreq = 0;
                tea599x_globalEvent = TEA599X_EVENT_SCAN_NO_CHANNEL_FOUND;
            }
        }
        else if(interrupts & TEA599X_IRPT_OPERATIONFAILED)
        {
            /* TEA599X_SCAN_DONE_NO_CHANNEL_FOUND */
            tea599x_noOfScanFreq = 0;
            tea599x_globalEvent = TEA599X_EVENT_SCAN_NO_CHANNEL_FOUND;
        }
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
        else
        {
            /* TEA599X_SCAN_ONGOING */
            if (tea599x_scan_timeout)
            {
                tea599x_scan_timeout--;
                /* schedule_delayed_work return 1 if success, 0 if work already on queue. We don't really care of it. */
                status = schedule_delayed_work(&tea599x_fm_ScanWork, msecs_to_jiffies(TEA599X_TIMER_SCAN_TIME));
                DBG("schedule_delayed_work[%d]", status);
            }
        }
#endif
    }
    DBG("Result[%d]", result);
    tea599x_unlock_driver();
}

static int tea599x_fm_ScanGetResults(void)
{
    int result = 0;
    unsigned short buff[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char getcount = (unsigned char)((TEA599X_SCAN_MAX_CHANNELS/3) + ((TEA599X_SCAN_MAX_CHANNELS%3)!=0?1:0));
    unsigned char index = 0;
    unsigned long freqHi, freqLo;
    DBG_FUNC_ENTRY;

    result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &freqHi, &freqLo);

    while ((getcount--) && (!result))
    {
        buff[0] = TEA599X_CMD_FMR_SP_SCAN_GETRESULT_CMD;
        buff[1] = index;
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 8*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_SCAN_GETRESULT_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
        }

        if (!result)
        {
            /* Get number of channel found only once */
            if (index == 0)
                tea599x_noOfScanFreq = buff[1];
            tea599x_scanFreq[index] = freqLo + 100000 * (buff[2]/2);
            tea599x_scanFreqRssiLevel[index] = buff[3];
            DBG("Freq[%09d] - Rssi[%05d]", (int)(tea599x_scanFreq[index]), (int)(tea599x_scanFreqRssiLevel[index]));
            if ((index + 1) < TEA599X_SCAN_MAX_CHANNELS)
            {
                tea599x_scanFreq[index+1] = freqLo + 100000 * (buff[4]/2);
                tea599x_scanFreqRssiLevel[index+1] = buff[5];
                DBG("Freq[%09d] - Rssi[%05d]", 
                    (int)(tea599x_scanFreq[index+1]), (int)(tea599x_scanFreqRssiLevel[index+1]));
            }
            if ((index + 2) < TEA599X_SCAN_MAX_CHANNELS)
            {
                tea599x_scanFreq[index+2] = freqLo + 100000 * (buff[6]/2);
                tea599x_scanFreqRssiLevel[index+2] = buff[7];
                DBG("Freq[%09d] - Rssi[%05d]", 
                    (int)(tea599x_scanFreq[index+2]), (int)(tea599x_scanFreqRssiLevel[index+2]));
            }
            index += 3;
        }
    }

    return result;
}

static int tea599x_fm_SearchFreq(tea599x_dir_enum dir)
{
    int result;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    int status;
#endif
    unsigned short buff[5] = {0, 0, 0, 0, 0};
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();

    if (!result)
    {
        buff[0] = TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START_CMD;
        buff[1] = (unsigned short)dir;
        buff[2] = tea599x_FmrData.v_RssiThreshold;
        buff[3] = TEA599X_STOP_NOISE_CONTINUOUS;
        buff[4] = TEA599X_STOP_NOISE_FINAL;
        result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 5*sizeof(unsigned short));
        if (!result)
            tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
        if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_SEARCH_EMBEDDED_START_RSP))
        {
            DBG_ERR("Failed [%d]", buff[0]);
            result = -EPERM;
        }

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
        if (!result)
        {
            tea599x_search_timeout = TEA599X_MAX_SEARCH_TIME/TEA599X_TIMER_SEARCH_TIME;
            /* schedule_delayed_work return 1 if success, 0 if work already on queue. We don't really care of it. */
            status = schedule_delayed_work(&tea599x_fm_SearchWork, msecs_to_jiffies(TEA599X_TIMER_SEARCH_TIME));
            DBG("schedule_delayed_work[%d]", status);
        }
#endif
    }

    return result;
}

static void tea599x_fm_SearchWorkCb(struct work_struct *work)
{
    int result;
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    int status;
#endif
    unsigned short interrupts = 0;
    unsigned long freq;
    DBG_FUNC_ENTRY;

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    result = tea599x_lock_driver();
    if (result)
        DBG("Failed[%d]", result);
#endif

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    if (!result)
#endif
    result = tea599x_fm_get_interrupt_register(&interrupts);
    if (!result)
    {
        if(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED)
        {
            /* TEA599X_SEARCH_DONE_CHANNELS_FOUND */
            result = tea599x_fm_GetFrequency(&freq);
            if (!result)
            {
                tea599x_searchFreq = freq;
                tea599x_globalEvent = TEA599X_EVENT_SEARCH_CHANNEL_FOUND;
            }
            DBG("Found freq[%d|%d]", (int)freq, (int)tea599x_searchFreq);
        }
        else if(interrupts & TEA599X_IRPT_OPERATIONFAILED)
        {
            /* TEA599X_SEARCH_DONE_NO_CHANNEL_FOUND */
            tea599x_searchFreq = 0;
            tea599x_globalEvent = TEA599X_EVENT_SEARCH_NO_CHANNEL_FOUND;
        }
#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
        else
        {
            /* TEA599X_SEARCH_ONGOING */
           if (tea599x_search_timeout)
            {
                tea599x_search_timeout--;
                /* schedule_delayed_work return 1 if success, 0 if work already on queue. We don't really care of it. */
                status = schedule_delayed_work(&tea599x_fm_SearchWork, msecs_to_jiffies(TEA599X_TIMER_SEARCH_TIME));
                DBG("schedule_delayed_work[%d]", status);
            }

            result = tea599x_fm_GetFrequency(&freq);
            if (!result)
                tea599x_searchFreq = freq;
        }
#endif
    }
    DBG("Result[%d]", result);
    tea599x_unlock_driver();
}

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_StopSearch(void)
{
    int result;
    unsigned short buff[1] = {0};
    DBG_FUNC_ENTRY;

#ifdef CONFIG_POLL_TIMERS_IN_KERNEL
    cancel_delayed_work(&tea599x_fm_SearchWork);
#endif

    buff[0] = TEA599X_CMD_FMR_SP_STOP_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_STOP_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    /* If scan is ongoing and Stop command is sent, an interrupt will occure.
       If scan is not ongoing and Stop command is sent, int will not occure.
       Since we are not sure is scan is ongoing on not, 200ms delay is 
        introduced, instead of flags polling. */
    tea599x_sleep(200);

    DBG("Result[%d]", result);

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_SwitchRdsState(tea599x_FMR_rds_state_enum state)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_DP_SETCONTROL_CMD;
    buff[1] = (unsigned short)state;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_DP_SETCONTROL_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_RdsBlocksGetCnt(unsigned int * count)
{
    int result;
    unsigned short buff[2] = {0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 2*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_DP_BUFFER_GETGROUPCOUNT_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }
    if (!result)
        *count = buff[1];

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_RdsBlocksGetData(struct v4l2_rds_block * block)
{
    int result;
    unsigned short buff[7] = {0, 0, 0, 0, 0, 0, 0};
    DBG_FUNC_ENTRY;

    buff[0] = TEA599X_CMD_FMR_DP_BUFFER_GETGROUP_CMD;
    result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
    if (!result)
        tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 7*sizeof(unsigned short));
    if ((!result) && (buff[0] != TEA599X_CMD_FMR_DP_BUFFER_GETGROUP_RSP))
    {
        DBG_ERR("Failed [%d]", buff[0]);
        result = -EPERM;
    }
    if (!result)
    {
        block->blocks[0] = buff[1];
        block->blocks[1] = buff[2];
        block->blocks[2] = buff[3];
        block->blocks[3] = buff[4];
        block->status[0] = (unsigned char)((buff[5] >> 8) & 0x00FF);
        block->status[1] = (unsigned char)(buff[5] & 0x00FF);
        block->status[2] = (unsigned char)((buff[6] >> 8) & 0x00FF);
        block->status[3] = (unsigned char)(buff[6] & 0x00FF);
    }

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static unsigned int tea599x_fm_GetAltFreqRssi(unsigned long altFreq, unsigned short * rssi)
{
    int result;
    unsigned short buff[2] = {0, 0};
    unsigned long FreqHi, FreqLo;
    unsigned short interrupts = 0;
    unsigned char timeout = (unsigned char)(TEA599X_MAX_AF_RSSI_UPDATE_TIME/TEA599X_POLLING_AF_RSSI_UPDATE_PERIOD);
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
    {
        result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &FreqHi, &FreqLo);
        if (!result)
        {
            buff[0] = TEA599X_CMD_FMR_SP_AFUPDATE_START_CMD;
            buff[1] = (unsigned short)((altFreq - FreqLo)/50000);
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 2*sizeof(unsigned short));
            if (!result)
                tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
            if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_AFUPDATE_START_RSP))
            {
                DBG_ERR("Failed [%d]", buff[0]);
                result = -EPERM;
            }
        }

        while ((!result) && (--timeout) && 
            !(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED) && !(interrupts & TEA599X_IRPT_OPERATIONFAILED))
        {
            tea599x_sleep(TEA599X_POLLING_AF_RSSI_UPDATE_PERIOD);
            result = tea599x_fm_get_interrupt_register(&interrupts);
        }
        if(timeout == 0)
        {
            DBG_ERR("Time out");
            result = -ETIMEDOUT;
        }
        else
        {
            buff[0] = TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT_CMD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
            if (!result)
                tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 2*sizeof(unsigned short));
            if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_AFUPDATE_GETRESULT_RSP))
            {
                DBG_ERR("Failed [%d]", buff[0]);
                result = -EPERM;
            }
            else
                *rssi = buff[1];
        }
    }
    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_fm_AfStartSwitch(unsigned long freq,
                                    unsigned int pi,
                                    unsigned int pimask,
                                    unsigned int wait_time,
                                    unsigned int rssi,
                                    unsigned short * conclusion,
                                    unsigned short * newRssi,
                                    unsigned short * newPi)
{
    int result;
    unsigned short buff[6] = {0, 0, 0, 0, 0, 0};
    unsigned long FreqHi, FreqLo;
    unsigned short interrupts = 0;
    unsigned char timeout = (unsigned char)(TEA599X_POLLING_AF_RSSI_SWITCH_TIME/TEA599X_POLLING_AF_RSSI_SWITCH_PERIOD);
    DBG_FUNC_ENTRY;

    result = tea599x_fm_clearInterrupts();
    if (!result)
    {
        result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &FreqHi, &FreqLo);
        if (!result)
        {
            buff[0] = TEA599X_CMD_FMR_SP_AFSWITCH_START_CMD;
            buff[1] = (unsigned short)((freq - FreqLo)/50000);
            buff[2] = pi;
            buff[3] = pimask;
            buff[4] = rssi;
            buff[5] = wait_time;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 6*sizeof(unsigned short));
            if (!result)
                tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 1*sizeof(unsigned short));
            if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_AFSWITCH_START_RSP))
            {
                DBG_ERR("Failed [%d]", buff[0]);
                result = -EPERM;
            }
        }

        while ((!result) && (--timeout) && 
            !(interrupts & TEA599X_IRPT_OPERATIONSUCCEEDED) && !(interrupts & TEA599X_IRPT_OPERATIONFAILED))
        {
            tea599x_sleep(TEA599X_POLLING_AF_RSSI_SWITCH_PERIOD);
            result = tea599x_fm_get_interrupt_register(&interrupts);
        }
        if(timeout == 0)
        {
            DBG_ERR("Time out");
            result = -ETIMEDOUT;
        }
        else
        {
            buff[0] = TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT_CMD;
            result = tea599x_i2c_write(TEA599X_REG_CMD_REG, buff, 1*sizeof(unsigned short));
            if (!result)
                tea599x_i2c_read(TEA599X_REG_RSP_REG, buff, 4*sizeof(unsigned short));
            if ((!result) && (buff[0] != TEA599X_CMD_FMR_SP_AFSWITCH_GETRESULT_RSP))
            {
                DBG_ERR("Failed [%d]", buff[0]);
                result = -EPERM;
            }
            else
            {
                *conclusion = buff[1];
                *newRssi = buff[2];
                *newPi = buff[3];
            }
        }
    }
    return result;
}
#endif

/* ################################################################################# */
/*                                                                                   */
/* Driver fops                                                                       */
/*                                                                                   */
/* ################################################################################# */

/* ****************************** STRUCTURES *************************************** */

/* ****************************** PROTOTYPES *************************************** */

static int tea599x_open(struct file * file);
static int tea599x_release(struct file * file);
#ifndef CONFIG_RADIO_TEA599X_NEW_API
static long tea599x_ioctl(struct file *file, unsigned int cmd, unsigned long arg );
static ssize_t  tea599x_read(struct file *file, char __user *data, size_t count, loff_t *ppos);
static unsigned int  tea599x_poll(struct file *file, struct poll_table_struct *wait);
static long tea599x_do_ioctl(struct file *file, unsigned int cmd, void *arg );;
static void tea599x_release_sysfs( struct video_device *dev );
static int tea599x_ioctl_VIDIOC_QUERYCAP(void * arg);
static int tea599x_ioctl_VIDIOC_G_AUDIO(void * arg);
static int tea599x_ioctl_VIDIOC_QUERYCTRL(void * arg);
static int tea599x_ioctl_VIDIOC_G_CTRL(void * arg);
static int tea599x_ioctl_VIDIOC_S_CTRL(void * arg);
static int tea599x_ioctl_VIDIOC_G_TUNER(void * arg);
static int tea599x_ioctl_VIDIOC_G_FREQUENCY(void * arg);
static int tea599x_ioctl_VIDIOC_S_FREQUENCY(void * arg);
static int tea599x_ioctl_VIDIOC_S_SEEK(void * arg);
static int tea599x_ioctl_VIDIOC_S_STEP(void * arg);
static int tea599x_ioctl_VIDIOC_S_SCAN(void * arg);
static int tea599x_ioctl_VIDIOC_G_SCAN(void * arg);
static int tea599x_ioctl_VIDIOC_S_RADIO_MISC(void * arg);
static int tea599x_ioctl_VIDIOC_G_RADIO_MISC(void * arg);
static int tea599x_ioctl_VIDIOC_S_RADIO_STATE(void * arg);
#else
static long tea599x_do_ioctl(struct file *file, void *fh, int cmd, void *arg);
static int tea599x_ioctl_VIDIOC_QUERYCAP(struct file *file, void *fh, struct v4l2_capability *v);
static int tea599x_ioctl_VIDIOC_G_AUDIO(struct file *file, void *fh, struct v4l2_audio *v);
static int tea599x_ioctl_VIDIOC_QUERYCTRL(struct file *file, void *fh, struct v4l2_queryctrl *v);
static int tea599x_ioctl_VIDIOC_G_CTRL(struct file *file, void *fh, struct v4l2_control *v);
static int tea599x_ioctl_VIDIOC_S_CTRL(struct file *file, void *fh, struct v4l2_control *v);
static int tea599x_ioctl_VIDIOC_G_TUNER(struct file *file, void *fh, struct v4l2_tuner *v);
static int tea599x_ioctl_VIDIOC_S_TUNER(struct file *file, void *fh, struct v4l2_tuner *v);
static int tea599x_ioctl_VIDIOC_G_FREQUENCY(struct file *file, void *fh, struct v4l2_frequency *v);
static int tea599x_ioctl_VIDIOC_S_FREQUENCY(struct file *file, void *fh, struct v4l2_frequency *v);
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_SEEK(struct file *file, void *fh, struct v4l2_hw_freq_seek *v);
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_STEP(struct file *file, void *fh, struct v4l2_hw_freq_step *v);
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_SCAN(struct file *file, void *fh, struct v4l2_hw_freq_scan *v);
static int tea599x_ioctl_VIDIOC_G_HW_FREQ_SCAN(struct file *file, void *fh, struct v4l2_hw_freq_scan_status *v);
static int tea599x_ioctl_VIDIOC_S_RADIO_MISC(struct file *file, void *fh, struct v4l2_radio_misc *v);
static int tea599x_ioctl_VIDIOC_G_RADIO_MISC(struct file *file, void *fh, struct v4l2_radio_misc *v);
static int tea599x_ioctl_VIDIOC_S_RADIO_STATE(struct file *file, void *fh, struct v4l2_radio_state *v);
static int tea599x_ioctl_VIDIOC_S_RADIO_RDS(struct file *file, void *fh, struct v4l2_radio_rds_ctrl *v);
static int tea599x_ioctl_VIDIOC_G_RADIO_RDS(struct file *file, void *fh, struct v4l2_radio_rds_data *v);
static int tea599x_ioctl_VIDIOC_S_RADIO_AF_SWITCH(struct file *file, void *fh, struct v4l2_radio_af_switch * v);
#endif

/* ****************************** DATA ********************************************* */

/* ****************************** FUNCTIONS **************************************** */

static int tea599x_open(struct file * file)
{
    int retVal = -EINVAL;

    DBG_FUNC_ENTRY;

    retVal = tea599x_lock_driver();
    if (retVal)
        return retVal;

    tea599x_users++;
    if(tea599x_users > 1) {
        DBG_ERR("More than 1 simultaneous user, error!!!");
        retVal = -EINVAL;
    }
    else {
        if (!try_module_get(THIS_MODULE))
            retVal = -ENODEV;
#ifndef  CONFIG_RADIO_TEA599X_NEW_API
        else
        {
            DBG("Switching On FM");
            retVal = tea599x_fm_SwitchOn(TEA599X_US_EU_BAND_FREQ_LOW, TEA599X_FM_BAND_US_EU, TEA599X_FM_GRID_100);
            tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
            tea599x_FmrData.v_Frequency = HZ_2_V4L2(TEA599X_US_EU_BAND_FREQ_LOW);
            tea599x_FmrData.v_Muted = false;
            tea599x_FmrData.v_AudioPath = 0; /* By default the balance is set to 0 */
            tea599x_FmrData.v_dac = 1; /* DAC is on */
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_FmrData.v_band = TEA599X_FM_BAND_US_EU;
            tea599x_FmrData.v_grid = TEA599X_FM_GRID_100;
        }
#else
    tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_OFF;
    tea599x_FmrData.v_Frequency = 0;
    tea599x_FmrData.v_Muted = TEA599X_MUTE_OFF;
    tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
    tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
    tea599x_FmrData.v_rdsState = TEA599X_FMR_RDS_STATE_OFF;
#endif
    }

    tea599x_unlock_driver();

    return retVal;
}

static int tea599x_release(struct file * file)
{
    int retVal;

    DBG_FUNC_ENTRY;

    retVal = tea599x_lock_driver();
    if (retVal)
        return retVal;

    tea599x_users--;

#ifndef CONFIG_RADIO_TEA599X_NEW_API
    retVal = tea599x_fm_SwitchOff();
    if(!retVal) {
        tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_OFF;
        tea599x_FmrData.v_Frequency = 0;
        tea599x_FmrData.v_Muted = false;
        tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
        tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
    }
#endif
    module_put( THIS_MODULE );

    tea599x_unlock_driver();

    return retVal;
}

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static long tea599x_ioctl(struct file *file, unsigned int cmd, unsigned long arg )
{
    long result;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;

    result = video_usercopy(file, cmd, arg, tea599x_do_ioctl );

    tea599x_unlock_driver();

    return result;
}

static ssize_t  tea599x_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
    DBG_FUNC_ENTRY;
    return 0;
}

static unsigned int  tea599x_poll(struct file *file, struct poll_table_struct *wait)
{
    DBG_FUNC_ENTRY;
    return POLLIN;
}

static long tea599x_do_ioctl(struct file *file, unsigned int cmd, void *arg )
{
    int result;

    DBG_FUNC_ENTRY;
    DBG("cmd[%d] - arg[%d]", cmd, (int)arg);

    switch( cmd )
    {
        /* Query capabilities. Should be the first thing to be called by an application. */
        case VIDIOC_QUERYCAP:
            result = tea599x_ioctl_VIDIOC_QUERYCAP(arg);
            break;

        /* Enumerate supported video standards */
        case VIDIOC_ENUMSTD:
        /* Select video standard */
        case VIDIOC_S_STD:
        /* Get video data format */
        case VIDIOC_G_FMT:
        /* Select audio input */
        case VIDIOC_S_AUDIO:
            /* They're all not supported */
            DBG("Cmd[%d]: not supported", cmd);
            result = -EINVAL;
            break;

        /* Get audio input. Not relevant, but the compat layer requires the
        implementation of this ioctl. So we return success. */
        case VIDIOC_G_AUDIO:
            result = tea599x_ioctl_VIDIOC_G_AUDIO(arg);
            break;

        /* Get the currently selected video standard. Not relevant, but the compat layer requires
           the implementation of this ioctl. So we return success, but do not indicate any video mode. */
        case VIDIOC_G_STD:
            *(v4l2_std_id *)arg = 0;
            result = 0;
            break;

        /* Query control elements.
        Not all control elements are real hardware functions. Some are virtual elements
        whose only purpose is to make some applications happy who rely on them. */
        case VIDIOC_QUERYCTRL:
            result = tea599x_ioctl_VIDIOC_QUERYCTRL(arg);
            break;

        /* Read control element value */
        case VIDIOC_G_CTRL:
            result = tea599x_ioctl_VIDIOC_G_CTRL(arg);
            break;

        /* Set control element value */
        case VIDIOC_S_CTRL:
            result = tea599x_ioctl_VIDIOC_S_CTRL(arg);
            break;

        /* Query the tuner attributes, including current status (RSSI, AFC, stereo flag) */
        case VIDIOC_G_TUNER:
            result = tea599x_ioctl_VIDIOC_G_TUNER(arg);
            break;

        /* Select the desired tuner. Not much to do here... */
        /* Only accepts selecting the one and only tuner available. */
        case VIDIOC_S_TUNER:
        {
            struct v4l2_tuner *v = arg;
            if( v->index != 0 )
                result = -EINVAL;                         /* Sorry, only 1 tuner */
            result = 0;
        }
        break;

        /* Query the current tuner frequency. */
        case VIDIOC_G_FREQUENCY:
            result = tea599x_ioctl_VIDIOC_G_FREQUENCY(arg);
            break;

        /* Set the tuner frequency */
        case VIDIOC_S_FREQUENCY:
            result = tea599x_ioctl_VIDIOC_S_FREQUENCY(arg);
            break;


        case VIDIOC_S_SEEK :
            result = tea599x_ioctl_VIDIOC_S_SEEK(arg);
            break;

        case VIDIOC_S_STEP :
            result = tea599x_ioctl_VIDIOC_S_STEP(arg);
            break;

        case VIDIOC_S_SCAN :
            result = tea599x_ioctl_VIDIOC_S_SCAN(arg);
            break;

        case VIDIOC_G_SCAN :
            result = tea599x_ioctl_VIDIOC_G_SCAN(arg);
            break;

        case VIDIOC_S_RADIO_MISC :
            result = tea599x_ioctl_VIDIOC_S_RADIO_MISC(arg);
            break;

        case VIDIOC_G_RADIO_MISC :
            result = tea599x_ioctl_VIDIOC_G_RADIO_MISC(arg);
            break;

        case VIDIOC_S_RADIO_STATE:
            result = tea599x_ioctl_VIDIOC_S_RADIO_STATE(arg);
            break;

        /* Other ioctl's may be legacy V4L1 ioctl's. Try first, and report error if it fails. */
        default:
            DBG("Translate request: cmd[%d]", cmd);
            result = v4l_compat_translate_ioctl(file, cmd, arg, tea599x_do_ioctl );
            break;
    }

    return result;
}
#else /* CONFIG_RADIO_TEA599X_NEW_API */
static long tea599x_do_ioctl(struct file *file, void *fh, int cmd, void *arg)
{
    int result;

    DBG_FUNC_ENTRY;
    DBG("cmd[%d] - arg[%d]", cmd, (int)arg);

    switch( cmd )
    {
        case VIDIOC_S_HW_FREQ_STEP :
            result = tea599x_ioctl_VIDIOC_S_HW_FREQ_STEP(file, fh, (struct v4l2_hw_freq_step *)arg);
            break;

        case VIDIOC_S_HW_FREQ_SCAN :
            result = tea599x_ioctl_VIDIOC_S_HW_FREQ_SCAN(file, fh, (struct v4l2_hw_freq_scan *)arg);
            break;

        case VIDIOC_G_HW_FREQ_SCAN :
            result = tea599x_ioctl_VIDIOC_G_HW_FREQ_SCAN(file, fh, (struct v4l2_hw_freq_scan_status *)arg);
            break;

        case VIDIOC_S_RADIO_MISC :
            result = tea599x_ioctl_VIDIOC_S_RADIO_MISC(file, fh, (struct v4l2_radio_misc *)arg);
            break;

        case VIDIOC_G_RADIO_MISC :
            result = tea599x_ioctl_VIDIOC_G_RADIO_MISC(file, fh, (struct v4l2_radio_misc *)arg);
            break;

        case VIDIOC_S_RADIO_STATE:
            result = tea599x_ioctl_VIDIOC_S_RADIO_STATE(file, fh, (struct v4l2_radio_state *)arg);
            break;

        case VIDIOC_S_RADIO_RDS:
            result = tea599x_ioctl_VIDIOC_S_RADIO_RDS(file, fh, (struct v4l2_radio_rds_ctrl *)arg);
            break;

        case VIDIOC_G_RADIO_RDS:
            result = tea599x_ioctl_VIDIOC_G_RADIO_RDS(file, fh, (struct v4l2_radio_rds_data *)arg);
            break;

        case VIDIOC_S_RADIO_AF_SWITCH:
            result = tea599x_ioctl_VIDIOC_S_RADIO_AF_SWITCH(file, fh, (struct v4l2_radio_af_switch *)arg);
            break;

        default:
            DBG_ERR("Unknown request: cmd[%d]", cmd);
            result = -EINVAL;
            break;
    }

    return result;
}
#endif /* CONFIG_RADIO_TEA599X_NEW_API */

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_QUERYCAP(void * arg)
{
    struct v4l2_capability *v = arg;
    DBG_FUNC_ENTRY;

    memset( v, 0, sizeof(*v) );
    strlcpy(v->driver, "TEA599x Driver", sizeof(v->driver));
    strlcpy(v->card, "TEA599x FM Radio", sizeof(v->card));
    strcpy( v->bus_info, "platform" );          /* max. 32 bytes! */
    v->version = TEA599X_RADIO_VERSION;
    v->version = LINUX_VERSION_CODE;
    v->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO
                        | V4L2_CAP_READWRITE
                        | V4L2_CAP_RDS_CAPTURE;
    memset( v->reserved, 0, sizeof(v->reserved) );

    return 0;
}
#else
static int tea599x_ioctl_VIDIOC_QUERYCAP(struct file *file, void *fh, struct v4l2_capability *v)
{
    int result;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    memset( v, 0, sizeof(*v) );
    strlcpy(v->driver, "TEA599x Driver", sizeof(v->driver));
    strlcpy(v->card, "TEA599x FM Radio", sizeof(v->card));
    strlcpy(v->bus_info, "I2C", sizeof(v->bus_info));
    v->version = TEA599X_RADIO_VERSION;
    v->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO | V4L2_CAP_HW_FREQ_SEEK;

    tea599x_unlock_driver();

    return 0;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_AUDIO(void * arg)
{
    struct v4l2_audio *v = arg;
    DBG_FUNC_ENTRY;
    strcpy( v->name, "" );                      /* max. 16 bytes! */
    v->capability = 0;
    v->mode = 0;
    return 0;
}
#else
static int tea599x_ioctl_VIDIOC_G_AUDIO(struct file *file, void *fh, struct v4l2_audio *v)
{
    int result;
    DBG_FUNC_ENTRY;
    
    result = tea599x_lock_driver();
    if (result)
        return result;
    
    strlcpy(v->name, "", sizeof(v->name));
    v->capability = V4L2_AUDCAP_STEREO;
    v->mode = 0;

    tea599x_unlock_driver();

    return 0;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_QUERYCTRL(void * arg)
{
    struct v4l2_queryctrl *v = arg;
    int result = 0;
    DBG_FUNC_ENTRY;

    /* Check which control is requested */
    switch( v->id )
    {
        /* Audio Mute. This is a hardware function in the TEA5766 radio */
        case V4L2_CID_AUDIO_MUTE:
            DBG( "V4L2_CID_AUDIO_MUTE" );
            v->type = V4L2_CTRL_TYPE_BOOLEAN;
            v->minimum = 0;
            v->maximum = 1;
            v->step = 1;
            v->default_value = 0;
            v->flags = 0;
            strncpy( v->name, "TEA599x Mute", 32 ); /* Max 32 bytes */
            break;

        /* Audio Volume. Not implemented in hardware. just a dummy function. */
        case V4L2_CID_AUDIO_VOLUME:
            DBG( "V4L2_CID_AUDIO_VOLUME" );
            strncpy( v->name, "TEA599x Volume", 32 ); /* Max 32 bytes */
            v->minimum = 0x00;
            v->maximum = 0x15;
            v->step = 1;
            v->default_value = 0x15;
            v->flags = 0;
            v->type = V4L2_CTRL_TYPE_INTEGER;
            break;

        case V4L2_CID_AUDIO_BALANCE:
            DBG("V4L2_CID_AUDIO_BALANCE" );
            strncpy( v->name, "TEA599x Audio Balance", 32 );
            v->type = V4L2_CTRL_TYPE_INTEGER;
            v->minimum = 0x0000;
            v->maximum = 0xFFFF;
            v->step = 0x0001;
            v->default_value = 0x0000;
            v->flags = 0;
            break;

        /* Explicitely list some CIDs to produce a verbose output in debug mode. */
        case V4L2_CID_AUDIO_BASS:
            DBG_ERR("V4L2_CID_AUDIO_BASS (unsupported)" );
            result = -EINVAL;
            break;

        case V4L2_CID_AUDIO_TREBLE:
            DBG_ERR("V4L2_CID_AUDIO_TREBLE (unsupported)" );
            result = -EINVAL;
            break;

        default:
            DBG_ERR("Unsupported id[%d]", (int)v->id );
            result = -EINVAL;
            break;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_QUERYCTRL(struct file *file, void *fh, struct v4l2_queryctrl *v)
{
    int result;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    /* Check which control is requested */
    switch( v->id )
    {
        /* Audio Mute. This is a hardware function in the TEA5766 radio */
        case V4L2_CID_AUDIO_MUTE:
            DBG( "V4L2_CID_AUDIO_MUTE" );
            v->type = V4L2_CTRL_TYPE_BOOLEAN;
	    strlcpy(v->name, "TEA599x Mute", sizeof(v->name));
            v->minimum = TEA599X_MUTE_OFF;
            v->maximum = TEA599X_MUTE_ON;
            v->step = 1;
            v->default_value = TEA599X_MUTE_OFF;
            v->flags = 0;
            break;

        /* Audio Volume. */
        case V4L2_CID_AUDIO_VOLUME:
            DBG( "V4L2_CID_AUDIO_VOLUME" );
	    v->type = V4L2_CTRL_TYPE_INTEGER;
            strlcpy(v->name, "TEA599x Volume", sizeof(v->name));
            v->minimum = TEA599X_VOLUME_MIN;
            v->maximum = TEA599X_VOLUME_MAX;
            v->step = 1;
            v->default_value = TEA599X_VOLUME_DEF;
            v->flags = 0;
            break;

        case V4L2_CID_AUDIO_BALANCE:
            DBG("V4L2_CID_AUDIO_BALANCE" );
	    v->type = V4L2_CTRL_TYPE_INTEGER;
            strlcpy(v->name, "TEA599x Audio Balance", sizeof(v->name));
            v->minimum = TEA599X_BALANCE_LEFT;
            v->maximum = TEA599X_BALANCE_RIGHT;
            v->step = 1;
            v->default_value = TEA599X_BALANCE_CENTER;
            v->flags = 0;
            break;

        default:
            DBG_ERR("Unsupported id[%d]", (int)v->id );
            result = -EINVAL;
            break;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_CTRL(void * arg)
{
    struct v4l2_control *v = arg;
    int result = 0;
    DBG_FUNC_ENTRY;
    DBG("v->id[%d]", v->id);

    switch (v->id)
    {
        case V4L2_CID_AUDIO_VOLUME:     v->value = tea599x_FmrData.v_Volume;      break;
        case V4L2_CID_AUDIO_MUTE:       v->value = tea599x_FmrData.v_Muted;       break;
        case V4L2_CID_AUDIO_BALANCE:    v->value = tea599x_FmrData.v_AudioPath;   break;
        default:
            DBG_ERR("Cid[%d] not supported", v->id);
            result = -EINVAL;
            break;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_G_CTRL(struct file *file, void *fh, struct v4l2_control *v)
{
    int result;
    DBG_FUNC_ENTRY;
    DBG("v->id[%d]", v->id);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    switch (v->id)
    {
        case V4L2_CID_AUDIO_VOLUME:     v->value = tea599x_FmrData.v_Volume;      break;
        case V4L2_CID_AUDIO_MUTE:       v->value = tea599x_FmrData.v_Muted;       break;
        case V4L2_CID_AUDIO_BALANCE:    v->value = tea599x_FmrData.v_AudioPath;   break;
        default:
            DBG_ERR("Cid[%d] not supported", v->id);
            result = -EINVAL;
            break;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_CTRL(void * arg)
{
    struct v4l2_control *v = arg;
    int status;
    int result = 0;
    DBG_FUNC_ENTRY;

    /* Check which control is requested */
    switch( v->id )
    {
        /* Audio Mute. This is a hardware function in the TEA5768 radio */
        case V4L2_CID_AUDIO_MUTE:
            DBG( "V4L2_CID_AUDIO_MUTE: value[%d]", v->value );
            if ( v->value > 1 && v->value < 0)
                result = -ERANGE;
            else if(v->value) 
                status = tea599x_fm_SetMute(TEA599X_MUTE_STATE_ON);
            else 
                status = tea599x_fm_SetMute(TEA599X_MUTE_STATE_OFF);
            result = status;
            if(!result)
                tea599x_FmrData.v_Muted = v->value;
            break;

        case V4L2_CID_AUDIO_VOLUME:
            DBG( "V4L2_CID_AUDIO_VOLUME: value[%d]", v->value );
            if ( v->value > 20 && v->value < 0 )
                result = -ERANGE;
            else
            {
                status = tea599x_fm_SetVolume(v->value);
                result = status;
                if (!result)
                    tea599x_FmrData.v_Volume = v->value;
            }
            break;

        case V4L2_CID_AUDIO_BALANCE: 
            DBG( "V4L2_CID_AUDIO_BALANCE: value[%d]", v->value );
            status = tea599x_fm_SetAudioBalance(v->value);
            result = status;
            if (!result)
                tea599x_FmrData.v_AudioPath = v->value;
            break;

        default:
            DBG( "Unsupported (id[%d])", (int)v->id );
            return -EINVAL;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_CTRL(struct file *file, void *fh, struct v4l2_control *v)
{
    int status;
    int result;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    /* Check which control is requested */
    switch( v->id )
    {
        /* Audio Mute. This is a hardware function in the TEA5768 radio */
        case V4L2_CID_AUDIO_MUTE:
            DBG( "V4L2_CID_AUDIO_MUTE: value[%d]", v->value );
            if ( v->value > 1 && v->value < 0)
            {
                DBG_ERR("Cid [%d] val[%d] not supported", v->id, v->value);
                result = -ERANGE;
            }
	    else
                result = tea599x_fm_SetMute(v->value);
            if(!result)
                tea599x_FmrData.v_Muted = v->value;
            break;

        case V4L2_CID_AUDIO_VOLUME:
            DBG( "V4L2_CID_AUDIO_VOLUME: value[%d]", v->value );
            if ( v->value > 20 && v->value < 0 )
            {
                DBG_ERR("Cid [%d] val[%d] not supported", v->id, v->value);
                result = -ERANGE;
            }
            else
            {
                status = tea599x_fm_SetVolume(v->value);
                result = status;
                if (!result)
                    tea599x_FmrData.v_Volume = v->value;
            }
            break;

        case V4L2_CID_AUDIO_BALANCE: 
            DBG( "V4L2_CID_AUDIO_BALANCE: value[%d]", v->value );
	    if ( v->value > 20 && v->value < 0 )
            {
                DBG_ERR("Cid [%d] val[%d] not supported", v->id, v->value);
                result = -ERANGE;
            }
            else
            {
	    	status = tea599x_fm_SetAudioBalance(v->value);
            	result = status;
            	if (!result)
                    tea599x_FmrData.v_AudioPath = v->value;
	    }
            break;

        default:
            DBG_ERR( "Unsupported (id[%d])", (int)v->id );
            return -EINVAL;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_TUNER(void * arg)
{
    struct v4l2_tuner *v = arg;
    int result = 0;
    unsigned short rssi;
    unsigned long freqHi, freqLo;
    DBG_FUNC_ENTRY;

    if (v->index > 0)
        result = -EINVAL;
    else if (!(result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &freqHi, &freqLo)))
    {
        strcpy(v->name, "TEA599x FM Receiver\n ");
        v->type = V4L2_TUNER_RADIO;
        v->rangelow = HZ_2_V4L2(freqLo);
        v->rangehigh = HZ_2_V4L2(freqHi);
        v->capability = V4L2_TUNER_CAP_LOW          /* Frequency steps = 1/16 kHz */
                            | V4L2_TUNER_CAP_STEREO;      /* Can receive stereo */
        /* Wait for 380ms after preset to get the mono stereo mode state */ 
        tea599x_sleep(380);
        switch (tea599x_FmrData.v_mode)
        {
            /* stereo */
            case TEA599X_STEREO_MODE_STEREO :
                v->audmode = V4L2_TUNER_MODE_STEREO;
                v->rxsubchans = V4L2_TUNER_SUB_STEREO;
                break;
            /* mono */
            case TEA599X_STEREO_MODE_FORCE_MONO :
                v->audmode = V4L2_TUNER_SUB_MONO;
                v->rxsubchans = V4L2_TUNER_SUB_STEREO;
                break;
            /* Switching or Blending, set mode as Stereo */
            default:
                v->audmode = V4L2_TUNER_MODE_STEREO;
                v->rxsubchans = V4L2_TUNER_SUB_MONO;
                break;
        }

        result =  tea599x_fm_GetSignalStrength(&rssi);
        if (!result)
            v->signal = rssi;
        else
            v->signal = 0;
        memset( v->reserved, 0, sizeof(v->reserved) );
    }
    return result;
}
#else
static int tea599x_ioctl_VIDIOC_G_TUNER(struct file *file, void *fh, struct v4l2_tuner *v)
{
    int result;
    unsigned short rssi;
    unsigned long freqHi, freqLo;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;

    if (v->index > 0)
        result = -EINVAL;
    else if (!(result = tea599x_fm_GetBandFreqHiLo(tea599x_FmrData.v_band, &freqHi, &freqLo)))
    {
        strlcpy(v->name, "TEA599x FM Receiver", sizeof(v->name));
        v->type = V4L2_TUNER_RADIO;
        v->rangelow = HZ_2_V4L2(freqLo);
        v->rangehigh = HZ_2_V4L2(freqHi);
        v->capability = V4L2_TUNER_CAP_LOW          /* Frequency steps = 1/16 kHz */
                            | V4L2_TUNER_CAP_STEREO;      /* Can receive stereo */
        /* Wait for 380ms after preset to get the mono stereo mode state */ 
        tea599x_sleep(380);
        switch (tea599x_FmrData.v_mode)
        {
            /* stereo */
            case TEA599X_STEREO_MODE_STEREO :
                v->audmode = V4L2_TUNER_MODE_STEREO;
                v->rxsubchans = V4L2_TUNER_SUB_STEREO;
                break;
            /* mono */
            case TEA599X_STEREO_MODE_FORCE_MONO :
                v->audmode = V4L2_TUNER_SUB_MONO;
                v->rxsubchans = V4L2_TUNER_SUB_STEREO;
                break;
            /* Switching or Blending, set mode as Stereo */
            default:
                v->audmode = V4L2_TUNER_MODE_STEREO;
                v->rxsubchans = V4L2_TUNER_SUB_MONO;
                break;
        }

        result =  tea599x_fm_GetSignalStrength(&rssi);
        if (!result)
            v->signal = rssi;
        else
            v->signal = 0;
    }
    
    tea599x_unlock_driver();

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_TUNER(struct file *file, void *fh, struct v4l2_tuner *v)
{
    int result;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    if( v->index != 0 )
        result = -EINVAL;    /* Sorry, only 1 tuner */
    
    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_FREQUENCY(void * arg)
{
    struct v4l2_frequency *v4l2_freq = arg;
    int result = 0;
    unsigned long freq;
    DBG_FUNC_ENTRY;

    DBG("tea599x_FmrData.v_SeekStatus[%d]", tea599x_FmrData.v_SeekStatus);

    if(tea599x_FmrData.v_SeekStatus == TEA599X_FMR_SEEK_IN_PROGRESS)
    {
        #ifndef CONFIG_POLL_TIMERS_IN_KERNEL
        /* Check status with FM chip */
        tea599x_fm_SearchWorkCb((struct work_struct *)NULL);
        #endif

        /*  Check if seek is finished or not */
        if(tea599x_globalEvent == TEA599X_EVENT_SEARCH_CHANNEL_FOUND)
        { 
            /* Seek is finished */
            tea599x_FmrData.v_Frequency = HZ_2_V4L2(tea599x_searchFreq);
            v4l2_freq->frequency = tea599x_FmrData.v_Frequency;
            DBG("freq[%d] SearchFreq[%d]", (int)(tea599x_FmrData.v_Frequency), (int)tea599x_searchFreq); 
            v4l2_freq->status = V4L2_RADIO_SEEK_NONE;
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else if(tea599x_globalEvent == TEA599X_EVENT_SEARCH_NO_CHANNEL_FOUND)
        {
            /* Seek is finished */
            v4l2_freq->status = V4L2_RADIO_SEEK_NONE;
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
            v4l2_freq->frequency =  tea599x_FmrData.v_Frequency; 
        }
        else
        {
            /* Seek is progress */
            v4l2_freq->status = V4L2_RADIO_SEEK_IN_PROGRESS;
            v4l2_freq->frequency =  HZ_2_V4L2(tea599x_searchFreq);
        }
    }
    else
    {
        v4l2_freq->status = V4L2_RADIO_SEEK_NONE;
        tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        result = tea599x_fm_GetFrequency((unsigned long *)&freq);
        if(!result)
        {
            tea599x_FmrData.v_Frequency = HZ_2_V4L2(freq);
            v4l2_freq->frequency = tea599x_FmrData.v_Frequency;
        }
        else 
            v4l2_freq->frequency = tea599x_FmrData.v_Frequency;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_G_FREQUENCY(struct file *file, void *fh, struct v4l2_frequency *v)
{
    int result;
    unsigned long freq;
    DBG_FUNC_ENTRY;

    DBG("tea599x_FmrData.v_SeekStatus[%d]", tea599x_FmrData.v_SeekStatus);

    result = tea599x_lock_driver();
    if (result)
        return result;
	
    v->seek_ongoing = false;

    if(tea599x_FmrData.v_SeekStatus == TEA599X_FMR_SEEK_IN_PROGRESS)
    {
        #ifndef CONFIG_POLL_TIMERS_IN_KERNEL
        /* Check status with FM chip */
        tea599x_fm_SearchWorkCb((struct work_struct *)NULL);
        #endif

        /*  Check if seek is finished or not */
        if(tea599x_globalEvent == TEA599X_EVENT_SEARCH_CHANNEL_FOUND)
        { 
            /* Seek is finished */
            tea599x_FmrData.v_Frequency = HZ_2_V4L2(tea599x_searchFreq);
            v->frequency = tea599x_FmrData.v_Frequency;
            DBG("freq[%d] SearchFreq[%d]", (int)(tea599x_FmrData.v_Frequency), (int)tea599x_searchFreq); 
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else if(tea599x_globalEvent == TEA599X_EVENT_SEARCH_NO_CHANNEL_FOUND)
        {
            /* Seek is finished */
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
            v->frequency =  tea599x_FmrData.v_Frequency; 
        }
        else
        {
            /* Seek is progress */
            v->seek_ongoing = true;
            v->frequency =  HZ_2_V4L2(tea599x_searchFreq);
        }
    }
    else
    {
        tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        result = tea599x_fm_GetFrequency((unsigned long *)&freq);
        if(!result)
            tea599x_FmrData.v_Frequency = HZ_2_V4L2(freq);
        v->frequency = tea599x_FmrData.v_Frequency;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_FREQUENCY(void * arg)
{
    const struct v4l2_frequency *v = arg;
    int result;
    DBG_FUNC_ENTRY;
    DBG("Freq[%d]", v->frequency);

    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    tea599x_FmrData.v_SeekStatus =  TEA599X_FMR_SEEK_NONE;
    tea599x_FmrData.v_ScanStatus =  TEA599X_FMR_SCAN_NONE;
    tea599x_FmrData.v_Frequency = v->frequency;
    result = tea599x_fm_PresetFreq(V4L2_2_HZ(v->frequency));
    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_FREQUENCY(struct file *file, void *fh, struct v4l2_frequency *v)
{
    int result;
    DBG_FUNC_ENTRY;
    DBG("Freq[%d]", v->frequency);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    tea599x_FmrData.v_SeekStatus =  TEA599X_FMR_SEEK_NONE;
    tea599x_FmrData.v_ScanStatus =  TEA599X_FMR_SCAN_NONE;
    result = tea599x_fm_PresetFreq(V4L2_2_HZ(v->frequency));
    if (!result)
        tea599x_FmrData.v_Frequency = v->frequency;

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_SEEK(void * arg)
{
    struct v4l2_radio_seek* Seek = arg;
    int result;
    DBG_FUNC_ENTRY;

    DBG("tea599x_FmrData.v_SeekStatus[%d] Seek->direction[%d] UP[%d] DOWN[%d]", 
       tea599x_FmrData.v_SeekStatus, Seek->direction, V4L2_RADIO_SEEK_UPWARDS, V4L2_RADIO_SEEK_DOWNWARDS);

    if ( tea599x_FmrData.v_SeekStatus == TEA599X_FMR_SEEK_IN_PROGRESS )
        result = -EINVAL;
    else
    {
        tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        tea599x_searchFreq = 0;
        tea599x_noOfScanFreq = 0;

        tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_IN_PROGRESS;
        if(V4L2_RADIO_SEEK_UPWARDS == Seek->direction)
            result = tea599x_fm_SearchFreq(TEA599X_DIR_UP);
        else if(V4L2_RADIO_SEEK_DOWNWARDS == Seek->direction)
            result = tea599x_fm_SearchFreq(TEA599X_DIR_DOWN);
        else 
            result = -EINVAL;
    }
    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_SEEK(struct file *file, void *fh, struct v4l2_hw_freq_seek *v)
{
    int result;
    DBG_FUNC_ENTRY;

    DBG("tea599x_FmrData.v_SeekStatus[%d] v->seek_upward[%d] v->stop_seek[%d]", 
       tea599x_FmrData.v_SeekStatus, v->seek_upward, v->stop_seek);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    if (v->stop_seek)
    {
        if ( tea599x_FmrData.v_SeekStatus != TEA599X_FMR_SEEK_IN_PROGRESS )
            result = -EINVAL;
        else
        {
            result = tea599x_fm_StopSearch();
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
        }
    }
    else
    {
    if ( tea599x_FmrData.v_SeekStatus == TEA599X_FMR_SEEK_IN_PROGRESS )
        result = -EINVAL;
    else
    {
        tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        tea599x_searchFreq = 0;
        tea599x_noOfScanFreq = 0;

        tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_IN_PROGRESS;
        if(v->seek_upward)
            result = tea599x_fm_SearchFreq(TEA599X_DIR_UP);
        else
            result = tea599x_fm_SearchFreq(TEA599X_DIR_DOWN);
    }
    }
    
    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_STEP(void * arg)
{
    struct v4l2_radio_step* Step = arg;
    int result;
    DBG_FUNC_ENTRY;

    DBG("Step->direction[%d]", Step->direction);

    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    tea599x_FmrData.v_SeekStatus =  TEA599X_FMR_SEEK_NONE;
    tea599x_FmrData.v_ScanStatus =  TEA599X_FMR_SCAN_NONE;

    if(V4L2_RADIO_STEP_UPWARDS == Step->direction)
        result = tea599x_fm_StepFreq(TEA599X_DIR_UP);
    else if(V4L2_RADIO_STEP_DOWNWARDS == Step->direction)
        result = tea599x_fm_StepFreq(TEA599X_DIR_DOWN);
    else 
        result = -EINVAL;

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_STEP(struct file *file, void *fh, struct v4l2_hw_freq_step *v)
{
    int result;
    DBG_FUNC_ENTRY;

    DBG("v->step_upward[%d]", v->step_upward);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    tea599x_FmrData.v_SeekStatus =  TEA599X_FMR_SEEK_NONE;
    tea599x_FmrData.v_ScanStatus =  TEA599X_FMR_SCAN_NONE;

    if(v->step_upward)
        result = tea599x_fm_StepFreq(TEA599X_DIR_UP);
    else
        result = tea599x_fm_StepFreq(TEA599X_DIR_DOWN);

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_SCAN(void * arg)
{
    struct v4l2_radio_scan* Scan = arg;
    int result;
    DBG_FUNC_ENTRY;

    DBG("Status[%d] mode[%d]", tea599x_FmrData.v_ScanStatus, Scan->mode);

    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    if(V4L2_RADIO_SCAN_START == Scan->mode)
    {
        if ( tea599x_FmrData.v_ScanStatus == TEA599X_FMR_SCAN_IN_PROGRESS )
            result = -EBUSY;
        else
        {
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_IN_PROGRESS;
            result = tea599x_fm_StartScan();
        }
    }
    else if(V4L2_RADIO_SCAN_STOP == Scan->mode)
    {
        result = tea599x_fm_StopScan();
        tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
        /* Clear the flag, just in case there is timeout in scan, application can clear this flag also */
        tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
    }
    else 
        result = -EINVAL;

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_HW_FREQ_SCAN(struct file *file, void *fh, struct v4l2_hw_freq_scan *v)
{
    int result;
    DBG_FUNC_ENTRY;

    DBG("Status[%d] scan_start[%d]", tea599x_FmrData.v_ScanStatus, v->scan_start);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    if(v->scan_start)
    {
        if ( tea599x_FmrData.v_ScanStatus == TEA599X_FMR_SCAN_IN_PROGRESS )
            result = -EBUSY;
        else
        {
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_IN_PROGRESS;
            result = tea599x_fm_StartScan();
        }
    }
    else
    {
        result = tea599x_fm_StopScan();
        tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
        tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_SCAN(void * arg)
{
    struct v4l2_radio_scan_status* ScanStat = arg;
    int result = 0;
    int index = 0;
    DBG_FUNC_ENTRY;

#ifndef CONFIG_POLL_TIMERS_IN_KERNEL
    /* Check status with FM chip */
    tea599x_fm_ScanWorkCb((struct work_struct *)NULL);
#endif

    DBG("tea599x_FmrData.v_ScanStatus[%d] tea599x_globalEvent[%d]", tea599x_FmrData.v_ScanStatus, tea599x_globalEvent); 

    if ( tea599x_FmrData.v_ScanStatus == TEA599X_FMR_SCAN_IN_PROGRESS)
    {
        /*  Check if seek is finished or not */
        if(TEA599X_EVENT_SCAN_CHANNELS_FOUND == tea599x_globalEvent)
        { 
            /* Scan is finished, Channels found */
            DBG("numChalFound[%d]", tea599x_noOfScanFreq); 
            ScanStat->status = V4L2_RADIO_SCAN_NONE;
            ScanStat->numChannelsFound =  tea599x_noOfScanFreq; 
            while(index < tea599x_noOfScanFreq) 
            {
                ScanStat->freq[index] = HZ_2_V4L2(tea599x_scanFreq[index]);
                ScanStat->rssi[index] = tea599x_scanFreqRssiLevel[index];
                DBG("Freq[%09d] RSSI[%05d]", (int)tea599x_scanFreq[index], (int)tea599x_scanFreqRssiLevel[index]); 
                index++;
            }
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else if(TEA599X_EVENT_SCAN_NO_CHANNEL_FOUND == tea599x_globalEvent) 
        {
            /* Scan finished, no Valid Channels found */
            ScanStat->status = V4L2_RADIO_SCAN_NONE;
            ScanStat->numChannelsFound =  0; 
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else /* Scan is progress */
            ScanStat->status = V4L2_RADIO_SCAN_IN_PROGRESS;
    }
    else
        result = -EINVAL;

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_G_HW_FREQ_SCAN(struct file *file, void *fh, struct v4l2_hw_freq_scan_status *v)
{
    int result;
    int index = 0;
    DBG_FUNC_ENTRY;

    result = tea599x_lock_driver();
    if (result)
        return result;
    
#ifndef CONFIG_POLL_TIMERS_IN_KERNEL
    /* Check status with FM chip */
    tea599x_fm_ScanWorkCb((struct work_struct *)NULL);
#endif

    DBG("tea599x_FmrData.v_ScanStatus[%d] tea599x_globalEvent[%d]", tea599x_FmrData.v_ScanStatus, tea599x_globalEvent); 

    if ( tea599x_FmrData.v_ScanStatus == TEA599X_FMR_SCAN_IN_PROGRESS)
    {
        /*  Check if seek is finished or not */
        if(TEA599X_EVENT_SCAN_CHANNELS_FOUND == tea599x_globalEvent)
        { 
            /* Scan is finished, Channels found */
            DBG("numChalFound[%d]", tea599x_noOfScanFreq); 
            v->scan_ongoing = false;
            v->numChannelsFound =  tea599x_noOfScanFreq; 
            while(index < tea599x_noOfScanFreq) 
            {
                v->freq[index] = HZ_2_V4L2(tea599x_scanFreq[index]);
                v->rssi[index] = tea599x_scanFreqRssiLevel[index];
                DBG("Freq[%09d] RSSI[%05d]", (int)tea599x_scanFreq[index], (int)tea599x_scanFreqRssiLevel[index]); 
                index++;
            }
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else if(TEA599X_EVENT_SCAN_NO_CHANNEL_FOUND == tea599x_globalEvent) 
        {
            /* Scan finished, no Valid Channels found */
            v->scan_ongoing = false;
            v->numChannelsFound =  0; 
            tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
        }
        else /* Scan is progress */
            v->scan_ongoing = true;
    }
    else
    {
        v->scan_ongoing = false;
        v->numChannelsFound =  0;
        result = 0;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_RADIO_MISC(void * arg)
{
    struct v4l2_radio_misc* misc = arg;
    int result;
    DBG_FUNC_ENTRY;
    DBG("misc->id[%d]", misc->id);

    switch(misc->id)
    {
        case V4L2_RADIO_BAND :
            result = tea599x_fm_SetBand(misc->value);
            if(!result)
                tea599x_FmrData.v_band = misc->value;
            break;
        case V4L2_RADIO_GRID :
            result = tea599x_fm_SetGrid(misc->value);
            if(!result)
                tea599x_FmrData.v_grid = misc->value;
            break;
        case V4L2_RADIO_MODE :
            result = tea599x_fm_Stereo_SetMode(misc->value);
            if(!result)
                tea599x_FmrData.v_mode = misc->value;
            break;
        case V4L2_RADIO_DAC :
            result = tea599x_fm_SetAudioDAC(misc->value);
            if(!result)
                tea599x_FmrData.v_dac = misc->value;
            break;
        default :
            result = -EINVAL;
    }
    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_RADIO_MISC(struct file *file, void *fh, struct v4l2_radio_misc *v)
{
    int result;
    DBG_FUNC_ENTRY;
    DBG("v->id[%d]", v->id);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    switch(v->id)
    {
        case TEA599X_CID_RADIO_BAND :
            result = tea599x_fm_SetBand(v->value);
            if(!result)
                tea599x_FmrData.v_band = v->value;
            break;
        case TEA599X_CID_RADIO_GRID :
            result = tea599x_fm_SetGrid(v->value);
            if(!result)
                tea599x_FmrData.v_grid = v->value;
            break;
        case TEA599X_CID_RADIO_MODE :
            result = tea599x_fm_Stereo_SetMode(v->value);
            if(!result)
                tea599x_FmrData.v_mode = v->value;
            break;
        case TEA599X_CID_RADIO_DAC :
            result = tea599x_fm_SetAudioDAC(v->value);
            if(!result)
                tea599x_FmrData.v_dac = v->value;
            break;
        case TEA599X_CID_RADIO_RSSI_THRESHOLD :
            tea599x_FmrData.v_RssiThreshold = v->value;
            break;
        default :
            result = -EINVAL;
    }
    
    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_RADIO_MISC(void * arg)
{
    struct v4l2_radio_misc* misc = arg;
    int result = 0;
    unsigned short rssi;
/* ACER Bob IH LEE, 2010/04/01, IssueKeys:???, Add get FM product ID { */
    tea599x_version_struct version;
/* } ACER Bob IH LEE, 2010/04/01 */
    DBG_FUNC_ENTRY;
    DBG("misc->id[%d]", misc->id);

    switch(misc->id)
    {
        case V4L2_RADIO_BAND :
            misc->value = tea599x_FmrData.v_band;
            break;
        case V4L2_RADIO_GRID :
            misc->value = tea599x_FmrData.v_grid;
            break;
        case V4L2_RADIO_MODE :
            misc->value = tea599x_FmrData.v_mode;
            break;
        case V4L2_RADIO_RSSI :
            result = tea599x_fm_GetSignalStrength(&rssi);
            misc->value = rssi;
            break;
        case V4L2_RADIO_DAC:
            misc->value = tea599x_FmrData.v_dac;
            break;
/* ACER Bob IH LEE, 2010/04/01, IssueKeys:???, Add get FM product ID { */
        case V4L2_RADIO_VERSION :
            result = tea599x_fm_GetVersion(&version);
            misc->value=version.product_id;
            break;
/* } ACER Bob IH LEE, 2010/04/01 */
        default :
            return -EINVAL;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_G_RADIO_MISC(struct file *file, void *fh, struct v4l2_radio_misc *v)
{
    int result;
    unsigned short rssi;
/* ACER Bob IH LEE, 2010/04/01, IssueKeys:???, Add get FM product ID { */
    tea599x_version_struct version;
/* } ACER Bob IH LEE, 2010/04/01 */
    DBG_FUNC_ENTRY;
    DBG("v->id[%d]", v->id);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    switch(v->id)
    {
        case TEA599X_CID_RADIO_BAND :
            v->value = tea599x_FmrData.v_band;
            break;
        case TEA599X_CID_RADIO_GRID :
            v->value = tea599x_FmrData.v_grid;
            break;
        case TEA599X_CID_RADIO_MODE :
            v->value = tea599x_FmrData.v_mode;
            break;
        case TEA599X_CID_RADIO_RSSI :
            result = tea599x_fm_GetSignalStrength(&rssi);
            v->value = rssi;
            break;
        case TEA599X_CID_RADIO_DAC:
            v->value = tea599x_FmrData.v_dac;
            break;
        case TEA599X_CID_RADIO_RSSI_THRESHOLD :
            v->value = tea599x_FmrData.v_RssiThreshold;
            break;
        case TEA599X_CID_RADIO_ALTFREQ_RSSI:
            result = tea599x_fm_GetAltFreqRssi(V4L2_2_HZ(v->afAltFreq), &rssi);
            v->value = rssi; 
            break;
/* ACER Bob IH LEE, 2010/04/01, IssueKeys:???, Add get FM product ID { */
        case V4L2_RADIO_VERSION :
            result = tea599x_fm_GetVersion(&version);
            v->value=version.product_id;
            break;
/* } ACER Bob IH LEE, 2010/04/01 */
        case V4L2_RADIO_POWERON_DEV_FTM:
/* IdaChiang, 0526, for FM New API FTM {{ */
/* Power On FM device only */
            if( tea599x_FmrData.v_state == TEA599X_FMR_SWITCH_OFF)
            {
                tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
                tea599x_searchFreq = 0;
                tea599x_noOfScanFreq = 0;
                result = tea599x_fm_SwitchOn(TEA599X_US_EU_BAND_FREQ_HI,
                                        TEA599X_FM_BAND_US_EU,
                                        TEA599X_FM_GRID_100,
                                        10,
                                        TEA599X_DAC_ENABLE,
                                        TEA599X_BALANCE_CENTER,
                                        false,
                                        V4L2_TUNER_MODE_MONO);
                if(!result)
                {
                    tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
                    tea599x_FmrData.v_Frequency = HZ_2_V4L2(TEA599X_US_EU_BAND_FREQ_HI);
                    tea599x_FmrData.v_Muted = false;
                    tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                    tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
                    tea599x_FmrData.v_AudioPath = TEA599X_BALANCE_CENTER;
                    tea599x_FmrData.v_dac = TEA599X_DAC_ENABLE;
                    tea599x_FmrData.v_mode = V4L2_TUNER_MODE_MONO;
                    tea599x_FmrData.v_band = TEA599X_FM_BAND_US_EU;
                    tea599x_FmrData.v_grid = TEA599X_FM_GRID_100;
                    tea599x_FmrData.v_Volume = 10;
                    tea599x_FmrData.v_RssiThreshold = TEA599X_SEARCH_STOP_LEVEL;
                }				
            }
/* IdaChiang, 0526, for FM New API FTM }} */						
            break;
        default :
            return -EINVAL;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifndef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_RADIO_STATE(void * arg)
{
    struct v4l2_radio_state* state = arg;
    int result = 0;
    DBG_FUNC_ENTRY;
    DBG("state->state[%d]", state->state);

    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    switch(state->state) 
    {
        case V4L2_RADIO_SWITCH_OFF:
            result = tea599x_fm_SwitchOff();
            if(!result)
            {
                tea599x_FmrData.v_AudioPath = 0;
                tea599x_FmrData.v_dac = 0; /* DAC is off */
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_OFF;
                tea599x_FmrData.v_Frequency = 0;
                tea599x_FmrData.v_Muted = true;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

        case V4L2_RADIO_SWITCH_ON:
            result = tea599x_fm_SwitchOn(state->freq, state->band, state->grid);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
                tea599x_FmrData.v_Frequency = HZ_2_V4L2(state->freq);
                tea599x_FmrData.v_Muted = false;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
                tea599x_FmrData.v_AudioPath = 0;
                tea599x_FmrData.v_dac = 1; /* DAC is on */
                tea599x_FmrData.v_mode = 0;
                tea599x_FmrData.v_band = state->band;
                tea599x_FmrData.v_grid = state->grid;
            }
            break;

        case V4L2_RADIO_STANDBY:
            result = tea599x_fm_SetPowerMode(TEA599X_POWER_ON_TO_STAND_BY);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_STANDBY;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

        case V4L2_RADIO_POWER_UP_FROM_STANDBY:
            result = tea599x_fm_SetPowerMode(TEA599X_STANDBY_TO_POWER_ON);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

         default :
            result = -EINVAL;
            break;
    }

    return result;
}
#else
static int tea599x_ioctl_VIDIOC_S_RADIO_STATE(struct file *file, void *fh, struct v4l2_radio_state *v)
{
    int result;
    DBG_FUNC_ENTRY;
    DBG("v->state[%d]", v->state);

    result = tea599x_lock_driver();
    if (result)
        return result;
    
    tea599x_globalEvent = TEA599X_EVENT_NO_EVENT;
    tea599x_searchFreq = 0;
    tea599x_noOfScanFreq = 0;

    switch(v->state) 
    {
        case TEA599X_RADIO_SWITCH_OFF:
            result = tea599x_fm_SwitchOff();
            if(!result)
            {
                tea599x_FmrData.v_AudioPath = TEA599X_BALANCE_CENTER;
                tea599x_FmrData.v_dac = TEA599X_DAC_DISABLE;
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_OFF;
                tea599x_FmrData.v_Frequency = 0;
                tea599x_FmrData.v_Muted = TEA599X_MUTE_ON;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

        case TEA599X_RADIO_SWITCH_ON:
            result = tea599x_fm_SwitchOn(V4L2_2_HZ(v->freq),
                                        v->band,
                                        v->grid,
                                        v->volume,
                                        v->dac,
                                        v->balance,
                                        v->mute,
                                        v->mode);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
                tea599x_FmrData.v_Frequency = v->freq;
                tea599x_FmrData.v_Muted = v->mute;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
                tea599x_FmrData.v_AudioPath = v->balance;
                tea599x_FmrData.v_dac = v->dac;
                tea599x_FmrData.v_mode = v->mode;
                tea599x_FmrData.v_band = v->band;
                tea599x_FmrData.v_grid = v->grid;
                tea599x_FmrData.v_Volume = v->volume;
                tea599x_FmrData.v_RssiThreshold = TEA599X_SEARCH_STOP_LEVEL;
            }
            break;

        case TEA599X_RADIO_STANDBY:
            result = tea599x_fm_SetPowerMode(TEA599X_POWER_ON_TO_STAND_BY);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_STANDBY;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

        case TEA599X_RADIO_POWER_UP_FROM_STANDBY:
            result = tea599x_fm_SetPowerMode(TEA599X_STANDBY_TO_POWER_ON);
            if(!result)
            {
                tea599x_FmrData.v_state = TEA599X_FMR_SWITCH_ON;
                tea599x_FmrData.v_SeekStatus = TEA599X_FMR_SEEK_NONE;
                tea599x_FmrData.v_ScanStatus = TEA599X_FMR_SCAN_NONE;
            }
            break;

         default :
            result = -EINVAL;
            break;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_RADIO_RDS(struct file *file, void *fh, struct v4l2_radio_rds_ctrl *v)
{
    int result;
    DBG_FUNC_ENTRY;
    DBG("v->turn_on[%d]", v->turn_on);

    result = tea599x_lock_driver();
    if (result)
        return result;

    if (v->turn_on)
    {
        result = tea599x_fm_SwitchRdsState(TEA599X_FMR_RDS_STATE_ON_ENHANCED);
        if (!result)
            tea599x_FmrData.v_rdsState = TEA599X_RDS_ON;
    }
    else
    {
        result = tea599x_fm_SwitchRdsState(TEA599X_FMR_RDS_STATE_OFF);
        if (!result)
            tea599x_FmrData.v_rdsState = TEA599X_RDS_OFF;
    }

    tea599x_unlock_driver();

    return result;
}
#endif

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_G_RADIO_RDS(struct file *file, void *fh, struct v4l2_radio_rds_data *v)
{
    int result;
    struct v4l2_rds_block * blocks;
    DBG_FUNC_ENTRY;

    DBG("v->get_data[%d]", v->get_data);

    result = tea599x_lock_driver();
    if (result)
        return result;

    blocks = NULL;

    v->length = 0;
    v->status = tea599x_FmrData.v_rdsState;

    if (v->get_data)
    {
        unsigned int count = 0;
        result = tea599x_fm_RdsBlocksGetCnt(&count);
        if (count > TEA599X_RDS_MAX_BLOCK_GROUPS)
        {
            result = -EOVERFLOW;
        }
        else if ((!result) && (count != 0))
        {
            int i;
            v->length = count;
            blocks = kmalloc(count * sizeof(struct v4l2_rds_block), GFP_KERNEL);
            if (!(blocks))
            {
                result = -ENOMEM;
            }
            else
            {
                for (i = 0; (i < count) && (!result); i++)
                {
                    result = tea599x_fm_RdsBlocksGetData(&(blocks[i]));
                }
                if (!result)
                {
                    if (copy_to_user(v->data, blocks, count * sizeof(struct v4l2_rds_block)))
                    {
                        result = -ENOBUFS;
                    }
                }
            }
        }
        if (blocks)
        {
            kfree(blocks);
        }
    }
    if (result)
    {
        v->length = 0;
    }

    tea599x_unlock_driver();

    return result;
}
#endif


#ifdef CONFIG_RADIO_TEA599X_NEW_API
static int tea599x_ioctl_VIDIOC_S_RADIO_AF_SWITCH(struct file *file, void *fh, struct v4l2_radio_af_switch * v)
{
    int result;
    DBG_FUNC_ENTRY;

    DBG("v->freq[%ld] v->pi[%d] v->pimask[%X] v->mute_time[%d]", v->freq, v->pi, v->pimask, v->mute_time);

    result = tea599x_lock_driver();
    if (result)
        return result;

    result = tea599x_fm_AfStartSwitch(V4L2_2_HZ(v->freq), v->pi, v->pimask, v->wait_time, 
                                    v->rssi, &(v->conclusion), &(v->newRssi), &(v->newPi));

    tea599x_unlock_driver();

    return result;
}
#endif


#ifndef CONFIG_RADIO_TEA599X_NEW_API
static void tea599x_release_sysfs( struct video_device *dev )
{
    DBG_FUNC_ENTRY;
}
#endif


/* ################################################################################# */
/*                                                                                   */
/* V4L I/F                                                                           */
/*                                                                                   */
/* ################################################################################# */

/* ****************************** STRUCTURES *************************************** */

/* ****************************** PROTOTYPES *************************************** */

static int tea599x_v4l_register(void);
static void tea599x_v4l_unregister(void);


/* ****************************** DATA ********************************************* */

static const struct v4l2_file_operations tea599x_fops = {
    .owner      = THIS_MODULE,
#ifndef CONFIG_RADIO_TEA599X_NEW_API
    .open           = tea599x_open,
    .release        = tea599x_release,
    .read           = tea599x_read,
    .poll           = tea599x_poll,
    .ioctl          = tea599x_ioctl,
#else
    .open       = tea599x_open,
    .release    = tea599x_release,
    .ioctl      = video_ioctl2,
#endif
};

#ifdef CONFIG_RADIO_TEA599X_NEW_API
static const struct v4l2_ioctl_ops tea599x_ioctl_ops = {
    .vidioc_querycap		= tea599x_ioctl_VIDIOC_QUERYCAP,
    .vidioc_queryctrl		= tea599x_ioctl_VIDIOC_QUERYCTRL,
    .vidioc_g_ctrl		= tea599x_ioctl_VIDIOC_G_CTRL,
    .vidioc_s_ctrl		= tea599x_ioctl_VIDIOC_S_CTRL,
    .vidioc_g_audio		= tea599x_ioctl_VIDIOC_G_AUDIO,
    .vidioc_g_tuner		= tea599x_ioctl_VIDIOC_G_TUNER,
    .vidioc_s_tuner		= tea599x_ioctl_VIDIOC_S_TUNER,
    .vidioc_g_frequency		= tea599x_ioctl_VIDIOC_G_FREQUENCY,
    .vidioc_s_frequency		= tea599x_ioctl_VIDIOC_S_FREQUENCY,
    .vidioc_s_hw_freq_seek	= tea599x_ioctl_VIDIOC_S_HW_FREQ_SEEK,
    .vidioc_default		= tea599x_do_ioctl,
};
#endif

static struct video_device tea599x_radio =
{
    .name       = "ST-Ericsson TEA599x Radio",
#ifndef CONFIG_RADIO_TEA599X_NEW_API
    .fops           = &tea599x_fops,
        .minor          = -1,
        .release        = tea599x_release_sysfs,
#else
    .fops       = &tea599x_fops,
    .minor      = -1,
    .release    = video_device_release,
    .ioctl_ops  = &tea599x_ioctl_ops,
#endif
};


/* ****************************** FUNCTIONS **************************************** */

static int tea599x_v4l_register(void)
{
    DBG_FUNC_ENTRY;
    if (video_register_device(&tea599x_radio, VFL_TYPE_RADIO, -1) == -1)
    {
        DBG_ERR("Failed to register device to V4L");
        return -EINVAL;
    }
    return 0;
}

static void tea599x_v4l_unregister(void)
{
    DBG_FUNC_ENTRY;
    video_unregister_device(&tea599x_radio);
}


/* ################################################################################# */
/*                                                                                   */
/* Module management                                                                 */
/*                                                                                   */
/* ################################################################################# */

MODULE_DESCRIPTION("ST-Ericsson TEA599x FM chip driver.");
MODULE_AUTHOR("Copyright (c) ST-Ericsson 2010");
MODULE_LICENSE("GPL");


static int __init tea599x_init(void)
{
    int result = 0;
	DBG_FUNC_ENTRY;
	
	result = tea599x_v4l_register();
    if (result)
    {
        DBG_ERR("Failed to register to V4L: [%d]", result);
        return result;
    }

    result = tea599x_i2c_init();
    if (result)
    {
        DBG_ERR("Failed to register to I2C driver: [%d]", result);
        tea599x_v4l_unregister();
        return result;
    }

    result = tea599x_fm_SetPowerMode(TEA599X_TO_POWER_DOWN);

    return result;
}

static void __exit tea599x_exit(void)
{
	DBG_FUNC_ENTRY;
    tea599x_i2c_close();
    tea599x_v4l_unregister();
}

module_init(tea599x_init);
module_exit(tea599x_exit);

#undef RADIO_TEA599X_C

