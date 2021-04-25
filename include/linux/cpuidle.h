/*
 * cpuidle.h - a generic framework for CPU idle power management
 *
 * (C) 2007 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 *          Shaohua Li <shaohua.li@intel.com>
 *          Adam Belay <abelay@novell.com>
 *
 * This code is licenced under the GPL.
 */

#ifndef _LINUX_CPUIDLE_H
#define _LINUX_CPUIDLE_H

#include <linux/percpu.h>
#include <linux/list.h>
#include <linux/hrtimer.h>

#define CPUIDLE_STATE_MAX	10
#define CPUIDLE_NAME_LEN	16
#define CPUIDLE_DESC_LEN	32

struct cpuidle_state_usage {
	unsigned long long	disable;
	unsigned long long	usage;
	unsigned long long	time; /* in US */
	unsigned long long	above; /* Number of times it's been too deep */
	unsigned long long	below; /* Number of times it's been too shallow */
#ifdef CONFIG_SUSPEND
	unsigned long long	s2idle_usage;
	unsigned long long	s2idle_time; /* in US */
#endif
};

struct cpuidle_device {
	unsigned int		registered:1;
	unsigned int		enabled:1;
	unsigned int		use_deepest_state:1;
	unsigned int		poll_time_limit:1;
	unsigned int		cpu;

	int			last_residency;
	struct cpuidle_state_usage	states_usage[CPUIDLE_STATE_MAX];

	struct list_head 	device_list;

};

DECLARE_PER_CPU(struct cpuidle_device *, cpuidle_devices);
DECLARE_PER_CPU(struct cpuidle_device, cpuidle_dev);


struct cpuidle_driver;
struct cpuidle_state {
	char		name[CPUIDLE_NAME_LEN];
	char		desc[CPUIDLE_DESC_LEN];

	unsigned int	flags;
	unsigned int	exit_latency; /* in US */
	int		power_usage; /* in mW */
	unsigned int	target_residency; /* in US */
	bool		disabled; /* disabled on all CPUs */

	int (*enter)	(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index);

	int (*enter_dead) (struct cpuidle_device *dev, int index);

	/*
	 * CPUs execute ->enter_s2idle with the local tick or entire timekeeping
	 * suspended, so it must not re-enable interrupts at any point (even
	 * temporarily) or attempt to change states of clock event devices.
	 */
	void (*enter_s2idle) (struct cpuidle_device *dev,
			      struct cpuidle_driver *drv,
			      int index);
};

struct cpuidle_driver {
	const char		*name;
	struct module 		*owner;
	int                     refcnt;

        /* used by the cpuidle framework to setup the broadcast timer */
	unsigned int            bctimer:1;
	/* states array must be ordered in decreasing power consumption */
	struct cpuidle_state	states[CPUIDLE_STATE_MAX];
	int			state_count;
	int			safe_state_index;

	/* the driver handles the cpus in cpumask */
	struct cpumask		*cpumask;
};

#endif /* _LINUX_CPUIDLE_H */
