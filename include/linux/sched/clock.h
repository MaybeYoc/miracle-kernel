/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_CLOCK_H
#define _LINUX_SCHED_CLOCK_H

#include <linux/smp.h>

/*
 * Do not use outside of architecture code which knows its limitations.
 *
 * sched_clock() has no promise of monotonicity or bounded drift between
 * CPUs, use (which you should not) requires disabling IRQs.
 *
 * Please use one of the three interfaces below.
 */
extern unsigned long long notrace sched_clock(void);

static inline void sched_clock_tick(void)
{
}

static inline void clear_sched_clock_stable(void)
{
}

static inline void sched_clock_idle_sleep_event(void)
{
}

static inline void sched_clock_idle_wakeup_event(void)
{
}

static inline u64 cpu_clock(int cpu)
{
	return sched_clock();
}

static inline u64 local_clock(void)
{
	return sched_clock();
}

static inline void enable_sched_clock_irqtime(void) {}
static inline void disable_sched_clock_irqtime(void) {}

/*
 * See the comment in kernel/sched/clock.c
 */
extern u64 running_clock(void);
extern u64 sched_clock_cpu(int cpu);

#endif /* _LINUX_SCHED_CLOCK_H */
