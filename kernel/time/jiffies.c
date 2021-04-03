// SPDX-License-Identifier: GPL-2.0+
/*
 * This file contains the jiffies based clocksource.
 *
 * Copyright (C) 2004, 2005 IBM, John Stultz (johnstul@us.ibm.com)
 */
#include <linux/clocksource.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/init.h>

static struct clocksource clocksource_jiffies;

struct clocksource * __init __weak clocksource_default_clock(void)
{
	return &clocksource_jiffies;
}
