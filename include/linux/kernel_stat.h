/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KERNEL_STAT_H
#define _LINUX_KERNEL_STAT_H

#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <asm/irq.h>

/*
 * 'kernel_stat.h' contains the definitions needed for doing
 * some kernel statistics (CPU usage, context switches ...),
 * used by rstatd/perfmeter
 */

enum cpu_usage_stat {
	CPUTIME_USER,
	CPUTIME_NICE,
	CPUTIME_SYSTEM,
	CPUTIME_SOFTIRQ,
	CPUTIME_IRQ,
	CPUTIME_IDLE,
	CPUTIME_IOWAIT,
	CPUTIME_STEAL,
	CPUTIME_GUEST,
	CPUTIME_GUEST_NICE,
	NR_STATS,
};

struct kernel_cpustat {
	u64 cpustat[NR_STATS];
};

struct kernel_stat {
	unsigned long irqs_sum;
	unsigned int softirqs[NR_SOFTIRQS];
};

DECLARE_PER_CPU(struct kernel_stat, kstat);
DECLARE_PER_CPU(struct kernel_cpustat, kernel_cpustat);

/* Must have preemption disabled for this to be meaningful. */
#define kstat_this_cpu this_cpu_ptr(&kstat)
#define kcpustat_this_cpu this_cpu_ptr(&kernel_cpustat)
#define kstat_cpu(cpu) per_cpu(kstat, cpu)
#define kcpustat_cpu(cpu) per_cpu(kernel_cpustat, cpu)

#endif /* _LINUX_KERNEL_STAT_H */
