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

/**
 * cpumask_pr_args - printf args to output a cpumask
 * @maskp: cpumask to be printed
 *
 * Can be used to provide arguments for '%*pb[l]' when printing a cpumask.
 */
#define cpumask_pr_args(maskp)		nr_possible_cpu_ids, cpumask_bits(maskp)

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

static inline unsigned int cpumask_next_and(int cpu, const cpumask_t *src1p,
		    const cpumask_t *src2p)
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

static inline int cpumask_is_set(int cpu, const cpumask_t *srcp)
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

static inline int cpumask_weight(const cpumask_t *srcp)
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
static inline bool cpumask_intersects(const cpumask_t *src1p, const cpumask_t *src2p)
{
	return bitmap_intersects(cpumask_bits(src1p), cpumask_bits(src2p),
						      nr_cpumask_bits);
}

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

/**
 * cpumask_size - size to allocate for a 'struct cpumask' in bytes
 */
static inline unsigned int cpumask_size(void)
{
	return BITS_TO_LONGS(nr_cpumask_bits) * sizeof(long);
}

/**
 * cpumask_any - pick a "random" cpu from *srcp
 * @srcp: the input cpumask
 *
 * Returns >= nr_possible_cpu_ids if no cpus set.
 */
#define cpumask_any(srcp) cpumask_first(srcp)

/**
 * cpumask_first_and - return the first cpu from *srcp1 & *srcp2
 * @src1p: the first input
 * @src2p: the second input
 *
 * Returns >= nr_possible_cpu_ids if no cpus set in both.  See also cpumask_next_and().
 */
#define cpumask_first_and(src1p, src2p) cpumask_next_and(-1, (src1p), (src2p))

/**
 * cpumask_any_and - pick a "random" cpu from *mask1 & *mask2
 * @mask1: the first input cpumask
 * @mask2: the second input cpumask
 *
 * Returns >= nr_possible_cpu_ids if no cpus set.
 */
#define cpumask_any_and(mask1, mask2) cpumask_first_and((mask1), (mask2))

/**
 * cpumask_of - the cpumask containing just a given cpu
 * @cpu: the cpu (<= nr_possible_cpu_ids)
 */
#define cpumask_of(cpu) (get_cpu_mask(cpu))

/**
 * to_cpumask - convert an NR_CPUS bitmap to a struct cpumask *
 * @bitmap: the bitmap
 *
 * There are a few places where cpumask_var_t isn't appropriate and
 * static cpumasks must be used (eg. very early boot), yet we don't
 * expose the definition of 'struct cpumask'.
 *
 * This does the conversion, and can be used as a constant initializer.
 */
#define to_cpumask(bitmap)						\
	((struct cpumask *)(1 ? (bitmap)				\
			    : (void *)sizeof(__check_is_bitmap(bitmap))))

static inline int __check_is_bitmap(const unsigned long *bitmap)
{
	return 1;
}

/*
 * Special-case data structure for "single bit set only" constant CPU masks.
 *
 * We pre-generate all the 64 (or 32) possible bit positions, with enough
 * padding to the left and the right, and return the constant pointer
 * appropriately offset.
 */
extern const unsigned long
	cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(NR_CPUS)];

static inline const struct cpumask *get_cpu_mask(unsigned int cpu)
{
	const unsigned long *p = cpu_bit_bitmap[1 + cpu % BITS_PER_LONG];
	p -= cpu / BITS_PER_LONG;
	return to_cpumask(p);
}

#define cpu_is_offline(cpu)	unlikely(!cpu_online(cpu))

/**
 * cpumask_complement - *dstp = ~*srcp
 * @dstp: the cpumask result
 * @srcp: the input to invert
 */
static inline void cpumask_complement(struct cpumask *dstp,
				      const struct cpumask *srcp)
{
	bitmap_complement(cpumask_bits(dstp), cpumask_bits(srcp),
					      nr_cpumask_bits);
}

/**
 * cpumask_equal - *src1p == *src2p
 * @src1p: the first input
 * @src2p: the second input
 */
static inline bool cpumask_equal(const struct cpumask *src1p,
				const struct cpumask *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p),
						 nr_cpumask_bits);
}

/**
 * cpumask_subset - (*src1p & ~*src2p) == 0
 * @src1p: the first input
 * @src2p: the second input
 *
 * Returns 1 if *@src1p is a subset of *@src2p, else returns 0
 */
static inline int cpumask_subset(const struct cpumask *src1p,
				 const struct cpumask *src2p)
{
	return bitmap_subset(cpumask_bits(src1p), cpumask_bits(src2p),
						  nr_cpumask_bits);
}

#if NR_CPUS <= BITS_PER_LONG
#define CPU_BITS_ALL						\
{								\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
}

#else /* NR_CPUS > BITS_PER_LONG */

#define CPU_BITS_ALL						\
{								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-2] = ~0UL,		\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
}
#endif /* NR_CPUS > BITS_PER_LONG */

#if NR_CPUS <= BITS_PER_LONG
#define CPU_MASK_ALL							\
(cpumask_t) { {								\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
} }
#else
#define CPU_MASK_ALL							\
(cpumask_t) { {								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-2] = ~0UL,			\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
} }
#endif /* NR_CPUS > BITS_PER_LONG */

#define CPU_MASK_NONE							\
(cpumask_t) { {								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-1] =  0UL				\
} }

#define CPU_MASK_CPU0							\
(cpumask_t) { {								\
	[0] =  1UL							\
} }

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

/* It's common to want to use cpu_all_mask in struct member initializers,
 * so it has to refer to an address rather than a pointer. */
extern const DECLARE_BITMAP(cpu_all_bits, NR_CPUS);
#define cpu_all_mask to_cpumask(cpu_all_bits)

/* First bits of cpu_bit_bitmap are in fact unset. */
#define cpu_none_mask to_cpumask(cpu_bit_bitmap[0])

#define for_each_possible_cpu(cpu) for_each_cpu_mask(cpu, cpu_possible_mask)
#define for_each_online_cpu(cpu) for_each_cpu_mask(cpu, cpu_online_mask)
#define for_each_present_cpu(cpu) for_each_cpu_mask(cpu, cpu_present_mask)
#define for_each_active_cpu(cpu) for_each_cpu_mask(cpu, cpu_active_mask)

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

static inline void cpu_clear_active(int cpu)
{
	cpumask_clear_cpu(cpu, cpu_active_mask);
	nr_active_cpu_ids = cpumask_weight(cpu_active_mask);
}

#define cpu_online(cpu)		cpumask_is_set((cpu), cpu_online_mask)
#define cpu_possible(cpu)	cpumask_is_set((cpu), cpu_possible_mask)
#define cpu_present(cpu)	cpumask_is_set((cpu), cpu_present_mask)
#define cpu_active(cpu)		cpumask_is_set((cpu), cpu_active_mask)

bool alloc_cpumask_var_node(cpumask_t **mask, gfp_t flags, int node);
bool alloc_cpumask_var(cpumask_t **mask, gfp_t flags);
bool zalloc_cpumask_var_node(cpumask_t **mask, gfp_t flags, int node);
bool zalloc_cpumask_var(cpumask_t **mask, gfp_t flags);
void free_cpumask_var(cpumask_t *mask);

#endif /* __LINUX_CPUMASK_H */
