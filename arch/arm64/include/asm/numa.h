/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_NUMA_H
#define __ASM_NUMA_H

#include <linux/compiler.h>
#include <linux/nodemask.h>
#include <linux/init.h>

#ifdef CONFIG_NUMA

int __node_distance(int from, int to);
#define node_distance(a, b) __node_distance(a, b)

extern nodemask_t numa_nodes_parsed;

/* Mappings between node number and cpus on that node. */
extern cpumask_t *node_to_cpumask_map[MAX_NUMNODES];

/* Returns a pointer to the cpumask of CPUs on Node 'node'. */
static inline cpumask_t *cpumask_of_node(int node)
{
	return node_to_cpumask_map[node];
}

#endif

void __init arm64_numa_init(void);
int __init numa_add_memblk(int nodeid, u64 start, u64 end);
void __init numa_set_distance(int from, int to, int distance);

#endif	/* __ASM_NUMA_H */
