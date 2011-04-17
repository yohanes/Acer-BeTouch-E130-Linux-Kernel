/**
  * @file boottime.h
  * @brief Linux boot time information
  * @author (C) 2010, ST-Ericsson
  * @par Contributor:
  * Emeric Vigier
  */

#ifndef BOOTTIME_H
#define BOOTTIME_H

/**
 * @define BOOTTIME_DEVICE_NAME
 * @brief device name
 */
#define BOOTTIME_DEVICE_NAME "boottime"

/**
 * @define BOOTTIME_MAJOR
 * @brief major of device
 */
#define BOOTTIME_MAJOR 0
#define BOOTTIME_MINOR 0

#define BOOTTIME_BUFFER_SIZE	16
#define BOOTTIME_NUM_REQUESTS	16

enum boottime_state {
	BT_STATE_NEW_DEVICE,
	BT_STATE_SETUP_COMPLETE,
	BT_STATE_CREATED
};

struct boottime_device {
	/* EVI: huge struct defined in <linux/input.h> */
	struct input_dev	*dev;
	enum boottime_state	state;
	wait_queue_head_t	waitq;
	unsigned char		ready;
	unsigned char		head;
	unsigned char		tail;
	struct input_event	buff[BOOTTIME_BUFFER_SIZE];
	int			ff_effects_max;

	struct uinput_request	*requests[BOOTTIME_NUM_REQUESTS];
	wait_queue_head_t	requests_waitq;
};

/* Tricky data structure embedding cdev struct that is of interest for us here */
struct boottime_dev_t {
	enum boottime_state	state;
    unsigned long size;			/* amount of data stored here */
    struct cdev cdev;			/* Char device structure      */
};

#if 0
/**
  * @brief
  *
  * @param boottime_id
  *
  * @return 0 successfull, else it means that boottime_id is not registered
  */
int boottime_enable(unsigned long boottime_id);

/**
  * @brief
  *
  * @param boottime_id
  *
  * @return 0 successfull, else it means that boottime_id is not registered
  */

int boottime_disable(unsigned long boottime_id);

/**
  * @brief
  *
  * @param boottime_id
  *
  * @return 0 if succesfull, else it means that boottime_id is already registered
  */
int boottime_register(unsigned long boottime_id);

/**
  * @brief
  *
  * @param boottime_id
  *
  * @return always  0
  */
int boottime_unregister(unsigned long boottime_id);

/**
  * @brief
  *
  * @param boottime_id
  *
  * @return 1 if boottime_id allows boottime,
  *         0 if boottime_id forbids boottime,
  *         -1 if boottime_id not registered
  */
int boottime_state(unsigned long boottime_id);

/**
  * @brief
  *
  * @return 1 if linux allows boottime
  *         0 if linux forbids boottime
  */
int boottime_linux_state(void);
#endif

#endif


