/**
  * A demo driver for hallsensor Character device."
  *
  * hallsensor.c 
  *
  * All rights reserved. Licensed under dual BSD/GPL license.
  */
/* ACER Bright Lee, 2009/11/20, AU2.FC-741, Improve hall sensor coding architecture { */
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <linux/irq.h>

//Selwyn 2009/11/25 modified for app query state
#include <linux/proc_fs.h>

int hall_sensor_flag = 0;

static const char proc_hallsensor_filename[] = "driver/hall_sensor";

static int proc_calc_lens(char *page, char **start, off_t off,
				 int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int proc_hallsensor_read(char *page, char **start, off_t off, int count,
	 int *eof, void *_data)
{
	int len;

	len = snprintf(page, PAGE_SIZE, "%s\n",(hall_sensor_flag)?"SLIDE_ON":"SLIDE_OFF");
		
	return proc_calc_lens(page, start, off, count, eof, len);	
}

#define create_proc_hallsensor() create_proc_read_entry(proc_hallsensor_filename,\
		0, NULL, proc_hallsensor_read, NULL)

#define remove_proc_hallsensor() remove_proc_entry(proc_hallsensor_filename, NULL)
//Selwyn modified



static int hallsensor_irq(int irq, void *dev_id)
{
        int value;
        struct input_dev *pInput_device;        
    	struct platform_device *pdev = (struct platform_device *)dev_id;
        
    	disable_irq(irq);

        pInput_device = dev_get_drvdata(&pdev->dev);
       
	    value = pnx_read_gpio_pin(EXTINT_TO_GPIO (irq));
        
        input_report_switch (pInput_device, SW_LID, value);

        //Selwyn 2009/11/25 modified
        hall_sensor_flag = value;
        //~Selwyn modified
        
    	enable_irq(irq);
    	return IRQ_HANDLED;
}


static int __devinit hallsensor_probe(struct platform_device *pdev)
{ 
        int err;
        int irq, pin;
        struct input_dev *input_device;

        irq = platform_get_irq(pdev, 0);
        pin = EXTINT_TO_GPIO (irq);

        pnx_request_gpio (pin);
        pnx_set_gpio_direction(pin, GPIO_DIR_INPUT);        
        pnx_set_gpio_mode_gpio (pin);

        set_irq_type (irq, IRQ_TYPE_EDGE_BOTH);

        input_device = input_allocate_device();

        set_bit(EV_SW, input_device->evbit);
        set_bit(SW_LID, input_device->swbit);
        
        if (!input_device)
        {
                printk (KERN_ERR "Unable to allocate %s input device\n", pdev->name);
                err = -EINVAL;
                goto err1;
        }
        input_device->name = "PNX_HallSensor";
        platform_set_drvdata(pdev, (void *)input_device);
        err = input_register_device(input_device);

        if (err < 0)
        {
            printk(KERN_ERR "Unable to register %s input device\n", pdev->name);
            input_free_device (input_device);                  
            goto err1;
        }
        
        err = request_irq(irq, &hallsensor_irq, 0, "hallsensor_up", pdev);

        //Selwyn 2009/11/25 modified for boot up hall sensor problem
        if(pnx_read_gpio_pin(EXTINT_TO_GPIO (irq)))
        {
            hall_sensor_flag = 1;
            input_report_switch (input_device, SW_LID, 1);
        }
        //~Selwyn modified

        //Selwyn 2009/11/25 modified for app query state
        create_proc_hallsensor();
        //Selwyn modified
                
        return 0;                
err1:
        pnx_free_gpio (pin);
        return err;
} 


static void hallsensor_remove(struct platform_device *pdev)
{

        int irq, pin;
        struct input_dev *input_device;

        irq = platform_get_irq(pdev, 0);
        pin = EXTINT_TO_GPIO (irq);  

        pnx_free_gpio(pin);
               
        input_device = platform_get_drvdata(pdev);
        free_irq(irq, pdev);
        input_unregister_device(input_device);
        input_free_device(input_device);

        //Selwyn 2009/11/25 modified for app query state
        remove_proc_hallsensor();
        //Selwyn modified

}


static struct platform_driver hallsensor_driver = {
	.driver.name	= "PNX_HallSensor",
	.driver.owner	= THIS_MODULE,
	.probe		= hallsensor_probe,
	.remove		= hallsensor_remove,
};


static int __init hallsensor_init(void)
{
	return platform_driver_register(&hallsensor_driver);
}

static void __exit hallsensor_exit(void)
{
	platform_driver_unregister(&hallsensor_driver);
}


module_init(hallsensor_init); 
module_exit(hallsensor_exit); 

MODULE_DESCRIPTION("HALLSENSOR Input Device"); 
MODULE_LICENSE("GPL"); 
/* } ACER Bright Lee, 2009/11/20 */
