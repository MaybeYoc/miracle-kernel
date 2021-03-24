/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H

/*
 *	Generic SMP support
 *		Alan Cox. <alan@redhat.com>
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/init.h>

#include <asm/smp.h>

#ifdef CONFIG_SMP
void kick_all_cpus_sync(void);
extern void __init setup_nr_cpu_ids(void);
#else
static inline void kick_all_cpus_sync(void) {  }
static inline void setup_nr_cpu_ids(void) { }
#endif

/*
 * smp_processor_id(): get the current CPU ID.
 *
 * if DEBUG_PREEMPT is enabled then we check whether it is
 * used in a preemption-safe way. (smp_processor_id() is safe
 * if it's used in a preemption-off critical section, or in
 * a thread that is bound to the current CPU.)
 *
 * NOTE: raw_smp_processor_id() is for internal use only
 * (smp_processor_id() is the preferred variant), but in rare
 * instances it might also be used to turn off false positives
 * (i.e. smp_processor_id() use that the debugging code reports but
 * which use for some reason is legal). Don't use this to hack around
 * the warning message, as your code might not work under PREEMPT.
 */
#define smp_processor_id() raw_smp_processor_id()

#endif /* __LINUX_SMP_H */
