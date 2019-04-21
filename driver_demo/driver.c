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
#include <linux/clocksource.h>
#include <pax/iproc_timer.h>

static struct hrtimer clock_timer;

static cycle_t timer_count[20];
	
static cycle_t get_timer_count(void)
{
#if 1
	u32 hi;
	u32 lo;
	u32 ho;
	u64 count;

	/*
	 * Read the upper half to detect a roll-over count
	 */
	do {
		hi = readl(IOMEM(IPROC_PERIPH_GLB_TIM_REG_VA) + 0x04);
		lo = readl(IOMEM(IPROC_PERIPH_GLB_TIM_REG_VA) + 0x00);
		ho = readl(IOMEM(IPROC_PERIPH_GLB_TIM_REG_VA) + 0x04);
	} while(hi != ho);

	count = (u64) hi << 32 | lo;
	return count;
#else
	return (~readl(IOMEM(IPROC_PERIPH_PVT_TIM_REG_VA)+0x04));
#endif
}

static int i=0;
#ifdef HR_TIMER
static enum hrtimer_restart clock_hrthandler(struct hrtimer *timer)
#else
static void clock_hrthandler(int timer_id, void *cookie)
#endif
{
#ifdef HR_TIMER
	timer_count[i]=get_timer_count();
	if(i++ <= 12)
		hrtimer_start(&clock_timer ,ktime_set(0, 1*1000*1000*1000),HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
#else
	timer_count[i]=get_timer_count();
	if(i++ <= 12) {
		iproc_timer_configure(1, 0, IPROC_TIMER_USEC_TO_TICKS(1000000), clock_hrthandler, NULL);
		iproc_timer_start(1);
	}
	return;
#endif
}

static ssize_t timer_show(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf)
{
	int j;
	for(j=0; j<10; j++)
		printk("1s time count=%lld\n", timer_count[j+1] -timer_count[j]);
	i=0;
	return 0;
}

static ssize_t timer_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
#ifdef HR_TIMER
	hrtimer_start(&clock_timer ,ktime_set(0, 1*1000*1000*1000),HRTIMER_MODE_REL);
#else
	iproc_timer_start(1);
#endif
	return count;
}

static struct kobj_attribute timer_attribute =
	__ATTR(reg, 0666, timer_show, timer_store);

static struct attribute *attrs[] = {
	&timer_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *example_kobj;

static int pax_probe(struct platform_device *pdev)
{
	int ret;
#ifdef HR_TIMER
	hrtimer_init(&clock_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	clock_timer.function = clock_hrthandler;
#else
	iproc_timer_configure(1, 0, IPROC_TIMER_USEC_TO_TICKS(1000000), clock_hrthandler, NULL);
#endif
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
	.name		= "timer_dev",
	.id		= -1,
};

static struct platform_driver pax_driver = {
    	.driver             = {
        		.name           = "timer_dev",
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
