/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_CPUMASK_H
#define __LINUX_CPUMASK_H

/*
 * Cpumasks provide a bitmap suitable for representing the
 * set of CPU's in a system, one bit position per CPU number.  In general,
 * only nr_possible_cpu_ids (<= NR_CPUS) bits are valid.
 */
#include <linux/bitmap.h>
#include <linux/threads.h>

/* Don't assign or return these: may not be this big! */
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;

/**
 * cpumask_bits - get the bits in a cpumask
 * @maskp: the struct cpumask *
 *
 * You should only assume nr_possible_cpu_ids bits of this mask are valid.  This is
 * a macro so it's const-correct.
 */
#define cpumask_bits(maskp) ((maskp)->bits)

/* Assuming NR_CPUS is huge, a runtime limit is more efficient.  Also,
 * not all bits may be allocated. */
#define nr_cpumask_bits	NR_CPUS

extern struct cpumask __cpu_possible_mask;
extern struct cpumask __cpu_online_mask;
extern struct cpumask __cpu_present_mask;
extern struct cpumask __cpu_active_mask;

#define cpu_possible_mask ((cpumask_t *)&__cpu_possible_mask)
#define cpu_online_mask   ((cpumask_t *)&__cpu_online_mask)
#define cpu_present_mask  ((cpumask_t *)&__cpu_present_mask)
#define cpu_active_mask   ((cpumask_t *)&__cpu_active_mask)

static inline unsigned int cpumask_first(const cpumask_t *srcp)
{
	return find_first_bit(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline unsigned int cpumask_first_zero(cpumask_t *srcp)
{
	return find_first_zero_bit(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline unsigned int cpumask_last(cpumask_t *srcp)
{
	return find_last_bit(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline unsigned int cpumask_next(int cpu, const cpumask_t *srcp)
{
	return find_next_bit(cpumask_bits(srcp), nr_cpumask_bits, cpu + 1);
}

static inline unsigned int cpumask_next_zero(int cpu, cpumask_t *srcp)
{
	return find_next_zero_bit(cpumask_bits(srcp), nr_cpumask_bits, cpu + 1);
}

static inline unsigned int cpumask_next_wrap(int cpu, cpumask_t *mask, int start, bool wrap)
{
	int next;

again:
	next = cpumask_next(cpu, mask);

	if (wrap && cpu < start && next >= start) {
		return nr_cpumask_bits;

	} else if (next >= nr_cpumask_bits) {
		wrap = true;
		cpu = -1;
		goto again;
	}

	return next;
}

static inline unsigned int cpumask_next_and(int cpu, cpumask_t *src1p,
		     cpumask_t *src2p)
{
	return find_next_and_bit(cpumask_bits(src1p), cpumask_bits(src2p),
		nr_cpumask_bits, cpu + 1);
}

static inline void cpumask_set_cpu(int cpu, cpumask_t *dstp)
{
	set_bit(cpu, cpumask_bits(dstp));
}

static inline void cpumask_clear_cpu(int cpu, cpumask_t *dstp)
{
	clear_bit(cpu, cpumask_bits(dstp));
}

static inline void cpumask_setall_cpu(cpumask_t *dstp)
{
	bitmap_fill(cpumask_bits(dstp), nr_cpumask_bits);
}

static inline void cpumask_clearall_cpu(cpumask_t *dstp)
{
	bitmap_zero(cpumask_bits(dstp), nr_cpumask_bits);
}

static inline int cpumask_is_set(int cpu, cpumask_t *srcp)
{
	return test_bit(cpu, cpumask_bits(srcp));
}

static inline int cpumask_and(cpumask_t *dstp, cpumask_t *src1p, const cpumask_t *src2p)
{
	return bitmap_and(cpumask_bits(dstp), cpumask_bits(src1p),
				       cpumask_bits(src2p), nr_cpumask_bits);
}

static inline void cpumask_or(cpumask_t *dstp, cpumask_t *src1p, cpumask_t *src2p)
{
	bitmap_or(cpumask_bits(dstp), cpumask_bits(src1p),
				      cpumask_bits(src2p), nr_cpumask_bits);
}

static inline void cpumask_xor(cpumask_t *dstp, cpumask_t *src1p, cpumask_t *src2p)
{
	bitmap_xor(cpumask_bits(dstp), cpumask_bits(src1p),
				       cpumask_bits(src2p), nr_cpumask_bits);
}

static inline int cpumask_test_and_set(int cpu, cpumask_t *dstp)
{
	return test_and_set_bit(cpu, cpumask_bits(dstp));
}

static inline int cpumask_empty(const cpumask_t *srcp)
{
	return bitmap_empty(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline int cpumask_full(cpumask_t *srcp)
{
	return bitmap_full(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline int cpumask_weight(cpumask_t *srcp)
{
	return bitmap_weight(cpumask_bits(srcp), nr_cpumask_bits);
}

static inline void cpumask_copy(cpumask_t *dstp,const cpumask_t *srcp)
{
	bitmap_copy(cpumask_bits(dstp), cpumask_bits(srcp), nr_cpumask_bits);
}

/**
 * cpumask_intersects - (*src1p & *src2p) != 0
 * @src1p: the first input
 * @src2p: the second input
 */
static inline bool cpumask_intersects(cpumask_t *src1p, const cpumask_t *src2p)
{
	return bitmap_intersects(cpumask_bits(src1p), cpumask_bits(src2p),
						      nr_cpumask_bits);
}

#if NR_CPUS > 1
#define for_each_cpu_mask(cpu, mask)		\
	for ((cpu) = cpumask_first(mask);	\
		(cpu) < NR_CPUS;		\
		(cpu) = cpumask_next((cpu), (mask)))

#define for_each_cpu_not_mask(cpu, mask)		\
	for ((cpu) = cpumask_first_zero(mask);	\
		(cpu) < NR_CPUS;		\
		(cpu) = cpumask_next_zero((cpu), (mask)))

#define for_each_cpu_wrap_mask(cpu, mask, start)					\
	for ((cpu) = cpumask_next_wrap((start)-1, (mask), (start), false);	\
	     (cpu) < nr_cpumask_bits;						\
	     (cpu) = cpumask_next_wrap((cpu), (mask), (start), true))

#define for_each_cpu_and_mask(cpu, mask, and)				\
	for ((cpu) = -1;						\
		(cpu) = cpumask_next_and((cpu), (mask), (and)),		\
		(cpu) < nr_possible_cpu_ids;)

#else

#define for_each_cpu_mask(cpu, mask)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)

#define for_each_cpu_not_mask(cpu, mask)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)

#define for_each_cpu_wrap_mask(cpu, mask, start)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask, (void)(start))

#define for_each_cpu_and_mask(cpu, mask, and)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask, (void)and)

#endif /* NR_CPUS > 1 */

#define for_each_possible_cpu(cpu) for_each_cpu_mask(cpu, cpu_possible_mask)
#define for_each_online_cpu(cpu) for_each_cpu_mask(cpu, cpu_online_mask)
#define for_each_present_cpu(cpu) for_each_cpu_mask(cpu, cpu_present_mask)
#define for_each_active_cpu(cpu) for_each_cpu_mask(cpu, cpu_active_mask)

#if NR_CPUS > 1
extern unsigned int nr_possible_cpu_ids;
extern unsigned int nr_online_cpu_ids;
extern unsigned int nr_present_cpu_ids;
extern unsigned int nr_active_cpu_ids;

static inline void cpu_set_possible(int cpu)
{
	cpumask_set_cpu(cpu, cpu_possible_mask);
	nr_possible_cpu_ids = cpumask_weight(cpu_possible_mask);
}

static inline void cpu_set_online(int cpu)
{
	cpumask_set_cpu(cpu, cpu_online_mask);
	nr_online_cpu_ids = cpumask_weight(cpu_online_mask);
}

static inline void cpu_set_present(int cpu)
{
	cpumask_set_cpu(cpu, cpu_present_mask);
	nr_present_cpu_ids = cpumask_weight(cpu_present_mask);
}

static inline void cpu_set_active(int cpu)
{
	cpumask_set_cpu(cpu, cpu_active_mask);
	nr_active_cpu_ids = cpumask_weight(cpu_active_mask);
}

#define num_online_cpus()	cpumask_weight(cpu_online_mask)
#define num_possible_cpus()	cpumask_weight(cpu_possible_mask)
#define num_present_cpus()	cpumask_weight(cpu_present_mask)
#define num_active_cpus()	cpumask_weight(cpu_active_mask)
#define cpu_online(cpu)		cpumask_is_set((cpu), cpu_online_mask)
#define cpu_possible(cpu)	cpumask_is_set((cpu), cpu_possible_mask)
#define cpu_present(cpu)	cpumask_is_set((cpu), cpu_present_mask)
#define cpu_active(cpu)		cpumask_is_set((cpu), cpu_active_mask)
#else
#define nr_possible_cpu_ids		1U
#define nr_online_cpu_ids		1U
#define nr_present_cpu_ids		1U
#define nr_active_cpu_ids		1U

static inline void cpu_set_possible(int cpu) {}
static inline void cpu_set_online(int cpu) {}
static inline void cpu_set_present(int cpu) {}
static inline void cpu_set_active(int cpu) {}

#define num_online_cpus()	1U
#define num_possible_cpus()	1U
#define num_present_cpus()	1U
#define num_active_cpus()	1U
#define cpu_online(cpu)		((cpu) == 0)
#define cpu_possible(cpu)	((cpu) == 0)
#define cpu_present(cpu)	((cpu) == 0)
#define cpu_active(cpu)		((cpu) == 0)

#endif /* NR_CPUS > 1 */

/**
 * cpulist_parse - extract a cpumask from a user string of ranges
 * @buf: the buffer to extract from
 * @dstp: the cpumask to set.
 *
 * Returns -errno, or 0 for success.
 */
static inline int cpulist_parse(const char *buf, cpumask_t *dstp)
{
	return bitmap_parselist(buf, cpumask_bits(dstp), nr_cpumask_bits);
}

#endif /* __LINUX_CPUMASK_H */
