/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_NODEMASK_H
#define __LINUX_NODEMASK_H

#include <linux/bitmap.h>
#include <linux/numa.h>

typedef struct { DECLARE_BITMAP(bits, MAX_NUMNODES); } nodemask_t;

/**
 * nodemask_bits - get the bits in a cpumask
 * @maskp: the struct cpumask *
 *
 * You should only assume nr_possible_cpu_ids bits of this mask are valid.  This is
 * a macro so it's const-correct.
 */
#define nodemask_bits(maskp) ((maskp)->bits)

#define nr_nodemask_bits ((unsigned int)MAX_NUMNODES)

extern nodemask_t __node_possible_mask;
extern nodemask_t __node_online_mask;

#define node_possible_mask	((nodemask_t *)&__node_possible_mask)
#define node_online_mask	((nodemask_t *)&__node_online_mask)

static inline unsigned int nodemask_first(nodemask_t *srcp)
{
	return find_first_bit(nodemask_bits(srcp), nr_nodemask_bits);
}

static inline unsigned int nodemask_last(nodemask_t *srcp)
{
	return find_last_bit(nodemask_bits(srcp), nr_nodemask_bits);
}

static inline unsigned int nodemask_next(int n, nodemask_t *srcp)
{
	return find_next_bit(nodemask_bits(srcp), nr_nodemask_bits, n + 1);
}

static inline void nodemask_set_node(int node, nodemask_t *dstp)
{
	set_bit(node, nodemask_bits(dstp));
}

static inline void nodemask_clear_node(int node, nodemask_t *dstp)
{
	clear_bit(node, nodemask_bits(dstp));
}

static inline void nodemask_setall_node(nodemask_t *dstp)
{
	bitmap_fill(nodemask_bits(dstp), nr_nodemask_bits);
}

static inline void nodemask_clearall_node(nodemask_t *dstp)
{
	bitmap_zero(nodemask_bits(dstp), nr_nodemask_bits);
}

static inline int nodemask_is_set(int node, nodemask_t *srcp)
{
	return test_bit(node, nodemask_bits(srcp));
}

static inline int nodemask_test_and_set(int node, nodemask_t *dstp)
{
	return test_and_set_bit(node, nodemask_bits(dstp));
}

static inline int nodemask_empty(nodemask_t *srcp)
{
	return bitmap_empty(nodemask_bits(srcp), nr_nodemask_bits);
}

static inline int nodemask_full(nodemask_t *srcp)
{
	return bitmap_full(nodemask_bits(srcp), nr_nodemask_bits);
}

static inline int nodemask_weight(nodemask_t *srcp)
{
	return bitmap_weight(nodemask_bits(srcp), nr_nodemask_bits);
}

#if MAX_NUMNODES > 1
#define for_each_node_mask(node, mask)		\
	for ((node) = nodemask_first(mask);	\
		(node) < MAX_NUMNODES;		\
		(node) = nodemask_next((node), (mask)))
#else
#define for_each_node_mask(node, mask)		\
	for ((node) = 0; (node) < 1; (node)++)
#endif /* MAX_NUMNODES */

#define for_each_possible_node(node) for_each_node_mask(node, node_possible_mask)
#define for_each_online_node(node) for_each_node_mask(node, node_online_mask)

#if MAX_NUMNODES > 1
#define first_possible_node nodemask_first(node_possible_mask)
#define first_online_node	nodemask_first(node_online_mask)

#define next_possible_node(node) nodemask_next(node, node_possible_mask);
#define next_online_node(node) nodemask_next(node, node_online_mask);

#define node_possible(node) nodemask_is_set(node, node_possible_mask)
#define node_online(node) nodemask_is_set(node, node_online_mask)

extern int nr_possible_nodes;
extern int nr_online_nodes;

static inline void node_set_possible(int node)
{
	nodemask_set_node(node, node_possible_mask);
	nr_possible_nodes = nodemask_weight(node_possible_mask);
}

static inline void node_set_online(int node)
{
	nodemask_set_node(node, node_online_mask);
	nr_online_nodes = nodemask_weight(node_online_mask);
}

#else

#define first_possible_node 0
#define first_online_node 0

#define next_possible_node(node) nr_nodemask_bits
#define next_online_node(node) nr_nodemask_bits

#define node_possible(node) ((node) == 0)
#define node_online(node) ((node) == 0)

#define nr_possible_nodes 1
#define nr_online_nodes 1

static inline void node_set_possible(int node) {}
static inline void node_set_online(int node) {}

#endif /* MAX_NUMNODES > 1 */

#endif /* __LINUX_NODEMASK_H */
