/*
 * EHCI HCD (Host Controller Driver) for PNX67XX USB2.0 OTG.
 */

#include <linux/platform_device.h>
#include <linux/types.h>
#include <mach/gpio.h>
#include <mach/clock.h>
#include <mach/usb.h>
#include <linux/clk.h>
#include <linux/pm_qos_params.h>


struct clk * clk;

static int ehci_pnx_init(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;
	u32 temp;

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

    	tdi_reset(ehci);
	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	hcd->has_tt = 1;

 	if (ehci_is_TDI(ehci))
		ehci_reset(ehci);

	temp = HCS_N_CC(ehci->hcs_params) * HCS_N_PCC(ehci->hcs_params);
	temp &= 0x0f;
	if (temp && HCS_N_PORTS(ehci->hcs_params) > temp) {
		printk( "bogus port configuration: "
			"cc=%d x pcc=%d < ports=%d\n",
			HCS_N_CC(ehci->hcs_params),
			HCS_N_PCC(ehci->hcs_params),
			HCS_N_PORTS(ehci->hcs_params));
	}

	ehci_port_power(ehci, 1);

	return retval;
}

static const struct hc_driver ehci_pnx67xx_hc_driver = {
	.description = hcd_name,
	.product_desc = "PNX67XX EHCI",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	//.irq = ehci_pnx_irq,
	.flags = HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	//.reset = ehci_init,
	.reset = ehci_pnx_init,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
#if defined CONFIG_PM
//	.suspend = ehci_bus_suspend,
//	.resume = ehci_bus_resume,
#endif
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
};


int ehci_pnx67xx_probe(const struct hc_driver *driver, struct platform_device *dev)
{
	int retval;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	u32 tmp;
	struct usb_platform_data * pnx_usb_data = dev->dev.platform_data;

	/* enable USB clock */
	clk = clk_get(&dev->dev, "USBD");
	if (!clk)
	{
		printk("Can't get USBD clock\n");
		return (-ENODEV);
	}
	else
	{
		/* setup USBPWR */
		if (gpio_request(pnx_usb_data->gpio_host_usbpwr, dev->name)==0)
		{
			pnx_gpio_set_mode(pnx_usb_data->gpio_host_usbpwr, pnx_usb_data->gpio_mux_host_usbpwr);
			/* set USB REGU ON */
			pnx_usb_data->usb_vdd_onoff(USB_ON);
			/* Enable USB bock clock */
			clk_enable(clk);
		}
		else
		{
			printk("Can't get USBPWR GPIO\n");
			return (-ENODEV);
		}
	}

	pm_qos_update_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *) hcd_name, 104);
	pm_qos_update_requirement(PM_QOS_DDR_THROUGHPUT, (char *) hcd_name, 60);


	if (dev->resource[1].flags != IORESOURCE_IRQ) {
		printk("resource[1] is not IORESOURCE_IRQ");
		retval = -ENOMEM;
	}
	hcd = usb_create_hcd(driver, &dev->dev, "pnx67xx");
	if (!hcd)
		return -ENOMEM;
	printk("UBD HCD Created\n");
	hcd->rsrc_start = dev->resource[0].start;
	hcd->rsrc_len = (dev->resource[0].end) - (dev->resource[0].start) + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		printk("request_mem_region failed");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		printk("ioremap failed");
		retval = -ENOMEM;
		goto err2;
	}

	ehci = hcd_to_ehci(hcd);

	/* The PNX67xx EHCI USB OTG block has id, config and timmer registers
	 * at the beggining which is why the caps reg starts at 0x100 offset
	 */
	ehci->caps = hcd->regs + 0x100;
	ehci->regs = hcd->regs + 0x100 + HC_LENGTH(readl(&ehci->caps->hc_capbase));

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	/*read USB_CMD register */
	tmp = readl(USB_CMD_REG);
	/* set RST bit */
	tmp |= CMD_RESET;
	writel(tmp,USB_CMD_REG);

	/*read USB_MODE register */
	tmp = readl(USB_MODE_REG);
	/* set HOST mode */
	tmp |= USB_CM_3;
	writel(tmp,USB_MODE_REG);

	/*read USB_CMD register */
	tmp = readl(USB_CMD_REG);
	/* set RUN bit */
	tmp |= CMD_RUN;
	writel(tmp,USB_CMD_REG);

	/*read USB_PORTSC register */
	tmp = readl(USB_PORTSC_REG);
	/* clear PHCD bit */
	tmp &=~USB_PHCD;
	/* set USB PWR bit */
	tmp |= USB_PP;
	writel(tmp,USB_PORTSC_REG);

	retval = usb_add_hcd(hcd, dev->resource[1].start, /*IRQF_DISABLED |*/IRQF_SHARED);

	if (retval == 0){
		printk("USB HCD ADD\n");
		return retval;
	}

	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
	clk_disable(clk);
	pnx_usb_data->usb_vdd_onoff(USB_OFF);
	clk_put(clk);
	/* PUT USBPWR pin in GPIO mode , in INPUT (done by pnx_free_gpio) in order to save power */
	pnx_gpio_set_mode(pnx_usb_data->gpio_host_usbpwr, GPIO_MODE_MUX0);
	gpio_free(pnx_usb_data->gpio_host_usbpwr);

	pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *) hcd_name, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_requirement(PM_QOS_DDR_THROUGHPUT, (char *) hcd_name, PM_QOS_DEFAULT_VALUE);


	return retval;
}


static int ehci_hcd_pnx67xx_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct usb_platform_data * pnx_usb_data = pdev->dev.platform_data;

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	clk_disable(clk);
	pnx_usb_data->usb_vdd_onoff(USB_OFF);
	clk_put(clk);
	/* PUT USBPWR pin in GPIO mode , in INPUT (done by pnx_free_gpio) in order to save power */
	pnx_gpio_set_mode(pnx_usb_data->gpio_host_usbpwr, GPIO_MODE_MUX0);
	gpio_free(pnx_usb_data->gpio_host_usbpwr);

	pm_qos_remove_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *)hcd_name);
	pm_qos_remove_requirement(PM_QOS_DDR_THROUGHPUT, (char *)hcd_name);

	return 0;
}

static int ehci_hcd_pnx67xx_drv_probe(struct platform_device *pdev)
{
	int ret;

	printk("In ehci_hcd_pnx67xx_drv_probe\n");

	if (usb_disabled())
		return -ENODEV;
	
	pm_qos_add_requirement(PM_QOS_HCLK2_THROUGHPUT, (char *) hcd_name, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_requirement(PM_QOS_DDR_THROUGHPUT, (char *) hcd_name, PM_QOS_DEFAULT_VALUE);

	ret = ehci_pnx67xx_probe(&ehci_pnx67xx_hc_driver, pdev);
	return ret;
}

MODULE_ALIAS("pnx67xx_ehci_udc");

static struct platform_driver ehci_hcd_pnx67xx_driver = {
	.probe = ehci_hcd_pnx67xx_drv_probe,
	.remove = ehci_hcd_pnx67xx_drv_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "pnx67xx_ehci_udc",
		.bus = &platform_bus_type
	}
};
