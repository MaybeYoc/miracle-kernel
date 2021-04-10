/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H

#ifdef CONFIG_NODES_SHIFT
#define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
#define NODES_SHIFT     0
#endif

#define MAX_NUMNODES    (1 << NODES_SHIFT)

#define	NUMA_NO_NODE	(-1)

#include <linux/percpu.h>

#include <asm/numa.h>

#ifndef nr_cpus_node
#define nr_cpus_node(node) cpumask_weight(cpumask_of_node(node))
#endif

#define for_each_node_with_cpus(node) 	\
	for_each_online_cpu(node)	\
		if (nr_cpus_node(node))

#define LOCAL_DISTANCE		10
#define REMOTE_DISTANCE		20
#ifndef node_distance
#define node_distance(from,to)	((from) == (to) ? LOCAL_DISTANCE : REMOTE_DISTANCE)
#endif

#ifndef RECLAIM_DISTANCE
/*
 * If the distance between nodes in a system is larger than RECLAIM_DISTANCE
 * (in whatever arch specific measurement units returned by node_distance())
 * and node_reclaim_mode is enabled then the VM will only call node_reclaim()
 * on nodes within this distance.
 */
#define RECLAIM_DISTANCE 30
#endif

#ifndef PENALTY_FOR_NODE_WITH_CPUS
#define PENALTY_FOR_NODE_WITH_CPUS	(1)
#endif

#ifdef CONFIG_NUMA
DECLARE_PER_CPU(int, numa_node);

#ifndef this_cpu_numa_node_id
static inline int this_cpu_numa_node_id(void)
{
	return this_cpu_read(numa_node);
}
#endif

#ifndef cpu_to_node
static inline int cpu_to_node(int cpu)
{
	return per_cpu(numa_node, cpu);
}
#endif

#ifndef this_cpu_set_numa_node
static inline void this_cpu_set_numa_node(int node)
{
	this_cpu_write(numa_node, node);
}
#endif

#ifndef set_cpu_numa_node
static inline void set_cpu_numa_node(int cpu, int node)
{
	per_cpu(numa_node, cpu) = node;
}
#endif

#else

#ifndef this_cpu_numa_node_id
#define this_cpu_numa_node_id() 0
#endif

#ifndef cpu_to_node
#define cpu_to_node(cpu) 0
#endif

#ifndef this_cpu_set_numa_node
#define this_cpu_set_numa_node(node)
#endif

#ifndef set_cpu_numa_node
#define set_cpu_numa_node(cpu, node)
#endif

#endif /* CONFIG_NUMA */

#endif /* _LINUX_NUMA_H */
