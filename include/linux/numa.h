/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H

#include <asm/numa.h>

#ifdef CONFIG_NODES_SHIFT
#define NODES_SHIFT     CONFIG_NODES_SHIFT
#else
#define NODES_SHIFT     0
#endif

#define MAX_NUMNODES    (1 << NODES_SHIFT)

#define	NUMA_NO_NODE	(-1)

/* Returns the number of the current Node. */
#ifndef numa_node_id
static inline int numa_node_id(void)
{
#ifdef CONFIG_NUMA
	// return cpu_to_node(raw_smp_processor_id());
#endif
	return 0;
}
#endif

#endif /* _LINUX_NUMA_H */
