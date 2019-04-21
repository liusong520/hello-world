/*
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/amba/pl022.h>
#include <linux/pwm.h>
#include <linux/hrtimer.h>

struct pax_paltform_data {
	unsigned long status;
	void __iomem	*dmu_base;
	void __iomem	*core_base;
	void __iomem	*arm_base;
	void __iomem	*map_reg;
	struct miscdevice miscdev;
	struct spi_device *spi;
};
struct pax_paltform_data plat_data;

struct pl022_config_chip pax_spi_chip = {
        .com_mode                       =POLLING_TRANSFER,
        .iface                          = SSP_INTERFACE_MOTOROLA_SPI,
        .hierarchy                      = SSP_MASTER,
        .slave_tx_disable       = 0,
};

static struct spi_board_info pax_spi_info= {
		.modalias       = "spi2_dev",
		.platform_data  = NULL,
		.controller_data = &pax_spi_chip,
		.max_speed_hz   = 500000,
		.bus_num        = 2, 
		.chip_select    = 1,
		.mode           = SPI_MODE_0 |SPI_LOOP ,
};

static int pax_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(1, &plat_data.status))
		return -EBUSY;
	return 0;
}

/*
 * Close the watchdog device.
 */
static int pax_close(struct inode *inode, struct file *file)
{
	clear_bit(1, &plat_data.status);
    	return 0;
}

static long pax_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	switch (cmd) {
	case WDIOC_GETSUPPORT:
        		break;
	case WDIOC_GETSTATUS:
        		break;
	case WDIOC_GETTIMEOUT:
		break;
	}

	return 0;
}

static ssize_t pax_write(struct file *file, const char __user *data, 
                size_t len, loff_t *ppos)
{
    return len;
}

static const struct file_operations pax_fops = {
    .owner              	= THIS_MODULE,
    .llseek             	= no_llseek,
    .unlocked_ioctl     	= pax_ioctl,
    .open               	= pax_open,
    .release            	= pax_close,
    .write              	= pax_write,
};

static ssize_t reg_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int addr,value;
	char *end;

	addr = simple_strtoul(buf, &end, 16);

	if(addr>0x19000000)
		plat_data.map_reg = plat_data.arm_base+(addr- 0x19000000);
	else if(addr>0x18000000)
		plat_data.map_reg = plat_data.core_base+(addr- 0x18000000);
	else	
		plat_data.map_reg = plat_data.dmu_base +(addr- 0x03000000);

	if(*end == ' ') {
		while(*buf++ != ' ');
		value = simple_strtoul(buf, &end, 16);
		__raw_writel(value, plat_data.map_reg);
		printk("WRITE addr=0x%08x, value=0x%x\n", addr, value);
	}
	else {
		value = __raw_readl(plat_data.map_reg);
		printk("READ addr=0x%08x, value=0x%x\n", addr, value);
	}

	return count;
}

static unsigned int batch_reg[1024];

static ssize_t gmac_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int i,addr,value;
	char *end;

	value = simple_strtoul(buf, &end, 16);
	if(value == 0) {
		for(i=0,addr=0x42000; addr<0x42c00; addr += 4,i++) {
			batch_reg[i]= __raw_readl(plat_data.core_base+addr);		
			if(!(i%4)) printk("\n");
			printk("REG[%08x]=0x%08x, ",addr+0x18000000,batch_reg[i]);
		}
	}
	else if(value == 1) {
		for(i=0,addr=0x42000; addr<0x42c00; addr += 4,i++) {
			__raw_writel(batch_reg[i], plat_data.core_base+addr);
		}
	}
	printk("\n");
	return count;
}

#define MII_phy_adr	0
#define MII_Management_Control							(plat_data.core_base + 0x2000)
#define MII_Management_Control__BYP 10
#define MII_Management_Control__EXT 9
#define MII_Management_Control__BSY 8
#define MII_Management_Control__PRE 7
#define MII_Management_Control__MDCDIV_R 0
#define MII_Management_Control__MDCDIV_WIDTH 7
#define MII_Management_Control__RESERVED_R 11
#define MII_Management_Control_WIDTH 11
#define MII_Management_Control__WIDTH 11
#define MII_Management_Command_Data 					(plat_data.core_base + 0x2004)
#define MII_Management_Command_Data__SB_R 30
#define MII_Management_Command_Data__SB_WIDTH 2
#define MII_Management_Command_Data__OP_R 28
#define MII_Management_Command_Data__OP_WIDTH 2
#define MII_Management_Command_Data__PA_R 23
#define MII_Management_Command_Data__PA_WIDTH 5
#define MII_Management_Command_Data__RA_R 18
#define MII_Management_Command_Data__RA_WIDTH 5
#define MII_Management_Command_Data__TA_R 16
#define MII_Management_Command_Data__TA_WIDTH 2
#define MII_Management_Command_Data__Data_R 0
#define MII_Management_Command_Data__Data_WIDTH 16

static ssize_t phy_show(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf)
{
	int i,addr,value,data;
	char *end;

	for (addr=0; addr<0x1F; addr++) {
		if (addr==0x0c||addr==0x0d||addr==0x0e||addr==0x15||addr==0x16)
			continue;
		if(!(addr%2))
			printk("\n");
		data  = 0;
		data |= (2			<< MII_Management_Command_Data__TA_R);
		data |= (addr	  			<< MII_Management_Command_Data__RA_R);
		data |= (MII_phy_adr	<< MII_Management_Command_Data__PA_R);
		data |= (2			<< MII_Management_Command_Data__OP_R);
		data |= (1			<< MII_Management_Command_Data__SB_R);

		__raw_writel(data,MII_Management_Command_Data);
		while(__raw_readl(MII_Management_Control) & (1<<MII_Management_Control__BSY));
		value = __raw_readl(MII_Management_Command_Data) & 0xFFFF;

		printk("Phy0_addr=0x%03x,value=0x%04x  ", addr, value);
	}
	printk("\n");
	return 0;
}

static ssize_t phy_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int i,addr,value,data;
	char *end;

	addr = simple_strtoul(buf, &end, 16);

	if(*end == ' ') {
		while(*buf++ != ' ');
		value = simple_strtoul(buf, &end, 16);

		data  = 0;
		data |= (2			<< MII_Management_Command_Data__TA_R);
		data |= (addr	  		<< MII_Management_Command_Data__RA_R);
		data |= (MII_phy_adr	<< MII_Management_Command_Data__PA_R);
		data |= (1			<< MII_Management_Command_Data__OP_R);
		data |= (1			<< MII_Management_Command_Data__SB_R);
		data |= value;

		__raw_writel(data,MII_Management_Command_Data);
		while(__raw_readl(MII_Management_Control) & (1<<MII_Management_Control__BSY));

		printk("Phy0 addr=0x%03x,value=0x%x\n,", addr, value);
	}
	else {
		data  = 0;
		data |= (2			<< MII_Management_Command_Data__TA_R);
		data |= (addr	  		<< MII_Management_Command_Data__RA_R);
		data |= (MII_phy_adr	<< MII_Management_Command_Data__PA_R);
		data |= (2			<< MII_Management_Command_Data__OP_R);
		data |= (1			<< MII_Management_Command_Data__SB_R);

		__raw_writel(data,MII_Management_Command_Data);
		while(__raw_readl(MII_Management_Control) & (1<<MII_Management_Control__BSY));
		value = __raw_readl(MII_Management_Command_Data) & 0xFFFF;

		printk("Phy0 addr=0x%03x, value=0x%x\n", addr, value);
	}

	printk("\n");
	return count;
}



static ssize_t switch_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	return count;
}

static struct kobj_attribute reg_attribute =
	__ATTR(reg, 0666, NULL, reg_store);

static struct kobj_attribute gmac_attribute =
	__ATTR(gmac, 0666, NULL,gmac_store);

static struct kobj_attribute phy_attribute =
	__ATTR(phy, 0666, phy_show, phy_store);

static struct kobj_attribute switch_attribute =
	__ATTR(switch, 0666, NULL, switch_store);

static struct attribute *attrs[] = {
	&reg_attribute.attr,
	&gmac_attribute.attr,
	&phy_attribute.attr,
	&switch_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *example_kobj;

static int pax_probe(struct platform_device *pdev)
{
	int ret;

    	plat_data.dmu_base =  ioremap(0x03000000, 0x100000);
	plat_data.core_base =  ioremap(0x18000000, 0x100000);
	plat_data.arm_base  =  ioremap(0x19000000, 0x100000); 
	if (!plat_data.dmu_base) {
        		ret = -ENOMEM;
        		printk("could not map I/O memory\n");
        		goto err_free;
	}
	printk("ioremap 0x03000000 to 0x%x\n",plat_data.dmu_base);
	printk("ioremap 0x18000000 to 0x%x\n",plat_data.core_base);
	plat_data.status = 0;
	plat_data.miscdev.minor    = MISC_DYNAMIC_MINOR;
	plat_data.miscdev.name    = "pax_misc";
	plat_data.miscdev.fops    = &pax_fops;
	plat_data.miscdev.parent    = &pdev->dev;

	platform_set_drvdata(pdev, &plat_data);


	ret = misc_register(&plat_data.miscdev);
    	if (ret) {
        		printk("failed to register pax_misc miscdev\n");
        		goto err_register;
    	}

	example_kobj = kobject_create_and_add("cygnus", NULL);
	if(!example_kobj) {
		printk(" kobject creat fail\n");
		goto err_register;
	}

	sysfs_create_group(example_kobj, &attr_group);
	
	printk("register sucessfully\n");

	return 0;

err_register:
    	platform_set_drvdata(pdev, NULL);
    	iounmap(plat_data.dmu_base);
err_free:
    	return ret;
}

static int pax_remove(struct platform_device *pdev)
{
        	misc_deregister(&plat_data.miscdev);
        	iounmap(plat_data.dmu_base);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_device pax_device = {
	.name		= "pax_dev",
	.id		= -1,	
};

static struct platform_driver pax_driver = {
	.remove             = pax_remove,
    	.driver             = {
        		.name           = "pax_dev",
        		.owner          = THIS_MODULE,
    	},
};

static int pax_init(void)
{
	platform_device_register(&pax_device);
	return platform_driver_probe(&pax_driver, pax_probe);
}
module_init(pax_init);

static void pax_exit(void)
{
	platform_driver_unregister(&pax_driver);
	platform_device_unregister(&pax_device);
}
module_exit(pax_exit);

MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Watchdog driver for Broadcom iProc chips");
MODULE_LICENSE("GPL");
