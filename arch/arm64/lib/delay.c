/*
 * Delay loops based on the OpenRISC implementation.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/processor.h>

#include <clocksource/arm_arch_timer.h>

extern u64 ktime_get_cycles(void);


void __delay(unsigned long cycles)
{
	u64 start = ktime_get_cycles();

	if (arch_timer_evtstrm_available()) {
		while ((ktime_get_cycles() - start) < cycles)
			wfe();

		while ((ktime_get_cycles() - start) < cycles)
			cpu_relax();
	}
}

void __ndelay(unsigned long nsecs)
{
	u64 start = ktime_get();

	if (arch_timer_evtstrm_available()) {
		while ((ktime_get() - start) < nsecs)
			wfe();

		while ((ktime_get() - start) < nsecs)
			cpu_relax();
	}
}
