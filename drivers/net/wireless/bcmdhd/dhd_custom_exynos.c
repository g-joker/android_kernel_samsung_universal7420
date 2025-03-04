/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 1999-2016, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_custom_exynos.c 617191 2016-02-04 11:49:36Z $
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>

#if defined(CONFIG_64BIT)
#include <asm-generic/gpio.h>
#else
#if !defined(CONFIG_ARCH_SWA100) && !defined(CONFIG_MACH_UNIVERSAL7580)
#include <mach/gpio.h>
#endif /* !CONFIG_ARCH_SWA100 && !CONFIG_MACH_UNIVERSAL7580 */
#endif /* CONFIG_64BIT */

#if !defined(CONFIG_ARCH_SWA100) && !defined(CONFIG_SOC_EXYNOS7870)
#include <mach/irqs.h>
#include <plat/gpio-cfg.h>
#endif /* !CONFIG_ARCH_SWA100 && !CONFIG_MACH_UNIVERSAL7580 */

#include <linux/sec_sysfs.h>

#ifdef CONFIG_MACH_A7LTE
#define PINCTL_DELAY 150
#endif /* CONFIG_MACH_A7LTE */
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_STATIC_DHD_WLFC_INFO	8
#define WLAN_STATIC_DHD_WLFC_HANGER	9
#define WLAN_STATIC_DHD_LOG_DUMP_BUF	10
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE			(24 * 1024)
#define WLAN_STATIC_DHD_WLFC_INFO_SIZE		(64 * 1024)
#define WLAN_STATIC_DHD_WLFC_HANGER_SIZE	(64 * 1024)

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17
#define DHD_LOG_DUMP_BUF_SIZE	(1024 * 1024)

#if defined(CONFIG_ARGOS)
extern int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
                 struct cpumask *affinity_cpu_mask,
                 struct cpumask *default_cpu_mask);
#endif /* CONFIG_ARGOS */
static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0 = NULL;
void *wlan_static_scan_buf1 = NULL;
void *wlan_static_dhd_info_buf = NULL;
void *wlan_static_dhd_wlfc_buf = NULL;
void *wlan_static_dhd_wlfc_hanger_buf = NULL;
void *wlan_static_dhd_log_dump_buf = NULL;

static void *dhd_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than"
				" static size(%d).\n", size,
				WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if (section == WLAN_STATIC_DHD_WLFC_INFO)  {
		if (size > WLAN_STATIC_DHD_WLFC_INFO_SIZE) {
			pr_err("request DHD_WLFC_INFO size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_STATIC_DHD_WLFC_INFO_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_buf;
	}

	if (section == WLAN_STATIC_DHD_WLFC_HANGER)  {
		if (size > WLAN_STATIC_DHD_WLFC_HANGER_SIZE) {
			pr_err("request DHD_WLFC_HANGER size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_STATIC_DHD_WLFC_HANGER_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_hanger_buf;
	}

	if (section == WLAN_STATIC_DHD_LOG_DUMP_BUF) {
		if (size > DHD_LOG_DUMP_BUF_SIZE) {
			pr_err("request DHD_LOG_DUMP_BUF size(%lu) is bigger then"
				" static size(%d).\n", size, DHD_LOG_DUMP_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_log_dump_buf;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int dhd_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		wlan_mem_array[i].mem_ptr =
			kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	wlan_static_dhd_wlfc_buf = kmalloc(WLAN_STATIC_DHD_WLFC_INFO_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_buf) {
		goto err_mem_alloc;
	}

	wlan_static_dhd_wlfc_hanger_buf = kmalloc(WLAN_STATIC_DHD_WLFC_HANGER_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_hanger_buf) {
		goto err_mem_alloc;
	}

	wlan_static_dhd_log_dump_buf = kmalloc(DHD_LOG_DUMP_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_log_dump_buf) {
		pr_err("Failed to alloc wlan_static_dhd_log_dump_buf\n");
		goto err_mem_alloc;
	}

	pr_err("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);
	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);
	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);
	if (wlan_static_dhd_wlfc_buf)
		kfree(wlan_static_dhd_wlfc_buf);
	if (wlan_static_dhd_log_dump_buf)
		kfree(wlan_static_dhd_log_dump_buf);

	for (j = 0; j < i; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

static struct device *wlan_dev;
static int wlan_pwr_on = -1;
static int wlan_host_wake_irq = 0;
#ifdef CONFIG_MACH_A7LTE
extern struct device *mmc_dev_for_wlan;
#endif /* CONFIG_MACH_A7LTE */
#if defined(CONFIG_MACH_UNIVERSAL3475) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_MACH_UNIVERSAL7420)
extern struct mmc_host *wlan_mmc;
extern void mmc_ctrl_power(struct mmc_host *host, bool onoff);
#endif /* CONFIG_MACH_UNIVERSAL3475 || CONFIG_SOC_EXYNOS7870 */

static int dhd_wlan_power(int onoff)
{
#ifdef CONFIG_MACH_A7LTE
	struct pinctrl *pinctrl = NULL;
#endif /* CONFIG_MACH_A7LTE */

	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __FUNCTION__, onoff ? "on" : "off");

#ifdef CONFIG_MACH_A7LTE
	if (onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan, "sdio_wifi_on");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n", __FUNCTION__);
		msleep(PINCTL_DELAY);
	}
#endif /* CONFIG_MACH_A7LTE */

	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
			__FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}

#ifdef CONFIG_MACH_A7LTE
	if (!onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan, "sdio_wifi_off");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n", __FUNCTION__);
	}
#endif /* CONFIG_MACH_A7LTE */
#if defined(CONFIG_MACH_UNIVERSAL3475) || defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_MACH_UNIVERSAL7420)
	if (wlan_mmc)
		mmc_ctrl_power(wlan_mmc, onoff);
#endif /* CONFIG_MACH_UNIVERSAL3475 || CONFIG_SOC_EXYNOS7870 */
	return 0;
}

static int dhd_wlan_reset(int onoff)
{
	return 0;
}

extern void (*notify_func_callback)(void *dev_id, int state);
extern void *mmc_host_dev;

static int dhd_wlan_set_carddetect(int val)
{
	pr_err("%s: notify_func=%p, mmc_host_dev=%p, val=%d\n",
		__FUNCTION__, notify_func_callback, mmc_host_dev, val);

	if (notify_func_callback)
		notify_func_callback(mmc_host_dev, val);
	else
		pr_warning("%s: Nobody to notify\n", __FUNCTION__);

	return 0;
}

int dhd_get_system_rev(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	struct device_node *root_node = NULL;
	unsigned int base_system_rev_for_nv = 0;
	int ret;

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		printk(KERN_ERR "couldn't get root node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(root_node, "base_system_rev_for_nv",
			&base_system_rev_for_nv);
	if (ret) {
		printk(KERN_INFO "couldn't get base_system_rev_for_nv\n");
		return -ENODEV;
	}

	return base_system_rev_for_nv;
}

int __init dhd_wlan_init_gpio(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	unsigned int wlan_host_wake_up = -1;
	struct device_node *root_node = NULL;

#ifndef CONFIG_ARCH_SWA100
	wlan_dev = sec_device_create(NULL, "wlan");
	BUG_ON(!wlan_dev);
#endif /* not CONFIG_ARCH_SWA100 */
	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4354\n");
		return -ENODEV;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
	gpio_direction_output(wlan_pwr_on, 0);
	gpio_export(wlan_pwr_on, 1);
#ifndef CONFIG_ARCH_SWA100
	gpio_export_link(wlan_dev, "WLAN_REG_ON", wlan_pwr_on);
#endif /* not CONFIG_ARCH_SWA100 */
	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 1);
#ifndef CONFIG_ARCH_SWA100
	gpio_export_link(wlan_dev, "WLAN_HOST_WAKE", wlan_host_wake_up);
#endif /* not CONFIG_ARCH_SWA100 */
	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);

	return 0;
}

#if defined(CONFIG_ARGOS)
void set_cpucore_for_interrupt(cpumask_var_t default_cpu_mask,
	cpumask_var_t affinity_cpu_mask) {
#if defined(CONFIG_MACH_UNIVERSAL5430)
	argos_irq_affinity_setup_label(IRQ_SPI(226), "WIFI", affinity_cpu_mask, default_cpu_mask);
	argos_irq_affinity_setup_label(IRQ_SPI(2), "WIFI", affinity_cpu_mask, default_cpu_mask);
#endif
}
#endif /* CONFIG_ARGOS */

void interrupt_set_cpucore(int set, unsigned int dpc_cpucore, unsigned int primary_cpucore)
{
	printk(KERN_INFO "%s: set: %d\n", __FUNCTION__, set);
	if (set)
	{
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(dpc_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(dpc_cpucore));
#elif defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(dpc_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(dpc_cpucore));
#elif defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(dpc_cpucore));
#endif
	} else {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(primary_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(primary_cpucore));
#elif defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(primary_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(primary_cpucore));
#elif defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(primary_cpucore));
#endif
	}
}

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL |
	IORESOURCE_IRQ_SHAREABLE,
};

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_power,
	.set_reset	= dhd_wlan_reset,
	.set_carddetect	= dhd_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif
};

int __init dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: start\n", __FUNCTION__);
	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
			__FUNCTION__, ret);
		return ret;
	}

	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	ret = dhd_init_wlan_mem();
#endif

	return ret;
}

device_initcall(dhd_wlan_init);
