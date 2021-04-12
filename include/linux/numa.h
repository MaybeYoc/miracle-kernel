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

#include <linux/smp.h>

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
#define RECLAIM_DISTANCE 30
#endif

#ifndef PENALTY_FOR_NODE_WITH_CPUS
#define PENALTY_FOR_NODE_WITH_CPUS	(1)
#endif

#ifndef cpu_to_node
#define cpu_to_node(cpu)	0
#endif

#ifndef set_cpu_numa_node
#define set_cpu_numa_node(cpu, node)
#endif

#ifndef this_cpu_numa_node_id
static inline int this_cpu_numa_node_id(void)
{
	return cpu_to_node(smp_processor_id());
}
#endif

#ifndef this_cpu_set_numa_node
static inline void this_cpu_set_numa_node(int node)
{
	set_cpu_numa_node(smp_processor_id(), node);
}
#endif

#endif /* _LINUX_NUMA_H */
