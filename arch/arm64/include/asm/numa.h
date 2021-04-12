/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_NUMA_H
#define __ASM_NUMA_H

#include <linux/compiler.h>
#include <linux/nodemask.h>
#include <linux/cpumask.h>
#include <linux/init.h>

extern nodemask_t numa_nodes_parsed;

cpumask_t *__cpumask_of_node(int node);
#define cpumask_of_node(node) __cpumask_of_node(node)

int __node_distance(int from, int to);
#define node_distance(a, b) __node_distance(a, b)

int __cpu_to_node(int cpu);
#define cpu_to_node(cpu) __cpu_to_node(cpu)

void __set_cpu_numa_node(int cpu, int node);
#define set_cpu_numa_node(cpu, node) __set_cpu_numa_node(cpu, node)

void __init arm64_numa_init(void);
int __init numa_add_memblk(int nodeid, u64 start, u64 end);
void __init numa_set_distance(int from, int to, int distance);

void numa_add_cpu(unsigned int cpu);
void numa_remove_cpu(unsigned int cpu);
void numa_clear_node(unsigned int cpu);

#endif	/* __ASM_NUMA_H */
