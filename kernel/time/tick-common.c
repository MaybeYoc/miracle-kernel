// SPDX-License-Identifier: GPL-2.0
/*
 * This file contains the base functions to manage periodic tick
 * related events.
 *
 * Copyright(C) 2005-2006, Thomas Gleixner <tglx@linutronix.de>
 * Copyright(C) 2005-2007, Red Hat, Inc., Ingo Molnar
 * Copyright(C) 2006-2007, Timesys Corp., Thomas Gleixner
 */

#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/clockchips.h>

#include "tick-sched.h"

/*
 * Tick devices
 */
DEFINE_PER_CPU(struct tick_device, tick_cpu_device);

/*
 * Check, if the new registered device should be used. Called with
 * clockevents_lock held and interrupts disabled.
 */
void tick_check_new_device(struct clock_event_device *newdev)
{
	struct tick_device *td;
	int cpu;

	cpu = smp_processor_id();

	td = &per_cpu(tick_cpu_device, cpu);
	td->evtdev = newdev;
	td->mode = TICKDEV_MODE_ONESHOT;
}

/*
 * Check whether the new device is a better fit than curdev. curdev
 * can be NULL !
 */
bool tick_check_replacement(struct clock_event_device *curdev,
			    struct clock_event_device *newdev)
{
	return true;
}

void tick_install_replacement(struct clock_event_device *newdev)
{
}
