// SPDX-License-Identifier: GPL-2.0-only
/*
 * ARM Cortex-A9 MPCore private watchdog. Registers are banked per CPU;
 * only the CPU0 bank is used, so every access runs on CPU0.
 */
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/watchdog.h>

#define WDT_LOAD		0x00
#define WDT_CONTROL		0x08
#define WDT_RESET_STATUS	0x10
#define WDT_DISABLE		0x14

#define WDT_CTRL_ENABLE		BIT(0)
#define WDT_CTRL_WD_MODE	BIT(3)
#define WDT_PRESCALER		255

#define WDT_DEFAULT_TIMEOUT	30

struct mpcore_wdt {
	struct watchdog_device wdd;
	void __iomem *base;
	u32 tick_rate;
	u32 ctrl;
	u32 reset_status;
};

static void mpcore_wdt_load(struct mpcore_wdt *wdt)
{
	u64 ticks = (u64)wdt->wdd.timeout * wdt->tick_rate;

	writel(min_t(u64, ticks, U32_MAX), wdt->base + WDT_LOAD);
}

static void mpcore_wdt_do_start(void *data)
{
	struct mpcore_wdt *wdt = data;

	mpcore_wdt_load(wdt);
	writel(WDT_PRESCALER << 8 | WDT_CTRL_WD_MODE | WDT_CTRL_ENABLE,
	       wdt->base + WDT_CONTROL);
}

static void mpcore_wdt_do_stop(void *data)
{
	struct mpcore_wdt *wdt = data;

	writel(0x12345678, wdt->base + WDT_DISABLE);
	writel(0x87654321, wdt->base + WDT_DISABLE);
	writel(0, wdt->base + WDT_CONTROL);
}

static void mpcore_wdt_do_ping(void *data)
{
	mpcore_wdt_load(data);
}

static void mpcore_wdt_do_probe(void *data)
{
	struct mpcore_wdt *wdt = data;

	wdt->ctrl = readl(wdt->base + WDT_CONTROL);
	wdt->reset_status = readl(wdt->base + WDT_RESET_STATUS);
	writel(1, wdt->base + WDT_RESET_STATUS);
}

static int mpcore_wdt_on_cpu0(struct watchdog_device *wdd, smp_call_func_t fn)
{
	return smp_call_function_single(0, fn, watchdog_get_drvdata(wdd), 1);
}

static int mpcore_wdt_start(struct watchdog_device *wdd)
{
	return mpcore_wdt_on_cpu0(wdd, mpcore_wdt_do_start);
}

static int mpcore_wdt_stop(struct watchdog_device *wdd)
{
	return mpcore_wdt_on_cpu0(wdd, mpcore_wdt_do_stop);
}

static int mpcore_wdt_ping(struct watchdog_device *wdd)
{
	return mpcore_wdt_on_cpu0(wdd, mpcore_wdt_do_ping);
}

static int mpcore_wdt_set_timeout(struct watchdog_device *wdd, unsigned int t)
{
	wdd->timeout = t;
	return watchdog_active(wdd) ? mpcore_wdt_ping(wdd) : 0;
}

/* Runs on CPU0, the reboot CPU, with the other CPU stopped */
static int mpcore_wdt_restart(struct watchdog_device *wdd, unsigned long action,
			      void *data)
{
	struct mpcore_wdt *wdt = watchdog_get_drvdata(wdd);

	mpcore_wdt_do_stop(wdt);
	writel(1, wdt->base + WDT_LOAD);
	writel(WDT_CTRL_WD_MODE | WDT_CTRL_ENABLE, wdt->base + WDT_CONTROL);
	mdelay(1000);

	return 0;
}

static const struct watchdog_info mpcore_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE |
		   WDIOF_CARDRESET,
	.identity = "MPCore watchdog",
};

static const struct watchdog_ops mpcore_wdt_ops = {
	.owner = THIS_MODULE,
	.start = mpcore_wdt_start,
	.stop = mpcore_wdt_stop,
	.ping = mpcore_wdt_ping,
	.set_timeout = mpcore_wdt_set_timeout,
	.restart = mpcore_wdt_restart,
};

static int mpcore_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mpcore_wdt *wdt;
	struct clk *clk;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk), "no clock\n");
	wdt->tick_rate = clk_get_rate(clk) / (WDT_PRESCALER + 1);
	if (!wdt->tick_rate)
		return -EINVAL;

	wdt->wdd.info = &mpcore_wdt_info;
	wdt->wdd.ops = &mpcore_wdt_ops;
	wdt->wdd.parent = dev;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_hw_heartbeat_ms = div_u64((u64)U32_MAX * 1000, wdt->tick_rate);
	wdt->wdd.timeout = WDT_DEFAULT_TIMEOUT;
	watchdog_init_timeout(&wdt->wdd, 0, dev);
	watchdog_set_drvdata(&wdt->wdd, wdt);
	watchdog_set_restart_priority(&wdt->wdd, 128);

	ret = smp_call_function_single(0, mpcore_wdt_do_probe, wdt, 1);
	if (ret)
		return ret;
	if (wdt->reset_status & 1)
		wdt->wdd.bootstatus = WDIOF_CARDRESET;
	if ((wdt->ctrl & (WDT_CTRL_WD_MODE | WDT_CTRL_ENABLE)) ==
	    (WDT_CTRL_WD_MODE | WDT_CTRL_ENABLE)) {
		mpcore_wdt_start(&wdt->wdd);
		set_bit(WDOG_HW_RUNNING, &wdt->wdd.status);
	}

	ret = devm_watchdog_register_device(dev, &wdt->wdd);
	if (ret)
		return ret;

	dev_info(dev, "tick %u Hz, timeout %u s, %s%s\n", wdt->tick_rate,
		 wdt->wdd.timeout,
		 watchdog_hw_running(&wdt->wdd) ? "running" : "stopped",
		 wdt->wdd.bootstatus ? ", last reset by watchdog" : "");
	return 0;
}

static const struct of_device_id mpcore_wdt_of_match[] = {
	{ .compatible = "arm,cortex-a9-twd-wdt" },
	{ }
};
MODULE_DEVICE_TABLE(of, mpcore_wdt_of_match);

static struct platform_driver mpcore_wdt_driver = {
	.probe = mpcore_wdt_probe,
	.driver = {
		.name = "mpcore_wdt",
		.of_match_table = mpcore_wdt_of_match,
	},
};
module_platform_driver(mpcore_wdt_driver);

MODULE_DESCRIPTION("ARM Cortex-A9 MPCore private watchdog");
MODULE_LICENSE("GPL");
