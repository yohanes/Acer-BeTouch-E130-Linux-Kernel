
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <linux/irq.h>

#include <linux/cm3623.h>

struct cm3623_data
{
	struct i2c_client * client;
	struct work_struct work;
  struct workqueue_struct *cm3623_workqueue;
	unsigned int gpiopin;
	unsigned int irqpin;
  int irqid;
  u16 alsdata;
  u8 psdata;
};

static struct i2c_driver cm3623_driver;
static struct platform_device *cm3623_pdev;
struct cm3623_data *cm3623_global;


static ssize_t show_alsdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cm3623_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%d\n", cm3623_global->alsdata);
}

static ssize_t show_psdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cm3623_data *data = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%d\n", cm3623_global->psdata);
}

static DEVICE_ATTR(alsdata, S_IRUGO, show_alsdata, NULL);
static DEVICE_ATTR(psdata, S_IRUGO, show_psdata, NULL);


static struct attribute *cm3623_sysfs_entries[] = {
	&dev_attr_alsdata.attr,
  &dev_attr_psdata.attr,
	NULL
};

static const struct attribute_group cm3623_attr_group = {
	.name	= "cm3623",
	.attrs = cm3623_sysfs_entries,
};


static u8 cm3623_write_byte(struct i2c_client *client, unsigned short opaddr, u8 value)
{
	return i2c_smbus_xfer(client->adapter,opaddr,client->flags,
	                      I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

static u8 cm3623_read_byte(struct i2c_client *client, unsigned short opaddr)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, opaddr, client->flags,
				I2C_SMBUS_READ, 0,
				I2C_SMBUS_BYTE, &data);
  
	return (status < 0) ? status : data.byte;
}

static void cm3623_work(struct work_struct *work)
{
	u8 val;
  u8 als_msb, als_lsb;
  u16 als_data;
  
	struct cm3623_data *data = container_of(work, struct cm3623_data, work);

	//printk("cm3623_work is called!\n");

	//check which interrupt event occur
	val = cm3623_read_byte(data->client, CM3623_INTERRUPT_ADDR_7BITS);
  //printk("interrupt val=[0x%X]\n",val);

	if(0x90 == val)
	{
	  als_lsb = cm3623_read_byte(data->client, CM3623_ALS_READ_LSB_ADDR_7BITS);
    als_msb = cm3623_read_byte(data->client, CM3623_ALS_READ_MSB_ADDR_7BITS);
    als_data = als_msb << 8;
    als_data = als_data | als_lsb;
    printk("als data=[%d]\n",als_data);
    cm3623_global->alsdata = als_data;
  }
  else if(0xF0 == val)
  {
    val = cm3623_read_byte(data->client,CM3623_PS_READ_ADDR_7BITS);
    printk("ps data=[%d]\n",val);
    cm3623_global->psdata = val;
  }
  //printk("enable irqid=[%d]\n",cm3623_global->irqid);

  /*clear interrupt pin, cm3623 interrupt may be triggered when we reading ALS/PS data */
  val = cm3623_read_byte(data->client, CM3623_INTERRUPT_ADDR_7BITS);

  enable_irq(cm3623_global->irqid);

}

static int cm3623_irq_handler(int irq, void *dev_id)
{
	//printk("cm3623_irq_handler is called! id=[%d]\n",irq);

  disable_irq(irq);
  cm3623_global->irqid = irq;
  schedule_work(&cm3623_global->work);
  //enable_irq(irq);
  
	return IRQ_HANDLED;
}

static void cm3623_init_registers(struct cm3623_data *data)
{	
  u8 als_data, ps_thd, ps_data, tmp;
  printk("entering cm3623_init_registers\n");

  als_data = 1 << CM3623_ALS_GAIN1_OFFSET /* +/- 1024 STEP */
           | 1 << CM3623_ALS_GAIN0_OFFSET
           | 0 << CM3623_ALS_THD1_OFFSET
           | 1 << CM3623_ALS_THD0_OFFSET  
           | 0 << CM3623_ALS_IT1_OFFSET   /* 100 ms */
           | 0 << CM3623_ALS_IT0_OFFSET   
           | 1 << CM3623_ALS_WDM_OFFSET   /* word mode */
           | 0 << CM3623_ALS_SD_OFFSET;   /* shut down disable */

  ps_thd = 0x03;

  ps_data = 0 << CM3623_PS_DR1_OFFSET      /* Duty Ratio setting 1/160 */
          | 0 << CM3623_PS_DR0_OFFSET
          | 1 << CM3623_PS_IT1_OFFSET      /* 1.875 T */
          | 1 << CM3623_PS_IT0_OFFSET
          | 1 << CM3623_PS_INTALS_OFFSET   /* enable ALS interrupt */
          | 1 << CM3623_PS_INTPS_OFFSET    /* enable PS interrupt */
          | 0 << CM3623_PS_RES_OFFSET
          | 0 << CM3623_PS_SD_OFFSET;      /* shut down disable */

  printk("init_data=[0x%X] als_data=[0x%X] ps_thd=[0x%X] ps_data=[0x%X]\n", 
          CM3623_INITIAL_INT_DATA, als_data, ps_thd, ps_data);

  /* clear interrupt */
  //printk("clear interrupt\n");
  tmp = cm3623_read_byte(data->client,CM3623_INTERRUPT_ADDR_7BITS);
  
  /* init device */
  //printk("init device\n");
  cm3623_write_byte(data->client,CM3623_INITIAL_ADDR_7BITS,CM3623_INITIAL_INT_DATA);

  /* init ALS */
  //printk("init ALS\n");
  cm3623_write_byte(data->client, CM3623_ALS_WRITE_ADDR_7BITS,als_data);

  /* ps Threshold */
  cm3623_write_byte(data->client, CM3623_PS_THD_ADDR_7BITS,ps_thd);

  /*init ps*/
  cm3623_write_byte(data->client, CM3623_PS_WRITE_ADDR_7BITS,ps_data);
}

static int cm3623_probe(struct i2c_client *new_client, const struct i2c_device_id *id)
{
	struct cm3623_data *data;
	int err = 0;

	printk("entering cm3623_probe\n");

	data = kcalloc(1, sizeof(*data), GFP_KERNEL);
	if(NULL == data)
	{
		err = -ENOMEM;
		goto err_exit;
	}
	i2c_set_clientdata(new_client, data);
	data->client = new_client;

  cm3623_global = data;

  /* init work queue*/
	INIT_WORK(&data->work, cm3623_work);

	/* initial the device */
	cm3623_init_registers(data);

  /* setup interrupt */
  data->irqpin= platform_get_irq(cm3623_pdev, 0);
	data->gpiopin= EXTINT_TO_GPIO(data->irqpin);

  //printk("cm3623: irqpin=[%d] gpiopin=[%d]\n",data->irqpin,data->gpiopin);

  err = pnx_gpio_request(data->gpiopin);
	if( err )
	{
		printk("cm3623: Can't get GPIO_D19 for keypad led\n");
		goto exit_free;
	}

	pnx_gpio_set_mode(data->gpiopin, GPIO_MODE_MUX1);
	pnx_gpio_set_direction(data->gpiopin, GPIO_DIR_INPUT);
	set_irq_type(data->irqpin, IRQ_TYPE_EDGE_FALLING);	

	err = request_irq(data->irqpin, &cm3623_irq_handler, 0, "cm3623_interrupt", NULL);
	if(err < 0)
	{
    printk("cm3623 request_irq error!\n");
    goto exit_free;
	}

	err = sysfs_create_group(&new_client->dev.kobj, &cm3623_attr_group);
	if (err) {
		printk("cm3623 creating sysfs group error!\n");
		goto exit_free;
	}

	return 0;

err_exit:
	return err;

exit_free:
	kfree(data);
	return err;
}

static int __devexit cm3623_remove(struct i2c_client *client)
{
}

#define cm3623_suspend	NULL
#define cm3623_resume	NULL


/* platform driver, since i2c devices don't have platform_data */
static int __init cm3623_plat_probe(struct platform_device *pdev)
{
  printk("cm3623_plat_probe\n");
	cm3623_pdev = pdev;
	return 0;
}

static int cm3623_plat_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver cm3623_plat_driver = {
	.probe	= cm3623_plat_probe,
	.remove	= cm3623_plat_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name 	= "pnx-cm3623",
	},
};

static const struct i2c_device_id cm3623_id[] = {
	{ "cm3623", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cm3623_id);

static struct i2c_driver cm3623_driver = {
	.driver = {
		.name	= " cm3623",
		.owner  = THIS_MODULE,
	},
	.id_table =  cm3623_id,
	.probe	  =  cm3623_probe,
	.remove	  =  __devexit_p(cm3623_remove),
	.suspend  =  cm3623_suspend,
	.resume	  =  cm3623_resume,
};

static int __init cm3623_init(void)
{
	int rc;
	if (!(rc = platform_driver_register(&cm3623_plat_driver)))
	{
		printk("cm3623 platform_driver_register OK !!!\n");
		if (!(rc = i2c_add_driver(&cm3623_driver)))
		{
			printk("cm3623 i2c_add_driver OK !!!\n");
		}
		else
		{
			printk(KERN_ERR "cm3623 i2c_add_driver failed\n");
			platform_driver_unregister(&cm3623_driver);
			return 	-ENODEV;
		}
	}
	else
	{
		printk("cm3623 platform_driver_register Failed !!!\n");
	}

	return rc;
}

static void __exit cm3623_exit(void)
{
	i2c_del_driver (&cm3623_driver);
	platform_driver_unregister(&cm3623_plat_driver);
}

void cm3623_myinit()
{
  cm3623_init_registers(cm3623_global);
}

void cm3623_myread()
{
  u8 val;
  u8 als_msb, als_lsb;
  u16 als_data;
  
  val = cm3623_read_byte(cm3623_global->client, CM3623_INTERRUPT_ADDR_7BITS);
  printk("\ninterrupt val=[%d]\n",val);

	if(0x90 == val)
	{
	  als_lsb = cm3623_read_byte(cm3623_global->client, CM3623_ALS_READ_LSB_ADDR_7BITS);
    als_msb = cm3623_read_byte(cm3623_global->client, CM3623_ALS_READ_MSB_ADDR_7BITS);
    als_data = als_msb << 8;
    als_data = als_data | als_lsb;
    printk("als data=[%d]\n",als_data);
  }
}


module_init(cm3623_init);
module_exit(cm3623_exit);

MODULE_AUTHOR( "ACER" );
MODULE_DESCRIPTION( "CM3623 driver" );
MODULE_LICENSE( "GPL" );
