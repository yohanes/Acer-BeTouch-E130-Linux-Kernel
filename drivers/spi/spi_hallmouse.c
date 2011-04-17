/*
 * Driver for Hall-Mouse SPI Controllers
 *
 * Copyright (C) 2009 ACER
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/spi_hallmouse.h>
//Selwyn modified 2009/12/8
#include <linux/ctype.h>
//~Selwyn modified
#include <asm/io.h>
#include <mach/gpio.h>

#include "spi_pnx67xx.h"
#include "icthm02s_hm.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef HALLMOUSE_INPUT_DEVICE
/* standard linux input device handle */
static struct input_dev *sHallMouse_dev;
#define HM_KEYEVENT(keyid,stat) input_report_key( sHallMouse_dev, keyid, stat)
#else
static void spi_hallmouse_void(unsigned  int key_id, int pressed);
itsignalcallback spi_hallmouse_signal;
#define HM_KEYEVENT(keyid,stat) spi_hallmouse_signal(keyid, stat)
#endif

/* driver data structure */
struct spihm_data
{
  struct work_struct work;
	struct spi_device *spidev;
};
struct spihm_data *spihm_global;

#ifdef HM_DEBUG
#define hm_dbg(fmt, args...) printk(fmt, ## args)
#else
#define hm_dbg(fmt, args...) do{}while(0)
#endif

#ifdef HM_DEVFS
static dev_t hm_devt;

static struct hm_dev_t
{
  struct cdev char_dev;
};
struct hm_dev_t hm_devfs;

static struct file_operations hallmouse_ops = {
	.owner		=	THIS_MODULE,
	.ioctl		=	hallmouse_ioctl,
	.open 		=	hallmouse_open,
	.read     = hallmouse_read,
	.write    = hallmouse_write,
	.release	=	hallmouse_close,
};
#endif /*HM_DEVFS*/

                              
unsigned char init_buff[8] = 
  {
    0x07,       /* offset Byte */
    0x0C,       /* Threshold Byte */
    0x12,       /* Option Auto_Calibration */
    0x10,       /* sampling rate */
    0x00,       /* calibration High byte */
    0x00,       /* calibration Low byte */
    0x00,       /* reserved byte */
    0x00,       /* direction byte */
  };

unsigned char init2_buff[8] = 
  {
    0x07,       /* offset Byte */
    0x0C,       /* Threshold Byte */
    0x16,       /* Option Auto_Calibration */
    0x10,       /* sampling rate */
    0x00,       /* calibration High byte */
    0x00,       /* calibration Low byte */
    0x00,       /* reserved byte */
    0x00,       /* direction byte */
  };

unsigned char recbuf[8];
unsigned char readcmd=0xC0;
unsigned char dummycmd=0x8F;
unsigned char movebuf[3];
static int hmirq;
static int hm_en;

//Selwyn modified 2009/12/7
static int __devinit hm_init_nocal(struct spi_device *spi);

int check_initstate(void)
{
    int i;

    for(i=0; i<8; i++)
    {
        if(init2_buff[i] != recbuf[i])
        {
            return 0;
        }
    }
    return 1;
}

static ssize_t hm_calibration_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int cal_data;
	cal_data = (init2_buff[4] << 8) | init2_buff[5];
	return sprintf(buf, "0x%x\n", cal_data);
}

static ssize_t hm_calibration_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	char *endp;
	int cal_data = simple_strtoul(buf, &endp, 0);
	size_t size = endp - buf;

	int i = 0;
	int tmp = 0;
	struct spi_device *spi = spihm_global->spidev;

	if(*endp && isspace(*endp))
		size++;
	if(size != count)
		return -EINVAL;

	hm_dbg("Hall mouse calibration debug: cal_data = 0x%x\n", cal_data);
	init2_buff[4] = (char)((0x0000ff00 & cal_data) >> 8);
	init2_buff[5] = (char)(0x000000ff & cal_data);

	//write calibration data
	do
	{
	    for(i=0;i<8;i++)
	    {
		spi_write_then_read(spi,(const u8*)&i,1,recbuf,0);
		udelay(30);
		spi_write_then_read(spi,&init2_buff[i],1,recbuf,0);
	    }

	    mdelay(20);
	    for(i=0;i<8;i++)
	    {
		tmp = 0x40|i;
		spi_write_then_read(spi,(const u8*)&tmp,1,&recbuf[i],1);
		//delay us 30
		udelay(30);
	    }
	    hm_dbg("script do calibration=[0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X]\n"
		,recbuf[0], recbuf[1], recbuf[2], recbuf[3], recbuf[4], recbuf[5], recbuf[6], recbuf[7]);
	}
  	while(!check_initstate());

	return count;
}

static ssize_t hm_do_calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc, cal_data;
	struct spi_device *spi = spihm_global->spidev;

	rc = hm_init_nocal(spi);

	if(!rc)
	{
		cal_data = (init2_buff[4] << 8) | init2_buff[5];
	}

	return sprintf(buf,"0x%x\n", cal_data);
}

static DEVICE_ATTR(cal_data, S_IRUGO|S_IWUSR, hm_calibration_read, hm_calibration_write);
static DEVICE_ATTR(cal_done, S_IRUGO, hm_do_calibration, NULL);

static int hm_sysfs_init(struct device *dev)
{
	int error;
	error = device_create_file(dev, &dev_attr_cal_data);
	if(error)
		goto exit;
	error = device_create_file(dev, &dev_attr_cal_done);

exit:
	return error;
}
//~Selwyn modified

static int reset_hall_mouse(struct spi_device *spi)
{
	struct pnx_hall_mouse_pdata *pdata = spi->dev.platform_data;

  /* get HM_EN for hall mouse reset */
  if (pnx_request_gpio(pdata->hm_en))
  {
    hm_dbg("Can't get HM_EN %c%d for hall mouse reset\n", 'A'+pdata->hm_en/32, pdata->hm_en%32);
    return -EPERM;
  }

  hm_en = pdata->hm_en;
  pnx_set_gpio_mode_gpio(pdata->hm_en);
  pnx_set_gpio_direction(pdata->hm_en, GPIO_DIR_OUTPUT);

  /* set reset to high */
  pnx_write_gpio_pin(pdata->hm_en,1);
  mdelay(300);
  return 0;
}

int MX,MY;
int dir;

static irqreturn_t hm_int(int irq, void *dev_id)
{  
  disable_irq(irq);
  hmirq = irq;
  schedule_work(&spihm_global->work);
	return IRQ_HANDLED;
}


static unsigned char old_dir=0;
static void hm_irq_worker(struct work_struct *work)
{
  unsigned char dir;
  struct spihm_data *hallmouse = container_of(work, struct spihm_data, work);

  spi_write_then_read( hallmouse->spidev,&readcmd,1,recbuf/*rxbuff*/,0);
  spi_write_then_read( hallmouse->spidev,&dummycmd,0,&movebuf[0],1);
  spi_write_then_read( hallmouse->spidev,&dummycmd,0,&movebuf[1],1);
  spi_write_then_read( hallmouse->spidev,&dummycmd,0,&movebuf[2],1);
  
  MY = movebuf[1];
  MX = movebuf[2];
    
  if(movebuf[0] & 0x10) MY = -MY;
  if(movebuf[0] & 0x20) MX = -MX;
    
  dir = dir_ary[abs(MY-7)][abs(MX+7)];
  //hm_dbg("x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
  if(dir != old_dir)
  {
    if(dir == HM_C || dir == HM_N)
    {
      switch(old_dir)
      {
        case HM_U:
          HM_KEYEVENT(KEY_UP,0);
          //hm_dbg("KEY_UP Released\n");
          printk(KERN_DEBUG"KEY_UP R x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_R:
          HM_KEYEVENT(KEY_RIGHT,0);
          //hm_dbg("KEY_RIGHT Released\n");
          printk(KERN_DEBUG"KEY_RIGHT R x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_D:
          HM_KEYEVENT(KEY_DOWN,0);
          //hm_dbg("KEY_DOWN Released\n");
          printk(KERN_DEBUG"KEY_DOWN R x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_L:
          HM_KEYEVENT(KEY_LEFT,0);
          //hm_dbg("KEY_LEFT Released\n");
          printk(KERN_DEBUG"KEY_LEFT R x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
      }
    }
    else
    {
      switch(dir)
      {
        case HM_U:
          HM_KEYEVENT(KEY_UP,1);
            //hm_dbg("KEY_UP Pressed\n");
          printk(KERN_DEBUG"KEY_UP P x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_R:
          HM_KEYEVENT(KEY_RIGHT,1);
            //hm_dbg("KEY_RIGHT Pressed\n");
          printk(KERN_DEBUG"KEY_RIGHT P x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_D:
          HM_KEYEVENT(KEY_DOWN,1);
            //hm_dbg("KEY_DOWN Pressed\n");
          printk(KERN_DEBUG"KEY_DOWN P x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
        case HM_L:
          HM_KEYEVENT(KEY_LEFT,1);
            //hm_dbg("KEY_LEFT Pressed\n");
          printk(KERN_DEBUG"KEY_LEFT P x=[%d] y=[%d] dir=[%d]\n",MX,MY,dir);
          break;
      }
    }
    old_dir = dir;
  }
  enable_irq(hmirq);
}

static int __devinit hm_init_nocal(struct spi_device *spi)
{
  struct pnx_hall_mouse_pdata *pdata = spi->dev.platform_data;
  int i = 0;
  int tmp = 0;
  for(i=0;i<8;i++)
  {
    spi_write_then_read(spi,(const u8*)&i,1,recbuf,0);
    udelay(30);
    spi_write_then_read(spi,&init_buff[i],1,recbuf,0);
  }

  mdelay(20);
  for(i=0;i<8;i++)
  {
    tmp = 0x40|i;
    spi_write_then_read(spi,(const u8*)&tmp,1,&recbuf[i],1);
    //delay us 30
    udelay(30);
  }
  hm_dbg("auto calibration data=[0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X]\n"
    ,recbuf[0], recbuf[1], recbuf[2], recbuf[3], recbuf[4], recbuf[5], recbuf[6], recbuf[7]);

  init2_buff[4] = recbuf[4];
  init2_buff[5] = recbuf[5];
 
  /* set reset to low, to disable hall mouse for secand initial */
  pnx_set_gpio_direction(pdata->hm_en, GPIO_DIR_OUTPUT);
  pnx_write_gpio_pin(pdata->hm_en,0);
  mdelay(50);

  /* set reset to high */
  pnx_write_gpio_pin(pdata->hm_en,1);
  mdelay(300);
  
  for(i=0;i<8;i++)
  {
    spi_write_then_read(spi,(const u8*)&i,1,recbuf,0);
    udelay(30);
    spi_write_then_read(spi,&init2_buff[i],1,recbuf,0);
  }

  mdelay(20);
  for(i=0;i<8;i++)
  {
    tmp = 0x40|i;
    spi_write_then_read(spi,(const u8*)&tmp,1,&recbuf[i],1);
    //delay us 30
    udelay(30);
  }
  hm_dbg("read initial data=[0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X]\n"
    ,recbuf[0], recbuf[1], recbuf[2], recbuf[3], recbuf[4], recbuf[5], recbuf[6], recbuf[7]);

  return 0;
}
   
static int __devinit spi_hallmouse_probe(struct spi_device *spi)
{
  struct spihm_data *data;
  struct pnx_hall_mouse_pdata *pdata = spi->dev.platform_data;
  int err;
    
  hm_dbg(KERN_INFO "spi_hallmouse_probe\n");

#if defined(HM_SHOW_CONFIG)
  hm_dbg("config: HM_EN:  %c%d\n", 'A'+pdata->hm_en/32, pdata->hm_en%32);
  hm_dbg("        HM_CS:  %c%d\n", 'A'+spi->chip_select/32, spi->chip_select%32);
  hm_dbg("        HM_SCK: %c%d, MUX%d\n", 'A'+pdata->hm_sck_pin/32, pdata->hm_sck_pin%32, pdata->hm_sck_mux);
  hm_dbg("        HM_SI:  %c%d, MUX%d\n", 'A'+pdata->hm_si_pin/32, pdata->hm_si_pin%32, pdata->hm_si_mux);
  hm_dbg("        HM_SO:  %c%d, MUX%d\n", 'A'+pdata->hm_so_pin/32, pdata->hm_so_pin%32, pdata->hm_so_mux);
  hm_dbg("        HM_INT: %c%d, EXTINT%d\n", 
	'A'+EXTINT_TO_GPIO(pdata->hm_irq)/32, EXTINT_TO_GPIO(pdata->hm_irq)%32, EXTINT_NUM(pdata->hm_irq));
#endif

#ifdef HALLMOUSE_INPUT_DEVICE
  /* Initialize input device info */
  sHallMouse_dev = input_allocate_device();
  if (!sHallMouse_dev)
  {
    printk(KERN_ERR "Unable to allocate %s input device\n", sHallMouse_dev->name);
    err = -EINVAL;
    goto err1;
  }

  platform_set_drvdata(spi, sHallMouse_dev);
	
  /* setup input device */
  set_bit(EV_KEY, sHallMouse_dev->evbit);
  set_bit(KEY_UP, sHallMouse_dev->keybit);
  set_bit(KEY_DOWN, sHallMouse_dev->keybit);
  set_bit(KEY_LEFT, sHallMouse_dev->keybit);
  set_bit(KEY_RIGHT, sHallMouse_dev->keybit);
  sHallMouse_dev->name = "PNX-HallMouse";
  sHallMouse_dev->phys = "input1";
  sHallMouse_dev->dev.parent = &spi->dev;

  //sHallMouse_dev->id.bustype = BUS_HOST;
  //sHallMouse_dev->id.vendor = 0x0123;
  //sHallMouse_dev->id.product = 0x6220 /*dummy value*/;
  //sHallMouse_dev->id.version = 0x0100;

  //sHallMouse_dev->keycode = sKeymap;
  //sHallMouse_dev->keycodesize = sizeof(unsigned int);
  //sHallMouse_dev->keycodemax = pdata->keymapsize;

  /* Register linux input device */
  err = input_register_device(sHallMouse_dev);
  if (err < 0)
  {
    printk(KERN_ERR "Unable to register %s input device\n", sHallMouse_dev->name);
    goto err2;
  }
#endif /*HALLMOUSE_INPUT_DEVICE*/

  //Selwyn modified 2009/12/7
  if(hm_sysfs_init(&(sHallMouse_dev->dev)) != 0)
  {
    printk(KERN_ERR "Unable to register HallMouse SYSFS files\n");
  }
  //~Selwyn modified

  /* ACER Bright Lee, 2009/7/31, AU2.FC-70 Patch W929.5 STE release { */
  pnx_set_gpio_mode(pdata->hm_sck_pin, pdata->hm_sck_mux);
  pnx_set_gpio_mode(pdata->hm_so_pin, pdata->hm_so_mux);
  pnx_set_gpio_mode(pdata->hm_si_pin, pdata->hm_si_mux);
  /* } ACER Bright Lee, 2009/7/29 */

  
  //spi->bits_per_word = 8;
  //spi->mode = SPI_MODE_1;
  //spi->chip_select = HM_CS;
  err = spi_setup(spi);
  if( err < 0 )
  {
    printk(KERN_ERR "SPI SETUP FAILED\n");
    goto err3;
  }
  err = reset_hall_mouse(spi);
  if( err < 0 )
  {
    printk(KERN_ERR "RESET FAILED\n");
    goto err4;
  }
  if (!(data = kzalloc(sizeof(*data), GFP_KERNEL)))
  {
    printk(KERN_ERR "Unable to allocate data\n");
    err = -ENOMEM;
    goto err5;
  }

  INIT_WORK(&data->work, hm_irq_worker);

  /* First of all, reset the WLAN chip*/
  if(hm_init_nocal(spi)/*reset_wlan_chip()*/)
  {
    printk(KERN_ERR "hm_init_nocal failed\n");
    err=-EPERM;
    goto err6;
  }

  data->spidev = spi;
  spihm_global = data;

  err = pnx_request_gpio(EXTINT_TO_GPIO(pdata->hm_irq));
  if(err) 
  {
    printk(KERN_ERR "pnx_request_gpio(%c%d) fail\n", 
	'A'+EXTINT_TO_GPIO(pdata->hm_irq)/32, EXTINT_TO_GPIO(pdata->hm_irq)%32);
    goto err7;
  }
	
  pnx_set_gpio_mode_gpio(EXTINT_TO_GPIO(pdata->hm_irq));
  pnx_set_gpio_direction(EXTINT_TO_GPIO(pdata->hm_irq), GPIO_DIR_INPUT);

  set_irq_type(pdata->hm_irq,IRQ_TYPE_EDGE_FALLING);
  err = request_irq(pdata->hm_irq, &hm_int, 0, "hallmouse", NULL);
  if(err)
  {
    printk(KERN_ERR "request_irq(EXTINT%d) fail\n", EXTINT_NUM(pdata->hm_irq));
    goto err8;
  }   

  return 0;
  
err8:
  pnx_free_gpio(EXTINT_TO_GPIO(pdata->hm_irq));
err7:
//  hm_cleanup_nocal(spi);
err6:
  kfree(data);
err5:
//  unreset_hall_mouse(spi);
  hm_en = -1;
  pnx_free_gpio(pdata->hm_en); /*enable*/
err4:
//  spi_setdown(spi);

err3:
#ifdef HALLMOUSE_INPUT_DEVICE
  input_unregister_device(sHallMouse_dev);
#endif
err2:
#ifdef HALLMOUSE_INPUT_DEVICE
  input_free_device(sHallMouse_dev);
#endif
err1:
  return err;
}

static int __devexit spi_hallmouse_remove(struct platform_device *pdev)
{
  struct pnx_hall_mouse_pdata *pdata = pdev->dev.platform_data;

  hm_dbg(KERN_INFO "spi_hallmouse_remove\n");

  free_irq(pdata->hm_irq, &hm_int);
  pnx_free_gpio(EXTINT_TO_GPIO(pdata->hm_irq));
  
//  hm_cleanup_nocal(spi);

  kfree(spihm_global);

//  unreset_hall_mouse(spi);
  hm_en = -1;
  pnx_free_gpio(pdata->hm_en); /*enable*/

//  spi_setdown(spi);

#ifdef HALLMOUSE_INPUT_DEVICE
  /* Delete input device */
  input_unregister_device(sHallMouse_dev);
  input_free_device(sHallMouse_dev);
#endif
  return 0;
}

static struct spi_driver spi_hallmouse_driver = {
  .driver = {
    .name    = "spi_hallmouse",
    .owner   = THIS_MODULE,
  },
  .probe     = spi_hallmouse_probe,
  .remove    = spi_hallmouse_remove,
};

#ifndef HALLMOUSE_INPUT_DEVICE
void spi_hallmouse_registerSignalFct(itsignalcallback signalfct)
{
    int ret;

    hm_dbg("spi_hallmouse_registerSignalFct\n");
    spi_hallmouse_signal = signalfct;

    #if 0
    set_irq_type(IRQ_EXTINT(14),IRQ_TYPE_EDGE_FALLING);
    ret = request_irq(IRQ_EXTINT(14), &hm_int, 0, "hallmouse", NULL);
    if(ret)
    {
      hm_dbg("spi_hallmouse_registerSignalFctrequest_irq fail ret=[%d]\n",ret);
      spi_hallmouse_signal = spi_hallmouse_void;
    }   
    #endif
}

void spi_hallmouse_unregisterSignalFct(void)
{
    hm_dbg("spi_hallmouse_unregisterSignalFct\n");
    free_irq(IRQ_EXTINT(14), &hm_int);
    spi_hallmouse_signal = spi_hallmouse_void;
}

static void spi_hallmouse_void(unsigned  int key_id, int pressed)
{
    hm_dbg("WARNING !!! Function called before drvchr init !!!\n");   
}
#endif /*HALLMOUSE_INPUT_DEVICE*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hm_earlysuspend(struct early_suspend *pes)
{
  //printk("hm_earlysuspend\n");
  /* set reset to low, to disable hall mouse */
  if(hm_en != -1)
    pnx_write_gpio_pin(hm_en,0);
}

static void hm_earlyresume(struct early_suspend *pes)
{
  //Selwyn modified 2009/12/11
  int i = 0;
  int tmp = 0; 
  struct spi_device *spi = spihm_global->spidev;
  //~Selwyn modified

  //printk("hm_earlyresume\n");
  if(hm_en != -1)
    pnx_write_gpio_pin(hm_en,1);

  //Selwyn modified 2009/12/11
  do
  {
    mdelay(10);
    //write calibration data
    for(i=0;i<8;i++)
    {
        spi_write_then_read(spi,(const u8*)&i,1,recbuf,0);
        udelay(30);
        spi_write_then_read(spi,&init2_buff[i],1,recbuf,0);
    }
    mdelay(20);
    //~test

    for(i=0;i<8;i++)
    {
        tmp = 0x40|i;
        spi_write_then_read(spi,(const u8*)&tmp,1,&recbuf[i],1);
        //delay us 30
        udelay(30);
    }
    hm_dbg("read resume initial data=[0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X] [0x%X]\n"
      ,recbuf[0], recbuf[1], recbuf[2], recbuf[3], recbuf[4], recbuf[5], recbuf[6], recbuf[7]);
  }
  while(!check_initstate());
  //~Selwyn modified

}

static struct early_suspend hallmouse_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = hm_earlysuspend,
	.resume = hm_earlyresume,
};
#endif


static int __init spi_hallmouse_init(void)
{
	int err = 0;
  hm_dbg("spi_hallmouse_init\n");

  hm_en = -1;

#ifndef HALLMOUSE_INPUT_DEVICE  
  spi_hallmouse_signal = spi_hallmouse_void;
#endif

  // add hall mouse driver
  err = spi_register_driver(&spi_hallmouse_driver);
  if(err < 0)
{
    hm_dbg("failed to register hall mouse driver\n");
    return err;    
  }
  
	 /* Early suspende registration */
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hallmouse_earlys);
#endif

#ifdef HM_DEVFS
  // init devfs for hall mouse
  cdev_init(&hm_devfs.char_dev,&hallmouse_ops);
  hm_devfs.char_dev.owner = THIS_MODULE;
  // use dynsmic device number
  err = alloc_chrdev_region(&hm_devt, 0, 1, "hallmouse");
  if(err < 0)
  {
    hm_dbg("failed to allocate char dev region\n");
    return err;    
  }
  hm_dbg("MAJOR=[%d] MINOR=[%d]\n",MAJOR(hm_devt),MINOR(hm_devt));
  err = cdev_add(&hm_devfs.char_dev,hm_devt,1);
#endif /*HM_DEVFS*/

  return err;
}

static void __exit spi_hallmouse_exit(void)
{
  hm_dbg("spi_hallmouse_exit\n");
  
	 /* Early suspende Unregistration */
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&hallmouse_earlys);
#endif

#ifndef HALLMOUSE_INPUT_DEVICE
  spi_hallmouse_unregisterSignalFct();
#endif

  spi_unregister_driver(&spi_hallmouse_driver);

#ifdef HM_DEVFS      
  if(hm_devt)
	  unregister_chrdev_region(hm_devt, 1);
	
	cdev_del(&hm_devfs.char_dev);
#endif /*HM_DEVFS*/
}

#ifdef HM_DEVFS
static int hallmouse_ioctl(struct inode *inode, struct file *instance,
                              unsigned int cmd, unsigned long arg)
{
  return 0;  
}

static int hallmouse_open(struct inode * inode, struct file * instance)
{
	hm_dbg("hallmouse_open\n");
	return 0 ;
}

static int hallmouse_close(struct inode * inode, struct file * instance)
{
	hm_dbg("hallmouse_close\n");
	return 0 ;
}

static ssize_t hallmouse_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
  hm_dbg("hallmouse_read\n");
  return 0;
}

static ssize_t hallmouse_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
  hm_dbg("hallmouse_write\n");
  return 0;  
}
#endif

module_init(spi_hallmouse_init);
module_exit(spi_hallmouse_exit);

MODULE_DESCRIPTION("Hall-Mouse SPI Driver bus 1");
MODULE_LICENSE("GPL");
