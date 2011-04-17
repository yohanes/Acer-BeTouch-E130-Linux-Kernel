
#ifndef OSWARE_SDD_H
#define OSWARE_SDD_H

//SIZE OF THE BUFFER ALLOCATED FOR EACH CHANNEL AT OPENING
#define SDD_MMAP_BUFFER_SIZE (64*1024)

//session type, values must be the same than in vaudio.h
typedef enum{
  SDD_SE_TYPE_PLAYBACK		= 1,
  SDD_SE_TYPE_RECORD		= 2,
  SDD_SE_TYPE_TRANSODING	= 3
}et_sdd_sessionType;

//values must be the same than in vaudio.h
typedef enum{
  SDD_ST_TYPE_INVAL 		= 0,
  SDD_ST_TYPE_FR		= 1,
  SDD_ST_TYPE_HR		= 2,
  SDD_ST_TYPE_EFR		= 3,
  SDD_ST_TYPE_AMR		= 4,
  SDD_ST_TYPE_AMR_WB		= 5,
  SDD_ST_TYPE_G711_A_LAW	= 6,
  SDD_ST_TYPE_G711_U_LAW	= 7,
  SDD_ST_TYPE_G723_1	    = 8,
  SDD_ST_TYPE_MPEG_LAYER1	= 9,
  SDD_ST_TYPE_MPEG_LAYER2	= 10,
  SDD_ST_TYPE_MPEG_LAYER3	= 11,
  SDD_ST_TYPE_MPEG2_AAC_LC	= 12,
  SDD_ST_TYPE_MPEG4_AAC_LC	= 13,
  SDD_ST_TYPE_MPEG4_AAC_LTP	= 14,
  SDD_ST_TYPE_MPEG4_AAC_SC	= 15,
  SDD_ST_TYPE_PCM		= 16,
  SDD_ST_TYPE_ADPCM		= 17,
  SDD_ST_TYPE_MPEG4_AAC_HE	= 18,
  SDD_ST_TYPE_MPEG4_AAC_HE_PS  = 19
}et_sdd_streamType;

//values must be the same than in vaudio.h
typedef enum{
  SDD_ST_SOURCE_MULTIMEDIA	= 0, //in playback data comes from Linux
  SDD_ST_SOURCE_ANALOG_LOOP	= 1,
  SDD_ST_SOURCE_BAI_ANALOG_IN	= 2,
  SDD_ST_SOURCE_BAI_IIS_IN	= 3,
  SDD_ST_SOURCE_DSP_IIS_IN	= 4
}et_sdd_streamSource;

//values must be the same than in vaudio.h
typedef enum{
  SDD_ST_FORMAT_RTP_PAYLOAD	= 0,
  SDD_ST_FORMAT_AMR_IF1_NC	= 1,
  SDD_ST_FORMAT_AMR_IF1_C	= 2,
  SDD_ST_FORMAT_AMR_IF2_NC	= 3,
  SDD_ST_FORMAT_AMR_IF2_C	= 4
}et_sdd_streamFormat;

//values must be the same than in vaudio.h
typedef enum{
  SDD_CH_MONO		 		 		 = 0,
  SDD_CH_STEREO		 		 		 = 1,
  SDD_CH_DUAL		 		 		 = 2,
  SDD_CH_JOINT		 		 		 = 3
}et_sdd_channel;

//values must be the same than in vaudio.h
typedef enum{
  SDD_LOC_LOCAL			= 0,
  SDD_LOC_FAR			= 1
}et_sdd_location;

//values must be the same than in vaudio.h
typedef enum{
  SDD_ORGID_MAIN		= 0,
  SDD_ORGID_SECONDARY		= 1
}et_sdd_organId;

//values must be the same than in vaudio.h
typedef enum{
  SDD_ORGAN_INNER_SPEAKER		= 0,
  SDD_ORGAN_HEADSET_SPEECH_1_CHANNEL	= 1,
  SDD_ORGAN_HEADSET_SPEECH_2_CHANNELS	= 2,
  SDD_ORGAN_HEADSET_STEREO		= 3,
  SDD_ORGAN_ACCESSORY_A			= 4,
  SDD_ORGAN_ACCESSORY_B			= 5,
  SDD_ORGAN_ACCESSORY_C			= 6,
  SDD_ORGAN_ACCESSORY_D			= 7,
  SDD_ORGAN_HANDSFREE_SPEECH		= 8,
  SDD_ORGAN_HANDSFREE_SPEECH_STEREO	= 9,
  SDD_ORGAN_RINGER_ON_LOUDSPEAKER	= 10,
  SDD_ORGAN_RINGER_ON_LOUDSPEAKER_STEREO= 11,
  SDD_ORGAN_BLUETOOTH			= 12,
  SDD_ORGAN_BLUETOOTH_A2DP		= 13,
  SDD_ORGAN_TEST_ORGAN			= 14,
  SDD_ORGAN_NONE			= 15
}et_sdd_organType;

//values must be the same than in vaudio.h
typedef enum{
  SDD_FILTER_EQUALIZER			= 0,
  SDD_FILTER_VOICE_CLARITY		= 1,
  SDD_FILTER_BAD_FRAME_HEAVY_FILTERING	= 2,
  SDD_FILTER_WIDEBAND_SYNTHESIS		= 3
}et_sdd_filterType;

//values must be the same than in vaudio.h
typedef enum{
  SDD_EQUABAND_50HZ		= 0,
  SDD_EQUABAND_205HZ		= 1,
  SDD_EQUABAND_837HZ		= 2,
  SDD_EQUABAND_3427HZ		= 3,
  SDD_EQUABAND_14027HZ		= 4
}et_sdd_equalizerBand;

typedef enum{
  SDD_DRAINTYPE_BLOCKING	= 0,
  SDD_DRAINTYPE_NOT_BLOCKING	= 1
}et_sdd_drainType;

typedef enum{
  SDD_DRAINRES_STOPPED		= 0,
  SDD_DRAINRES_NOT_STOPPED	= 1
}et_sdd_drainResult;

//values must be the same than in osware-vaudio.h
typedef enum{
  SDD_STATE_OPENED	= 1, //after OPEN operation
  SDD_STATE_PREPARED	= 2, //after SDD_IOCTL_PREPARE
  SDD_STATE_STARTED	= 3, //after SDD_IOCTL_START
  SDD_STATE_DRAINING	= 4, //after SDD_IOCTL_DRAIN (while draining)
  SDD_STATE_STOPPED	= 5  //after error, or SDD_IOCTL_DROP or SDD_IOCTL_DRAIN for record devices
}et_sdd_state;

//values must be the same than in vaudio.h
typedef enum{
  SDD_TEST_STOP                      = 0,
  SDD_TEST_FATAL_ERROR               = 1,
  SDD_TEST_UNDERRUN                  = 2,
  SDD_TEST_OVERRUN                   = 3,
  SDD_TEST_MODULE_FATAL_ERROR        = 4,
  SDD_TEST_VT_LIKE_RECORD            = 5, //use vt way of recording for this channel
  SDD_TEST_NORMAL_RECORD             = 6  //normal mulitmedia way of recording
}et_sdd_testType;

typedef struct{
  void* p_mmapArea; //result of mmap function, this buffer will be used to transport data
  unsigned long v_bitRate; //bit rate in bit per second
  unsigned long v_samplingFrequency; //sampling frequency in Hz
  unsigned short v_periodSize; //in bytes, data will be sent to/by RTKE when a period is full
  unsigned short v_pcmSamplingFormat; // bits per sample (8 or 16), used only for PCM
  unsigned char v_channel; //channel numbers (et_sdd_channel)
  unsigned char v_streamType; //(et_sdd_streamType)
  unsigned short v_useless; //padding
}st_sdd_sessionInfos;

typedef struct{
  unsigned short v_type; //session type (et_sdd_sessionType)
  unsigned short v_streamSource; //stream source (et_sdd_streamSource)
  st_sdd_sessionInfos v_playbackInfos;
  st_sdd_sessionInfos v_recordInfos;
}st_sdd_prepare;

typedef struct{
  unsigned char v_type; //in, tell if you want a blocking or a none blocking drain (et_sdd_drainType)
  unsigned char v_result; //out, tell if channel is stopped (et_sdd_drainResult)
  unsigned short v_useless; //padding
}st_sdd_drain;

/* VVR LMSqb40604 ++ */
typedef struct{
  unsigned char v_AllDataArePresentBeforeStart; //in, 1 = yes, 0 = no
  unsigned char v_useless; //padding
  unsigned short v_useless2; //padding
}st_sdd_start;
/* VVR LMSqb40604 -- */

typedef struct{
  unsigned long v_space; //out, available space in mmapped buffer
  unsigned long v_xrun; //out, number of underrun or overrun since last start
  unsigned short v_type; //in, select which channel you want info about (et_sdd_sessionType)
  unsigned short v_useless; //padding
}st_sdd_getAvailableSpace;

typedef struct{
  unsigned long v_pointer; //out, current offset in mmapped buffer
  unsigned long v_xrun; //out, number of underrun or overrun since last start
  unsigned short v_type; //in, select which channel you want info about (et_sdd_sessionType)
  unsigned short v_useless; //padding
}st_sdd_getUserPointer;

typedef struct{
  unsigned long v_newValue; //in, new value of app pointer
  unsigned short v_type; //in, select which channel you want to update (et_sdd_sessionType)
  unsigned short v_useless; //padding
}st_sdd_setUserPointer;

typedef struct{
  unsigned short v_type; //in, select which channel you want to update (et_sdd_sessionType)
  unsigned short v_useless; //padding
}st_sdd_resetUserPointer;

typedef struct{
  unsigned char v_left;
  unsigned char v_right;
  unsigned short v_useless; //padding
}st_sdd_volume;

typedef struct{
  unsigned char v_location; //(et_sdd_location)
  unsigned char v_isMute;
  unsigned short v_useless; //padding
}st_sdd_muteStatus;

typedef struct{
  unsigned char v_organId; // (et_sdd_organId)
  unsigned char v_organType; // (et_sdd_organType)
  unsigned short v_useless; //padding
}st_sdd_organ;

typedef struct{
  unsigned short v_filterType; // (et_sdd_filterType)
  unsigned char v_isActive;
  unsigned char v_useless; //padding
}st_sdd_filterStatus;

typedef struct{
  unsigned short v_equalizerBand; // (et_sdd_equalizerBand)
  unsigned short v_value;
}st_sdd_equalizer;

typedef struct{
  unsigned long v_bitRate; //bit rate in bit per second
  unsigned long v_samplingFrequency; //sampling frequency in Hz
  void* p_playbackMmapArea; //result of mmap function, this buffer will be used to transport playback data
  void* p_recordMmapArea; //result of mmap function, this buffer will be used to transport record data
  unsigned short v_periodSize; //in bytes, data will be sent to/by RTKE when a period is full
  unsigned char v_channel; //channel numbers (et_sdd_channel)
  unsigned char v_streamType;  // (et_sdd_streamType)
  unsigned char v_streamFormat; // (et_sdd_streamFormat)
  unsigned char v_useless; //padding
  unsigned short v_useless2; //padding
}st_sdd_upgrade;

typedef struct{
  unsigned long v_overrun; //number of underrun since last START
  unsigned long v_underrun; //number of overrun since last START
  unsigned long v_otherErrors; //number of other not fatal decoding errors since last START
  unsigned short v_state; // (et_sdd_state)
  unsigned short v_useless; //padding
}st_sdd_status;

typedef struct{
 unsigned long  v_type; // (et_sdd_testType)
}st_sdd_test;
//send usefull parameters to GAUDIO before starting
#define SDD_IOCTL_PREPARE	_IOW('s', 0x20, st_sdd_prepare)
//start playing
#if 0 /* VVR LMSqb40604 ++ */
#define SDD_IOCTL_START		_IO('s', 0x21)
#else
/* add a parameter in start command */
#define SDD_IOCTL_START		_IOW('s', 0x21, st_sdd_start)
#endif /* VVR LMSqb40604 -- */

//stop playing immediately
#define SDD_IOCTL_DROP		_IO('s', 0x22)
//play until no more data and then stop (blocking or not blocking function)
#define SDD_IOCTL_DRAIN		_IOWR('s', 0x24, st_sdd_drain)
//pause sound
#define SDD_IOCTL_PAUSE		_IO('s', 0x25)
//resume a paused sound
#define SDD_IOCTL_RESUME	_IO('s', 0x26)
//get elapsed time since the beginning of the playback/record in ms
#define SDD_IOCTL_GET_ELAPSED_TIME	_IOR('s', 0x27, unsigned long)
//get the number of bytes that could be written in mmapped buffer
#define SDD_IOCTL_GET_AVAILABLE_SPACE	_IOWR('s', 0x28, st_sdd_getAvailableSpace)
//get user pointer in mmaped buffer
#define SDD_IOCTL_GET_USER_POINTER	_IOWR('s', 0x29, st_sdd_getUserPointer)
//set user pointer in mmaped buffer
#define SDD_IOCTL_SET_USER_POINTER	_IOW('s', 0x2a, st_sdd_setUserPointer)
//get device left and right volumes
#define SDD_IOCTL_GET_VOLUME		_IOR('s', 0x2b, st_sdd_volume)
//set device left and right volumes (output values really set if no error)
#define SDD_IOCTL_SET_VOLUME		_IOWR('s', 0x2c, st_sdd_volume)
//get device mute status. location is important as input parameter
#define SDD_IOCTL_GET_MUTE_STATUS	_IOWR('s', 0x2d, st_sdd_muteStatus)
//set device mute status  (output value really set if no error)
#define SDD_IOCTL_SET_MUTE_STATUS	_IOWR('s', 0x2e, st_sdd_muteStatus)
//get device main or secondary organ. Organ id is important as input parameter
#define SDD_IOCTL_GET_ORGAN		_IOWR('s', 0x2f, st_sdd_organ)
//set device main or secondary organ  (output value really set if no error)
#define SDD_IOCTL_SET_ORGAN		_IOWR('s', 0x30, st_sdd_organ)
//get filter status (on or off)
#define SDD_IOCTL_GET_FILTER_STATUS	_IOWR('s', 0x31, st_sdd_filterStatus)
//set filter status (on or off)
#define SDD_IOCTL_SET_FILTER_STATUS	_IOWR('s', 0x32, st_sdd_filterStatus)
//get equalizer gain
#define SDD_IOCTL_GET_EQUALIZER_GAIN	_IOWR('s', 0x33, st_sdd_equalizer)
//set equalizer gain
#define SDD_IOCTL_SET_EQUALIZER_GAIN	_IOWR('s', 0x34, st_sdd_equalizer)
//only on speech device, upgrade to Video Telephony call
#define SDD_IOCTL_UPGRADE		_IOW('s', 0x35, st_sdd_upgrade)
//only for debug, printnk current state
#define SDD_IOCTL_STATUS		_IOR('s', 0x36, st_sdd_status)
//reset user pointer to begining of buffer (to avoid writing less than one period because there is not enough space at the end of buffer)
//usable only in playback
#define SDD_IOCTL_RESET_USER_POINTER	_IOW('s', 0x37, st_sdd_resetUserPointer)
//only for test. Cmd done on gaudio directly
#define SDD_IOCTL_TEST		 	 _IOW('s', 0x38, st_sdd_test)
#define SDD_IOCTL_WAIT_FOR_DATA_CONSUMPTION		_IO('s', 0x39)

#endif //OSWARE_SDD_H
