/**
 *
 * Defines needed to interface TEA599X driver. This file is included
 * in radio-tea599x.c, and has to be by any application which want 
 * to use TEA599x driver.
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

#ifndef _RADIO_TEA599X_H
#define _RADIO_TEA599X_H
#ifdef CONFIG_RADIO_TEA599X_NEW_API

#define TEA599X_RDS_MAX_BLOCK_GROUPS    22

enum
{
    TEA599X_RADIO_SWITCH_OFF,
    TEA599X_RADIO_SWITCH_ON,
    TEA599X_RADIO_STANDBY,
    TEA599X_RADIO_POWER_UP_FROM_STANDBY
};

enum
{
    TEA599X_CID_RADIO_BAND,
    TEA599X_CID_RADIO_GRID,
    TEA599X_CID_RADIO_MODE,
    TEA599X_CID_RADIO_RSSI,
    TEA599X_CID_RADIO_DAC,
    TEA599X_CID_RADIO_RSSI_THRESHOLD,
    TEA599X_CID_RADIO_ALTFREQ_RSSI
};

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

typedef enum
{
    TEA599X_STEREO_MODE_STEREO     = 0,
    TEA599X_STEREO_MODE_FORCE_MONO = 1,
} tea599x_stereo_mode_enum;

typedef enum
{
    TEA599X_DAC_DISABLE = 0,
    TEA599X_DAC_ENABLE = 1,
    TEA599X_DAC_RIGHT_MUTE = 2,
    TEA599X_DAC_LEFT_MUTE = 3
} tea599x_power_state;

typedef enum
{
    TEA599X_BALANCE_CENTER = 10,
    TEA599X_BALANCE_LEFT   = 0,
    TEA599X_BALANCE_RIGHT  = 20
} tea599x_balance_mode;

enum
{
    TEA599X_VOLUME_MIN = 0,
    TEA599X_VOLUME_DEF = 5,
    TEA599X_VOLUME_MAX = 20
};

enum
{
    TEA599X_MUTE_OFF = 0,
    TEA599X_MUTE_ON = 1,
};

enum
{
    TEA599X_RDS_OFF = 0,
    TEA599X_RDS_ON = 1
};

enum
{
    TEA599X_AFSWITCH_SUCCEEDED       = 0,
    TEA599X_AFSWITCH_FAIL_LOW_RSSI   = 1,
    TEA599X_AFSWITCH_FAIL_WRONG_PI   = 2,
    TEA599X_AFSWITCH_FAIL_NO_RDS     = 3
} ;

#endif /* CONFIG_RADIO_TEA599X_NEW_API */
#endif /* _RADIO_TEA599X_H */
