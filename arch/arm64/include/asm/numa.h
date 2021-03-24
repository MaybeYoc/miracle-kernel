/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_NUMA_H
#define __ASM_NUMA_H

#include <linux/compiler.h>
#include <linux/smp.h>
#include <linux/init.h>

void __init arm64_numa_init(void);

/* Mappings between node number and cpus on that node. */
extern cpumask_var_t node_to_cpumask_map[MAX_NUMNODES];

/* Returns a pointer to the cpumask of CPUs on Node 'node'. */
static inline const struct cpumask *cpumask_of_node(int node)
{
	return node_to_cpumask_map[node];
}

#endif	/* __ASM_NUMA_H */
