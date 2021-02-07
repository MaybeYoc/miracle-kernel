// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/percpu.h>

#include <asm/pgtable.h>

/*
 * For dynamically allocated mm_structs, there is a dynamically sized cpumask
 * at the end of the structure, the size of which depends on the maximum CPU
 * number the system can see. That way we allocate only as much memory for
 * mm_cpumask() as needed for the hundreds, or thousands of processes that
 * a system typically runs.
 *
 * Since there is only one init_mm in the entire system, keep it simple
 * and size this cpu_bitmask to NR_CPUS.
 */
struct mm_struct init_mm = {
	.pgd		= swapper_pg_dir,
	.cpu_bitmap	= { [BITS_TO_LONGS(NR_CPUS)] = 0},
};
