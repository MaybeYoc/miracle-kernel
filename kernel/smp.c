/*
 * Generic helpers for smp ipi calls
 *
 * (C) Jens Axboe <jens.axboe@oracle.com> 2008
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/barrier.h>

struct cpumask __cpu_possible_mask __read_mostly;
struct cpumask __cpu_online_mask __read_mostly;
struct cpumask __cpu_present_mask __read_mostly;
struct cpumask __cpu_active_mask __read_mostly;

/* Setup number of possible processor ids */
unsigned int nr_possible_cpu_ids __read_mostly = NR_CPUS;
unsigned int nr_online_cpu_ids;
unsigned int nr_present_cpu_ids;
unsigned int nr_active_cpu_ids;

/* An arch may set nr_possible_cpu_ids earlier if needed, so this would be redundant */
void __init setup_nr_cpu_ids(void)
{
	nr_possible_cpu_ids = find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS) + 1;
}

/**
 * kick_all_cpus_sync - Force all cpus out of idle
 *
 * Used to synchronize the update of pm_idle function pointer. It's
 * called after the pointer is updated and returns after the dummy
 * callback function has been executed on all cpus. The execution of
 * the function can only happen on the remote cpus after they have
 * left the idle function which had been called via pm_idle function
 * pointer. So it's guaranteed that nothing uses the previous pointer
 * anymore.
 */
void kick_all_cpus_sync(void)
{
	/* Make sure the change is visible before we kick the cpus */
	smp_mb();
}

void on_each_cpu_cond(bool (*cond_func)(int cpu, void *info),
			smp_call_func_t func, void *info, bool wait,
			gfp_t gfp_flags)
{
}
