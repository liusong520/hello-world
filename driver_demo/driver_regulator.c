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
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/driver.h>
#include <pax/devices_base.h>
#include <linux/errno.h>


struct pax_paltform_data {
	int id;
	unsigned long status;
	struct regulator_config config;
	struct miscdevice miscdev;
};

struct pax_regulator_data {
	int		id;
	int		sleep_id;
#if 0
	/* Regulator register address.*/
	u8		reg_en_reg;
	u8		en_bit;
	u8		reg_disc_reg;
	u8		disc_bit;
	u8		vout_reg;
	u8		vout_mask;
	u8		vout_reg_cache;
	u8		sleep_reg;

	/* chip constraints on regulator behavior */
	int			min_uV;
	int			max_uV;
	int			step_uV;
	int			nsteps;
#endif
	/* regulator specific turn-on delay */
	u16			delay;

	/* used by regulator core */
	struct regulator_desc	desc;

	/* Device */
	struct device		*dev;
};

static int pax_list_voltage(struct regulator_dev *dev, unsigned selector)
{
	return 0;
}

static int pax_set_voltage(struct regulator_dev *dev, int min_uV, int max_uV, unsigned *selector)
{
	return 0;
}	

static int pax_set_voltage_sel(struct regulator_dev *dev, unsigned selector)
{
	return 0;
}

static int pax_get_voltage(struct regulator_dev *dev)
{
	return 3300000;
}	

static int pax_get_voltage_sel(struct regulator_dev *dev)
{
	return 0;
}

static int pax_set_current_limit(struct regulator_dev *dev, int min_uA, int max_uA)
{
	return 0;
}

static int pax_get_current_limit(struct regulator_dev *dev)
{
	return 0;
}

static int pax_enable(struct regulator_dev *dev)
{
	printk("%s\n",__func__);
	reg_gpio_set_pin(SDIO1_CD__REV__REV__GPIO128, 1);
	return 0;
}

static int pax_disable(struct regulator_dev *dev)
{
	printk("%s\n",__func__);
	reg_gpio_set_pin(SDIO1_CD__REV__REV__GPIO128, 0);
	return 0;
}

static int pax_is_enabled(struct regulator_dev *dev)
{
//	printk("%s\n",__func__);
	return reg_gpio_get_pin_output(SDIO1_CD__REV__REV__GPIO128);
}

static struct regulator_ops pax_regulator_ops = {
	.list_voltage	= pax_list_voltage,
	.set_voltage	= pax_set_voltage,
	.get_voltage	= pax_get_voltage,
	.enable		= pax_enable,
	.disable		= pax_disable,
	.is_enabled	= pax_is_enabled,
};

struct regulator_desc pax_regulator_desc = {
	.name = "pax_regulator",
	.id = 1,
	.ops = &pax_regulator_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,	
};

static struct regulator_consumer_supply pax_ldo_supply[] = {
	REGULATOR_SUPPLY("pax_vdd1", NULL),
	REGULATOR_SUPPLY("pax_vdd2", NULL),
};

static int regulator_init(void *driver_data)
{
	printk("%s\n",__func__);
	gpio_request(SDIO1_CD__REV__REV__GPIO128, "GPIO128");
	return 0;
}

static struct regulator_init_data pax_init_data = {
	.constraints = {		
		.min_uV = 0,
		.max_uV = 4000*1000,
		.valid_modes_mask = (REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY),
		.valid_ops_mask = (REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS |\
				   REGULATOR_CHANGE_VOLTAGE),
		.boot_on = 0,
		.apply_uV = 0,
	},
	.num_consumer_supplies = ARRAY_SIZE(pax_ldo_supply),
	.consumer_supplies = pax_ldo_supply,
	.supply_regulator = 0,
	.regulator_init = regulator_init,
};

struct regulator_config pax_regulator_config = {
	.init_data = &pax_init_data,
};

static ssize_t reg_show(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf)
{
	struct regulator *regulator;

	regulator = regulator_get(NULL, "pax_vdd1");
	if(!regulator)
		printk("get pax_vdd1 regulator fail\n");
	else
		printk("get pax_vdd1 regulator succes\n");
	regulator_enable(regulator);
	regulator_is_enabled(regulator);
	regulator_disable(regulator);
	regulator_is_enabled(regulator);
	
	return 0;
}

static ssize_t reg_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{

	return count;
}

static struct kobj_attribute regulator_attribute =
	__ATTR(reg, 0666, reg_show, reg_store);

static struct attribute *attrs[] = {
	&regulator_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *example_kobj;

static int pax_probe(struct platform_device *pdev)
{
	int ret;
	struct regulator_dev *rdev;

	pax_regulator_config.dev = &pdev->dev;
	
	rdev = regulator_register(&pax_regulator_desc, &pax_regulator_config);
	if (IS_ERR(rdev)) {
		ret= PTR_ERR(rdev);
		printk("regulator register fail: %d\n", ret);
		return ret;
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
    	return ret;
}

static struct platform_device pax_device = {
	.name		= "pax_dev",
	.id		= -1,	
};

static struct platform_driver pax_driver = {
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
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Watchdog driver for Broadcom iProc chips");
MODULE_LICENSE("GPL");
