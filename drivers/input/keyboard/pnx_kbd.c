/*
 *  linux/drivers/input/keyboard/pnx_kbd.c
 *
 *  Copyright (C) 2008 ST-Ericsson, Le Mans
 *
 *  Restriction key repeat is not managed
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*#define KEYPAD_DEBUG*/  /* Uncomment for verbose debug */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <linux/proc_fs.h>
#include <mach/keypad.h>

//selwyn modified
#include <linux/backlight.h>
#include <mach/gpio.h>
#include <linux/irq.h>
//~selwyn modified

/* clocks */
#include <linux/clk.h>

#include <linux/sysfs.h>
#include <linux/kobject.h>

/* PMU onkey */
#include <linux/pcf506XX.h>

#define ACER_L1_K3
#define ACER_L1_CHANGED

#ifdef KEYPAD_DEBUG
#define	dbg(fmt, args...) printk("%s: " fmt, __func__, ## args)
#else
#define	dbg(fmt, args...) do{}while(0)
#endif

//selwyn modified
#ifdef ACER_L1_AU2
#define JB_SE	1
u8 jb_u = 0;
u8 jb_d = 0;
u8 jb_l = 0;
u8 jb_r = 0;
#endif
//~selwyn modified

/* ACER Bright Lee, 2010/8/11, A21.B-2713, remove kernel command line pnx_kbd.mode=1, so set pnx_keypad_mode as 1 by default { */
// int pnx_keypad_mode = 0;
int pnx_keypad_mode = 1;
/* } ACER Bright Lee, 2010/8/11 */
module_param_named ( mode, pnx_keypad_mode, int, S_IRUGO );
MODULE_PARM_DESC(mode, "Normal or TAT mode");

static int pnx_kbd_sysfs_init(struct device * dev);

/* store previous state */
static unsigned long long old_keycode=0;

/* Specify the keyboard matrix size (depending on hardware configuration) */
static u8 sKbd_rows;
static u8 sKbd_cols; /* we have always sKbd_cols <= sKbd_rows */

static struct clk *sKbd_clk;

/* standard linux input device handle */
static struct input_dev *sKeypad_dev;

static struct input_dev *sKeypad_raw_dev;

#define MAX_ROWS		8
/* ACER Jen chang, 2010/01/21, IssueKeys:AU4.B-2526, Add for entering forcing ADB by some special combinative keys manually { */
#define KEY_FORCE_ADB 500
/* } ACER Jen Chang, 2010/01/21*/
//selwyn modified
static struct backlight_device *buttonbl;
#if (defined ACER_L1_AU2) || (defined ACER_L1_K3)
static struct backlight_device *jogballbl;
#endif
//~selwyn modified

/* ACER Jen chang, 2010/04/11, IssueKeys:AU21.B-1, Modify keypad getting gpio way for K2/K3 PR2 { */
//selwyn modified for AUX
int keypad_gpio_init(struct pnx_kbd_platform_data *pdata)
{
	int err;
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if defined (ACER_AU2_PR1) || defined (ACER_AU2_PR2) || defined (ACER_L1_AU3)
	//Key_led_ctrl--GPIOA3
	err = pnx_gpio_request(pdata->led1_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err1;
	}

	pnx_gpio_set_mode(pdata->led1_gpio, pdata->led1_pin_mux);
	pnx_gpio_set_direction(pdata->led1_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led1_gpio, 0);

#elif (defined ACER_L1_AU2)
	//led_ctrl1--LED1(green)
	err = pnx_gpio_request(pdata->led1_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err1;
	}

	pnx_gpio_set_mode(pdata->led1_gpio, pdata->led1_pin_mux);
	pnx_gpio_set_direction(pdata->led1_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led1_gpio, 0);

	//led_ctrl2--LED2(orange)
	err = pnx_gpio_request(pdata->led2_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err2;
	}

	pnx_gpio_set_mode(pdata->led2_gpio, pdata->led2_pin_mux);
	pnx_gpio_set_direction(pdata->led2_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led2_gpio, 0);

	//led_ctrl3--LED3(white)
	err = pnx_gpio_request(pdata->led3_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err3;
	}

	pnx_gpio_set_mode(pdata->led3_gpio, pdata->led3_pin_mux);
	pnx_gpio_set_direction(pdata->led3_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led3_gpio, 0);

	//led_ctrl4--LED4(keybad led)
	err = pnx_gpio_request(pdata->led4_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err4;
	}

	pnx_gpio_set_mode(pdata->led4_gpio, pdata->led4_pin_mux);
	pnx_gpio_set_direction(pdata->led4_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led4_gpio, 0);

#elif (defined ACER_K3_PR1)
	//led_ctrl1--LED1(green)
	err = pnx_gpio_request(pdata->led1_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err1;
	}

	pnx_gpio_set_mode(pdata->led1_gpio, pdata->led1_pin_mux);
	pnx_gpio_set_direction(pdata->led1_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led1_gpio, 0);

	//led_ctrl2--LED2(orange)
	err = pnx_gpio_request(pdata->led2_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err2;
	}

	pnx_gpio_set_mode(pdata->led2_gpio, pdata->led2_pin_mux);
	pnx_gpio_set_direction(pdata->led2_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led2_gpio, 0);

	//led_ctrl3--LED3(white)
	err = pnx_gpio_request(pdata->led3_gpio);
	if (err)
	{
		printk(KERN_WARNING "Can't get gpio for keypad led\n");
		goto err3;
	}

	pnx_gpio_set_mode(pdata->led3_gpio, pdata->led3_pin_mux);
	pnx_gpio_set_direction(pdata->led3_gpio, GPIO_DIR_OUTPUT);
	pnx_gpio_write_pin(pdata->led3_gpio, 0);
	#endif

	return 0;

#if (defined ACER_L1_AU2)
err4:
	pnx_gpio_free(pdata->led3_gpio);
#endif
#if (defined ACER_L1_AU2) || (defined ACER_K3_PR1)
err3:
	pnx_gpio_free(pdata->led2_gpio);
err2:
	pnx_gpio_free(pdata->led1_gpio);
#endif
err1:
	return err;
}
/* } ACER Jen Chang, 2010/04/11*/

/*ACER Erace.Ma@20091026, hall mouse use pnx_kdb to send key event */
#if defined(CONFIG_SPI_HALL_MOUSE) && !defined(HALLMOUSE_INPUT_DEVICE)
void pnx_kbd_signal(unsigned  int key_id, int pressed)
{
	dbg("key %d %s, spi hall mouse\n", key_id, (pressed)?("pressed"):("released"));
	if ( pressed )
	{
		input_report_key(sKeypad_dev, key_id, 1);
	}
	else
	{
		input_report_key(sKeypad_dev, key_id, 0);
	}
}
#endif
/*ACER Erace.Ma@20091026*/
//~selwyn modified

static void pnx_kbd_onkeypmu(u_int16_t pressed)
{
	if ( pressed )
	{
		input_report_key(sKeypad_dev, KEY_STOP, 1);
	}
	else
	{
		input_report_key(sKeypad_dev, KEY_STOP, 0);
	}
}

static u64 pnx_kbd_readmatrix(void)
{
	int row;
	unsigned long long keycode = 0;
	u8 shift = 0;
	u32 mask = (1<<sKbd_cols) - 1;

	for ( row=0; row<sKbd_rows; row++) {
		keycode |= (u64)((u32)readl(KBS_DATAx(row)) & mask) << shift;
		shift += sKbd_rows;
	}
	return keycode;
}

static inline int pnx_kbd_find_key(int *sKeymap, int col, int row)
{
	int i, key;

	key = KEY(col, row, 0);
	for (i = 0; sKeymap[i] != 0; i++)
		if ((sKeymap[i] & 0xff000000) == key)
			return sKeymap[i] & 0x00ffffff;
	return -1;
}

static int pnx_kbs_interrupt( int irq, void *dev_id )
{
	int row,col;
	unsigned long long keycode = 0;
	/* ACER Jen chang, 2010/01/21, IssueKeys:AU4.FC-150/AU4.B-2526, Add for entering ram dump/forcing ADB by some special combinative keys manually { */
	#ifdef ACER_DEBUG_HOTKEY
	static unsigned int combine_key = 1;
	static unsigned int combine_adb_key = 1;
	#if defined (ACER_L1_AU4) || defined (ACER_L1_K2)|| defined (ACER_L1_AS1)
/*ACER, IdaChiang, 20100628, modify K2 row and col value { */
	static unsigned long long key_array[] = {0x20, 0x28, 0x30, 0x21, 0x24}; //send, up, down, left, right
	static unsigned long long key_adb_array[] = {0x20, 0x21, 0x21, 0x24, 0x24}; //send, left, left, right, right
/* } ACER, IdaChiang, 20100628, modify K2 row and col value */
	#elif defined (ACER_L1_AU2) || defined (ACER_L1_K3)
	static unsigned long long key_array[] = {0x200, 0x208, 0x600, 0x20200, 0x204}; //send, 1, 2, 3, 4
	static unsigned long long key_adb_array[] = {0x200, 0x600, 0x600, 0x204, 0x204}; //send, 2, 2, 4, 4
	#else
	static unsigned long long key_array[] = {0x1000000000, 0x1000000008, 0x1000000400, 0x1000020000, 0x1000000004}; //send, 1, 2, 3, 4
	static unsigned long long key_adb_array[] = {0x1000000000, 0x1000000400, 0x1000000400, 0x1000000004, 0x1000000004}; //send, 2, 2, 4, 4
	#endif
	#endif /* ACER_DEBUG_HOTKEY */
	/* } ACER Jen Chang, 2010/01/21*/

	u32 tmp_row, old_row, status;
	u32 mask = (1<<sKbd_rows)-1;

	keycode =  pnx_kbd_readmatrix();

	/* ACER Jen chang, 2010/01/21, IssueKeys:AU4.FC-150/AU4.B-2526, Add for entering ram dump/forcing ADB by some special combinative keys manually { */
	#ifdef ACER_DEBUG_HOTKEY
	if (keycode == key_array[combine_key]) // force ramdump ->AU4.FC-150
	{
		combine_key ++;
		if (combine_key >= (sizeof(key_array) / sizeof(long long)))
			panic("--manual panic--");
	}
	else if(keycode == key_adb_array[combine_adb_key]) // force ADB ->AU4.B-2526
	{
		combine_adb_key ++;
		if (combine_adb_key >= (sizeof(key_array) / sizeof(long long)))
		{
			set_bit(KEY_FORCE_ADB, sKeypad_dev->keybit);
			input_report_key( sKeypad_dev, KEY_FORCE_ADB, 1);
			printk("--force change to adb--\n");
			combine_adb_key = 1;
			input_report_key( sKeypad_dev, KEY_FORCE_ADB, 0);
		}
	}
	else if (keycode != key_array[0]) //send key press
	{
		combine_key = 1;
		combine_adb_key = 1;
	}
	#endif /* ACER_DEBUG_HOTKEY */
	/* } ACER Jen Chang, 2010/01/21*/

	/* check if there are some changes */
	if(keycode != old_keycode)
	{
		for ( row=0; row<sKbd_rows; row++)
		{
			/* analyse row by row */
			tmp_row = (keycode >> (row * sKbd_rows)) & mask;
			old_row = (old_keycode >> (row * sKbd_rows)) & mask;

			status = tmp_row ^ old_row;

			for(col=0; col<sKbd_cols; col++)
			{
				if(status & (1 << col))
				{
					int key = pnx_kbd_find_key(sKeypad_dev->keycode, col, row);
					if (key < 0)
					{
						printk(KERN_WARNING "pnx_keypad: Spurious key event row=%d-col=%d\n", row, col);
					}
					else
					{
						/* One change report it */
						input_report_key( sKeypad_dev, key, (tmp_row & (1<<col)) ? 1 : 0 );
						dbg("key %d %s, row=%d col=%d\n",key,
						  (tmp_row&(1<<col)) ? "pressed" : "released", row, col);
					}
				}
			}
		}
		/* save current status */
		old_keycode = keycode;
	}

	if(pnx_keypad_mode)
		input_event(sKeypad_raw_dev, EV_MSC, MSC_RAW, keycode);
	//	dbg("keycode %16llx\n",keycode);

	/* acknowledge IT in KBS block */
	writel(1,KBS_IT_REG);

	return IRQ_HANDLED;
}


#ifdef CONFIG_PM
static int pnx_kbd_suspend(struct platform_device *dev, pm_message_t state)
{
	/* Nothing yet */

	return 0;
}

static int pnx_kbd_resume(struct platform_device *dev)
{
	/* Nothing yet */

	return 0;
}
#else
#define pnx_kbd_suspend	NULL
#define pnx_kbd_resume	NULL
#endif

//selwyn modified
#ifdef ACER_L1_AU2
static void jb_clear(void)
{
	jb_l=0;
	jb_r=0;
	jb_d=0;
	jb_u=0;
}

static int jogball_irq_left(int irq, void *dev_id)
{
	disable_irq(irq);
	jb_l++;
	if(jb_l > JB_SE)
	{
	input_report_key( sKeypad_dev, KEY_LEFT, 1 );
	input_report_key( sKeypad_dev, KEY_LEFT, 0 );
		jb_clear();
	dbg("jogball_irq_left is called!\n");
	}
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int jogball_irq_down(int irq, void *dev_id)
{
	disable_irq(irq);
	jb_d++;
	if(jb_d > JB_SE)
	{
	input_report_key( sKeypad_dev, KEY_DOWN, 1 );
	input_report_key( sKeypad_dev, KEY_DOWN, 0 );
		jb_clear();
	dbg("jogball_irq_down is called!\n");
	}
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int jogball_irq_right(int irq, void *dev_id)
{
	disable_irq(irq);
	jb_r++;
	if(jb_r > JB_SE)
	{
	input_report_key( sKeypad_dev, KEY_RIGHT, 1 );
	input_report_key( sKeypad_dev, KEY_RIGHT, 0 );
		jb_clear();
	dbg("jogball_irq_right is called!\n");
	}
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int jogball_irq_up(int irq, void *dev_id)
{
	disable_irq(irq);
	jb_u++;
	if(jb_u > JB_SE)
	{
	input_report_key( sKeypad_dev, KEY_UP, 1 );
	input_report_key( sKeypad_dev, KEY_UP, 0 );
		jb_clear();
	dbg("jogball_irq_up is called!\n");
	}
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int jogball_irq_select(int irq, void *dev_id)
{
	disable_irq(irq);
	if(pnx_gpio_read_pin(GPIO_A12))
	{
		input_report_key( sKeypad_dev, KEY_SELECT, 0 );
		dbg("jogball_irq_select is called! key release\n");
	}
	else
	{
		input_report_key( sKeypad_dev, KEY_SELECT, 1 );
		dbg("jogball_irq_select is called! key press\n");
	}
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int JogBall_init(void)
{
	int err = 0;
	int pin_l, pin_d, pin_r, pin_u;

	err = pnx_gpio_request(GPIO_A20); /* KEY_LEFT */
	if( err )
	{
		printk("Can't get GPIO_A20 for keypad led\n");
		goto err1;
	}
	err = pnx_gpio_request(GPIO_A13); /* KEY_DOWN */
	if( err )
	{
		printk("Can't get GPIO_A13 for keypad led\n");
		goto err2;
	}
	err = pnx_gpio_request(GPIO_D19); /* KEY_RIGHT */
	if( err )
	{
		printk("Can't get GPIO_D19 for keypad led\n");
		goto err3;
	}
	err = pnx_gpio_request(GPIO_D18); /* KEY_UP */
	if( err )
	{
		printk("Can't get GPIO_D18 for keypad led\n");
		goto err4;
	}
	err = pnx_gpio_request(GPIO_A12); /* KEY_SELECT */
	if( err )
	{
		printk("Can't get GPIO_A12 for keypad led\n");
		goto err5;
	}

	pnx_gpio_set_mode(GPIO_A20, GPIO_MODE_MUX0);
	pnx_gpio_set_mode(GPIO_A13, GPIO_MODE_MUX0);
	pnx_gpio_set_mode(GPIO_D19, GPIO_MODE_MUX1);
	pnx_gpio_set_mode(GPIO_D18, GPIO_MODE_MUX1);
	pnx_gpio_set_mode(GPIO_A12, GPIO_MODE_MUX0);

	pnx_gpio_set_direction(GPIO_A20, GPIO_DIR_INPUT);
	pnx_gpio_set_direction(GPIO_A13, GPIO_DIR_INPUT);
	pnx_gpio_set_direction(GPIO_D19, GPIO_DIR_INPUT);
	pnx_gpio_set_direction(GPIO_D18, GPIO_DIR_INPUT);
	pnx_gpio_set_direction(GPIO_A12, GPIO_DIR_INPUT);

	pin_l = pnx_gpio_read_pin(GPIO_A20);
	pin_d = pnx_gpio_read_pin(GPIO_A13);
	pin_r = pnx_gpio_read_pin(GPIO_D19);
	pin_u = pnx_gpio_read_pin(GPIO_D18);

	set_irq_type(IRQ_EXTINT(14), (pin_l==0)?IRQ_TYPE_EDGE_RISING:IRQ_TYPE_EDGE_FALLING);
	set_irq_type(IRQ_EXTINT(7), (pin_d==0)?IRQ_TYPE_EDGE_RISING:IRQ_TYPE_EDGE_FALLING);
	set_irq_type(IRQ_EXTINT(12), (pin_u==0)?IRQ_TYPE_EDGE_RISING:IRQ_TYPE_EDGE_FALLING);
	set_irq_type(IRQ_EXTINT(22), (pin_u==0)?IRQ_TYPE_EDGE_RISING:IRQ_TYPE_EDGE_FALLING);
	set_irq_type(IRQ_EXTINT(6), IRQ_TYPE_EDGE_BOTH);

	err = request_irq(IRQ_EXTINT(14), &jogball_irq_left, 0, "jogball_left", NULL);
	if(err < 0)
		goto err6;
	err = request_irq(IRQ_EXTINT(7), &jogball_irq_down, 0, "jogball_down", NULL);
	if(err < 0)
		goto err7;
	err = request_irq(IRQ_EXTINT(12), &jogball_irq_right, 0, "jogball_right", NULL);
	if(err < 0)
		goto err8;
	err = request_irq(IRQ_EXTINT(22), &jogball_irq_up, 0, "jogball_up", NULL);
	if(err < 0)
		goto err9;
	err = request_irq(IRQ_EXTINT(6), &jogball_irq_select, 0, "jogball_select", NULL);
	if(err < 0)
		goto err10;

	return 0;

err10:
	free_irq(IRQ_EXTINT(22), &jogball_irq_up);
err9:
	free_irq(IRQ_EXTINT(12), &jogball_irq_right);
err8:
	free_irq(IRQ_EXTINT(7), &jogball_irq_down);
err7:
	free_irq(IRQ_EXTINT(14), &jogball_irq_left);
err6:
	pnx_gpio_free(GPIO_A12);
err5:
	pnx_gpio_free(GPIO_D18);
err4:
	pnx_gpio_free(GPIO_D19);
err3:
	pnx_gpio_free(GPIO_A13);
err2:
	pnx_gpio_free(GPIO_A20);
err1:
	return err;
}
static void JogBall_cleanup(void)
{
	free_irq(IRQ_EXTINT(22), &jogball_irq_up);
	free_irq(IRQ_EXTINT(12), &jogball_irq_right);
	free_irq(IRQ_EXTINT(7), &jogball_irq_down);
	free_irq(IRQ_EXTINT(14), &jogball_irq_left);

	pnx_gpio_free(GPIO_A12);
	pnx_gpio_free(GPIO_D18);
	pnx_gpio_free(GPIO_D19);
	pnx_gpio_free(GPIO_A13);
	pnx_gpio_free(GPIO_A20);
}
#endif

#if (defined ACER_L1_AU2) || (defined ACER_L1_K3)
static int get_jogball_led(struct backlight_device *bd)
{
	u_int8_t intensity = 0;

	intensity = pnx_gpio_read_pin(GPIO_D21);
	if(intensity)
		return 1;
	intensity = pnx_gpio_read_pin(GPIO_D30);
	if(intensity)
		return 2;
	intensity = pnx_gpio_read_pin(GPIO_F8);
	if(intensity)
		return 3;
	else
		return 0;
}

static int set_jogball_led(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if(intensity == 1)
	{
		pnx_gpio_write_pin(GPIO_D21, 1);
	}
	else if(intensity == 2)
	{
		pnx_gpio_write_pin(GPIO_D30, 1);
	}
	else if(intensity == 3)
	{
		pnx_gpio_write_pin(GPIO_F8, 1);
	}
	else
	{
		pnx_gpio_write_pin(GPIO_D21, 0);
		pnx_gpio_write_pin(GPIO_D30, 0);
		pnx_gpio_write_pin(GPIO_F8, 0);
	}

	return 0;
}

static struct backlight_ops jogball_led_ops = {
	.get_brightness	= get_jogball_led,
	.update_status	= set_jogball_led,
};
#endif
//~selwyn modified

//selwyn modified
static int get_btn_led(struct backlight_device *bd)
{
	u_int8_t intensity = 0;

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
	#if defined (ACER_AU2_PR1) || defined (ACER_AU2_PR2) || defined (ACER_L1_AU3)
	intensity = pnx_gpio_read_pin(GPIO_A3);
	#endif
	/* } ACER Jen Chang, 2009/09/02*/

	return intensity;
}

static int set_btn_led(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
	#if defined (ACER_AU2_PR1) || defined (ACER_AU2_PR2) || defined (ACER_L1_AU3)
	if(intensity > 0)
	{
		pnx_gpio_write_pin(GPIO_A3, 1);
	}
	else
	{
		pnx_gpio_write_pin(GPIO_A3, 0);
	}
	#endif
	/* } ACER Jen Chang, 2009/09/02*/

	return 0;
}

static struct backlight_ops button_led_ops = {
	.get_brightness	= get_btn_led,
	.update_status	= set_btn_led,
};
//~selwyn modified

static int __init pnx_kbd_probe(struct platform_device *pdev)
{
	unsigned long long keycode = 0;
	struct pnx_kbd_platform_data *pdata =  pdev->dev.platform_data;
	int irq=0, i, ret=-EINVAL;
	int *sKeymap;
	int row,col;

	u32 tmp_row, old_row, status;
	u32 mask;

	/* ACER Jen chang, 2010/04/11, IssueKeys:AU21.B-1, Modify keypad getting gpio way for K2/K3 PR2 { */
	//selwyn add for AU2
	#if defined (ACER_L1_AU2) || defined (ACER_L1_AU3) || defined (ACER_L1_K3)
	keypad_gpio_init(pdata);
	#endif
	//~selwyn add
	/* } ACER Jen Chang, 2010/04/11*/

	/* Determine the keyboard matrix size (hardware board configuration) */
	sKbd_rows = pdata->rows;
	sKbd_cols = pdata->cols;

	if (!sKbd_rows || !sKbd_cols || !pdata->keymap || sKbd_cols > sKbd_rows ||
		sKbd_rows > MAX_ROWS)
	{
		printk(KERN_ERR "Bad values for rows, cols or keymap from pdata\n");
		return -EINVAL;
	}

	/* Initialize input device info */
	sKeypad_dev = input_allocate_device();
	if (!sKeypad_dev)
	{
		printk(KERN_ERR "Cannot allocate input device for keypad\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, sKeypad_dev);

	/* Get keymap from platform device data */
	sKeymap = pdata->keymap;

	/* setup input device */
	set_bit(EV_KEY, sKeypad_dev->evbit);
	for (i = 0; sKeymap[i] != 0; i++)
		set_bit(sKeymap[i] & KEY_MAX, sKeypad_dev->keybit);
#if defined(ACER_L1_CHANGED)
	//Selwyn marked 20090615
	//set_bit(KEY_STOP, sKeypad_dev->keybit);
	//~Selwyn marked
	//Selwyn modified 20090626 for hall mouse
	#ifdef ACER_L1_AU3
	#ifndef ACER_AU3_PREPR1
	set_bit(KEY_UP, sKeypad_dev->keybit);
	set_bit(KEY_DOWN, sKeypad_dev->keybit);
	set_bit(KEY_RIGHT, sKeypad_dev->keybit);
	set_bit(KEY_LEFT, sKeypad_dev->keybit);
	#endif
	#endif
#else
	set_bit(KEY_STOP, sKeypad_dev->keybit);
#endif
	//~Selwyn modified
	sKeypad_dev->name = "PNX-Keypad";
	sKeypad_dev->phys = "input0";
	sKeypad_dev->dev.parent = &pdev->dev;

	sKeypad_dev->id.bustype = BUS_HOST;
	sKeypad_dev->id.vendor = 0x0123;
	sKeypad_dev->id.product = 0x5220 /*dummy value*/;
	sKeypad_dev->id.version = 0x0100;

	sKeypad_dev->keycode = sKeymap;
	sKeypad_dev->keycodesize = sizeof(unsigned int);
	sKeypad_dev->keycodemax = pdata->keymapsize;

	/* Register linux input device */
	if (input_register_device(sKeypad_dev) < 0)
	{
		printk(KERN_ERR "Unable to register PNX-keypad input device\n");
		goto err1;
	}

	#if defined(ACER_L1_CHANGED)
	//Selwyn move here to prevent kernel panic problem
	/* RAW mode for TAT */
	if(pnx_keypad_mode)
	{
		/* Initialize input device info */
		sKeypad_raw_dev = input_allocate_device();
		if (!sKeypad_raw_dev)
		{
			goto err4;
		}

		platform_set_drvdata(pdev, sKeypad_raw_dev);

		/* Get keymap from platform device data */
		sKeymap = pdata->keymap;

		/* setup input device */
		set_bit(EV_MSC, sKeypad_raw_dev->evbit);
		for (i = 0; sKeymap[i] != 0; i++)
			set_bit(sKeymap[i] & KEY_MAX, sKeypad_raw_dev->keybit);
		set_bit(MSC_RAW, sKeypad_raw_dev->mscbit);
		sKeypad_raw_dev->name = "PNX-RAW-Keypad";
		sKeypad_raw_dev->phys = "input1";
		sKeypad_raw_dev->dev.parent = &pdev->dev;

		sKeypad_raw_dev->id.bustype = BUS_HOST;
		sKeypad_raw_dev->id.vendor = 0x0123;
		sKeypad_raw_dev->id.product = 0x5220 /*dummy value*/;
		sKeypad_raw_dev->id.version = 0x0100;

		sKeypad_raw_dev->keycode = sKeymap;
		sKeypad_raw_dev->keycodesize = sizeof(unsigned int);
		sKeypad_raw_dev->keycodemax = pdata->keymapsize;

		/* Register linux input device */
		if (input_register_device(sKeypad_raw_dev) < 0)
		{
			printk(KERN_ERR "Unable to register PNX-keypad input device\n");
			goto err5;
		}
	}
	/* End RAW mode */
	//~Selwyn move here
	#endif

	/* set kbsEn in clock gating */
	sKbd_clk = clk_get(NULL,"KBS");
	if (IS_ERR(sKbd_clk))
	{
		printk(KERN_WARNING "input: PNX keypad driver can't get clock !!!\n");
		ret = -EBUSY;
		goto err2;
	}
	clk_enable(sKbd_clk);

	/* hardware debouncing (in ms) */
	writel(32*pdata->dbounce_delay/sKbd_rows, KBS_DEB_REG);
	/* set keyboard matrix size */
	writel(sKbd_rows, KBS_MATRIX_DIM_REG);
	/* scan interval = 10ms, don't exists anymore */
	/* writel(10,KBS_SCAN_REG); */
	/* Clear pending interrupt */
	writel(1, KBS_IT_REG);

#if defined(ACER_L1_CHANGED)
	/* get keyboard IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq == 0) goto err1;
	if(request_irq( irq, &pnx_kbs_interrupt, IRQF_DISABLED, "PNX KBS", NULL))
	{
		printk(KERN_WARNING "input: PNX keypad driver can't register IRQ !!!\n");
		goto err1;
	}
#else
	/* RAW mode for TAT */
	if(pnx_keypad_mode)
	{
		/* Initialize input device info */
		sKeypad_raw_dev = input_allocate_device();
		if (!sKeypad_raw_dev)
		{
			printk(KERN_ERR "Cannot allocate input device for keypad_raw TAT\n");
			goto err3;
		}

		platform_set_drvdata(pdev, sKeypad_raw_dev);

		/* Get keymap from platform device data */
		sKeymap = pdata->keymap;

		/* setup input device */
		set_bit(EV_MSC, sKeypad_raw_dev->evbit);
		for (i = 0; sKeymap[i] != 0; i++)
			set_bit(sKeymap[i] & KEY_MAX, sKeypad_raw_dev->keybit);
		set_bit(MSC_RAW, sKeypad_raw_dev->mscbit);
		sKeypad_raw_dev->name = "PNX-RAW-Keypad";
		sKeypad_raw_dev->phys = "input1";
		sKeypad_raw_dev->dev.parent = &pdev->dev;

		sKeypad_raw_dev->id.bustype = BUS_HOST;
		sKeypad_raw_dev->id.vendor = 0x0123;
		sKeypad_raw_dev->id.product = 0x5220 /*dummy value*/;
		sKeypad_raw_dev->id.version = 0x0100;

		sKeypad_raw_dev->keycode = sKeymap;
		sKeypad_raw_dev->keycodesize = sizeof(unsigned int);
		sKeypad_raw_dev->keycodemax = pdata->keymapsize;

		/* Register linux input device */
		if (input_register_device(sKeypad_raw_dev) < 0)
		{
			printk(KERN_ERR "Unable to register PNX-keypad raw TAT input device\n");
			goto err4;
		}
	}
	/* End RAW mode */

	/* Register Onkey function */
    pcf506XX_registerOnkeyFct(pnx_kbd_onkeypmu);
#endif

	/* scan current status and enable interrupt */
	keycode = pnx_kbd_readmatrix();
	mask = (1<<sKbd_rows)-1;
	/* check if there are some changes */
	if(keycode != old_keycode)
	{
		for ( row=0; row<sKbd_rows; row++)
		{
			/* analyse row by row */
			tmp_row = (keycode >> (row * sKbd_rows)) & mask;
			old_row = (old_keycode >> (row * sKbd_rows)) & mask;

			status = tmp_row ^ old_row;

			for(col=0; col<sKbd_cols; col++)
			{
				if(status & (1 << col))
				{
					int key = pnx_kbd_find_key(sKeypad_dev->keycode, col, row);
					if (key < 0)
					{
						printk(KERN_WARNING "pnx_keypad: Spurious key event row=%d-col=%d\n", row, col);
					}
					else
					{
						/* One change report it */
						input_report_key( sKeypad_dev, key, (tmp_row & (1<<col)) ? 1 : 0 );
						dbg("key %d %s, row=%d col=%d\n",key,
						  (tmp_row&(1<<col)) ? "pressed" : "released", row, col);
					}
				}
			}
		}
		/* save current status */
		old_keycode = keycode;
	}



/*ACER Erace.Ma@20091026, hall mouse use pnx_kdb to send key event */
#if defined(CONFIG_SPI_HALL_MOUSE) && !defined(HALLMOUSE_INPUT_DEVICE)
    spi_hallmouse_registerSignalFct(pnx_kbd_signal);
#endif
/*ACER Erace.Ma@20091026*/
	if(pnx_keypad_mode)
	{
		if(pnx_kbd_sysfs_init(&(sKeypad_raw_dev->dev)) != 0)
		{
			printk(KERN_ERR "Unable to register PNX-keypad SYSFS files\n");
		}
	}

	//selwyn modified
	#ifdef ACER_L1_AU2
	if( JogBall_init() < 0)
	{
		printk(KERN_ERR "Unable to configure JogBall input device\n");
		goto err6;
	}
	#endif

	#if (defined ACER_L1_AU2) || (defined ACER_L1_K3)
	jogballbl = backlight_device_register("jogball-bl",
						    NULL,
						    NULL,
						    &jogball_led_ops);
	if (IS_ERR(jogballbl))
	{
		printk(KERN_ERR "Unable to register BL jogball led device\n");
		goto err7;
	}

	jogballbl->props.max_brightness = 3;
	jogballbl->props.power = 0;
	jogballbl->props.fb_blank = 0;

	jogballbl->props.brightness = 0;
	#endif
	//selwyn modified

#if defined(ACER_L1_CHANGED)
/* ACER Jen chang, 2009/09/02, IssueKeys:AU2.FC-201, Modify sd card detecting to fix pmu ic bug on PCR { */
#if (defined ACER_AU2_PCR) || (defined ACER_L1_K3)
	buttonbl = backlight_device_register("keypad-bl",
						    NULL,
						    NULL,
						    &button_led_ops);
#else
	buttonbl = backlight_device_register("button-bl",
						    NULL,
						    NULL,
						    &button_led_ops);
#endif
/* } ACER Jen Chang, 2009/09/02*/
	if (IS_ERR(buttonbl))
	{
		printk(KERN_ERR "Unable to register BL button led device\n");
		goto err8;
	}

	buttonbl->props.max_brightness = 1;
	buttonbl->props.power = 0;
	buttonbl->props.fb_blank = 0;

	buttonbl->props.brightness = 0;


	return 0;

err8:
#if defined (ACER_L1_AU2) || defined (ACER_L1_K3)
	backlight_device_unregister(jogballbl);
#endif
err7:
#ifdef ACER_L1_AU2
	JogBall_cleanup();
#endif
err6:
	input_unregister_device(sKeypad_raw_dev);
/*ACER Erace.Ma@20091026, hall mouse use pnx_kdb to send key event */
#if defined(CONFIG_SPI_HALL_MOUSE) && !defined(HALLMOUSE_INPUT_DEVICE)
	spi_hallmouse_unregisterSignalFct();
#endif
/*ACER Erace.Ma@20091026*/
	pcf506XX_unregisterOnkeyFct();
#else
	/* get keyboard IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq == 0) goto err1;
	if(request_irq( irq, &pnx_kbs_interrupt, IRQF_DISABLED, "PNX KBS", NULL))
	{
		printk(KERN_WARNING "input: PNX keypad driver can't register IRQ !!!\n");
		goto err5;
	}

	return 0;
#endif

err5:
	pcf506XX_unregisterOnkeyFct();

	if(pnx_keypad_mode)
		input_unregister_device(sKeypad_raw_dev);

err4:
	if(pnx_keypad_mode)
		input_free_device(sKeypad_raw_dev);

err3:
	/* reset kbsEn in clock gating */
	clk_disable(sKbd_clk);
	clk_put(sKbd_clk);

err2:
	input_unregister_device(sKeypad_dev);

err1:
	input_free_device(sKeypad_dev);

	return ret;
}

static int pnx_kbd_remove(struct platform_device *pdev)
{
	/* Clear pending interrupt */
	writel(1, KBS_IT_REG);

	/* reset kbsEn in clock gating */
	clk_disable(sKbd_clk);
	clk_put(sKbd_clk);

	free_irq( platform_get_irq(pdev, 0), &pnx_kbs_interrupt );

	/* Unregister Onkey function */
#if !defined(ACER_L1_CHANGED)
	pcf506XX_unregisterOnkeyFct();
#endif

/*ACER Erace.Ma@20091026, hall mouse use pnx_kdb to send key event */
#if defined(CONFIG_SPI_HALL_MOUSE) && !defined(HALLMOUSE_INPUT_DEVICE)
	spi_hallmouse_unregisterSignalFct();
#endif
/*ACER Erace.Ma@20091026*/

#if defined (ACER_L1_AU2) || defined (ACER_L1_K3)
	backlight_device_unregister(jogballbl);
#endif
#ifdef ACER_L1_AU2
	JogBall_cleanup();
#endif

#if defined(ACER_L1_CHANGED)
	backlight_device_unregister(buttonbl);
#endif

	/* Delete input device */
	input_unregister_device(sKeypad_dev);
	input_free_device(sKeypad_dev);
	/* Delete raw input device */
	input_unregister_device(sKeypad_raw_dev);
	input_free_device(sKeypad_raw_dev);

	return 0;
}

static struct platform_driver pnx_kbd_driver = {
	.probe		= pnx_kbd_probe,
	.remove		= pnx_kbd_remove,
	.suspend	= pnx_kbd_suspend,
	.resume		= pnx_kbd_resume,
	.driver		= {
		.name	= "pnx-keypad",
	},
};

static int __devinit pnx_kbd_init(void)
{
	printk(KERN_INFO "PNX Keypad Driver\n");
	return platform_driver_register(&pnx_kbd_driver);
}

static void __exit pnx_kbd_exit(void)
{
	platform_driver_unregister(&pnx_kbd_driver);
}

module_init(pnx_kbd_init);
module_exit(pnx_kbd_exit);

/* SYSFS */

static ssize_t pnx_kbd_store_TAT_simu(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{

	unsigned int row, col;
	int state;
	int key;
	int len;

	if(count >=5)
	{
		/* sscanf(buf, "%u%d", &emulated_code, &emulated_value);*/
		len = sscanf(buf, "%u %u %d", &row, &col, &state);

		printk("received emulated key event: row= %u, col=%u, state=%d\n", row, col, state);
		//  input_report_key(keypad_dev, *emulated_code, *emulated_value);
		if((col <= sKbd_cols) && (row <= sKbd_rows)) /* FIXME use dynamic definition from dev */
		{
			if(state !=0) state =1;
			key = pnx_kbd_find_key(sKeypad_dev->keycode , col, row);
			input_report_key( sKeypad_dev, key,state );
		}
		if(len != count)
		{
			printk("len = %x, count=%x\n", len, count);
		}
	}
	else
	{
		printk("usage: row col state\n");
	}
	return (count);
}


static DEVICE_ATTR(TAT_simu, S_IWUGO, NULL, pnx_kbd_store_TAT_simu);

static ssize_t pnx_kbd_show_TAT_keymap(struct device *dev, struct device_attribute *attr, char *buf)
{
  int i;
  int len = 0;
  struct pnx_kbd_platform_data *pdata = (struct pnx_kbd_platform_data*) (dev->parent->platform_data);

  for(i=0; i< pdata->keymapsize ; i++)
  {
	if(pdata->keymap[i]==0) break;
  	  len += scnprintf(buf + len, PAGE_SIZE - len, "0x%8x\n", pdata->keymap[i]);
  }
	return len;
}


static DEVICE_ATTR(TAT_keymap, S_IRUGO, pnx_kbd_show_TAT_keymap, NULL);

static int pnx_kbd_sysfs_init(struct device * dev)
{

	int error = device_create_file(dev, &dev_attr_TAT_keymap);
	if(!error)
		return device_create_file(dev, &dev_attr_TAT_simu);
	else
		return error;
}


MODULE_AUTHOR("Philippe Langlais");
MODULE_DESCRIPTION("PNX Keypad Driver");
MODULE_LICENSE("GPL");
