/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_PERCPU_H_
#define _ASM_GENERIC_PERCPU_H_

#include <linux/compiler.h>
#include <linux/threads.h>
#include <linux/percpu-defs.h>

#ifdef CONFIG_SMP

/*
 * per_cpu_offset() is the offset that has to be added to a
 * percpu variable to get to the instance for a certain processor.
 *
 * Most arches use the __per_cpu_offset array for those offsets but
 * some arches have their own ways of determining the offset (x86_64, s390).
 */
#ifndef __per_cpu_offset
extern unsigned long __per_cpu_offset[NR_CPUS];

#define per_cpu_offset(x) (__per_cpu_offset[x])
#endif

/*
 * Determine the offset for the currently active processor.
 * An arch may define __my_cpu_offset to provide a more effective
 * means of obtaining the offset to the per cpu variables of the
 * current processor.
 */
#ifndef __my_cpu_offset
#define __my_cpu_offset per_cpu_offset(raw_smp_processor_id())
#endif
#ifdef CONFIG_DEBUG_PREEMPT
#define my_cpu_offset per_cpu_offset(smp_processor_id())
#else
#define my_cpu_offset __my_cpu_offset
#endif

/*
 * Arch may define arch_raw_cpu_ptr() to provide more efficient address
 * translations for raw_cpu_ptr().
 */
#ifndef arch_raw_cpu_ptr
/* TODO */
//#define arch_raw_cpu_ptr(ptr) SHIFT_PERCPU_PTR(ptr, __my_cpu_offset)
#define arch_raw_cpu_ptr(ptr) SHIFT_PERCPU_PTR(ptr, per_cpu_offset(0))
#endif

#endif	/* SMP */

#ifndef PER_CPU_BASE_SECTION
#ifdef CONFIG_SMP
#define PER_CPU_BASE_SECTION ".data..percpu"
#else
#define PER_CPU_BASE_SECTION ".data"
#endif
#endif

#ifndef PER_CPU_ATTRIBUTES
#define PER_CPU_ATTRIBUTES
#endif

#define raw_cpu_generic_read(pcp)					\
({									\
	*raw_cpu_ptr(&(pcp));						\
})

#define raw_cpu_generic_to_op(pcp, val, op)				\
do {									\
	*raw_cpu_ptr(&(pcp)) op val;					\
} while (0)

#define raw_cpu_generic_add_return(pcp, val)				\
({									\
	typeof(&(pcp)) __p = raw_cpu_ptr(&(pcp));			\
									\
	*__p += val;							\
	*__p;								\
})

#define raw_cpu_generic_xchg(pcp, nval)					\
({									\
	typeof(&(pcp)) __p = raw_cpu_ptr(&(pcp));			\
	typeof(pcp) __ret;						\
	__ret = *__p;							\
	*__p = nval;							\
	__ret;								\
})

#define raw_cpu_generic_cmpxchg(pcp, oval, nval)			\
({									\
	typeof(&(pcp)) __p = raw_cpu_ptr(&(pcp));			\
	typeof(pcp) __ret;						\
	__ret = *__p;							\
	if (__ret == (oval))						\
		*__p = nval;						\
	__ret;								\
})

#define raw_cpu_generic_cmpxchg_double(pcp1, pcp2, oval1, oval2, nval1, nval2) \
({									\
	typeof(&(pcp1)) __p1 = raw_cpu_ptr(&(pcp1));			\
	typeof(&(pcp2)) __p2 = raw_cpu_ptr(&(pcp2));			\
	int __ret = 0;							\
	if (*__p1 == (oval1) && *__p2  == (oval2)) {			\
		*__p1 = nval1;						\
		*__p2 = nval2;						\
		__ret = 1;						\
	}								\
	(__ret);							\
})

#endif /* _ASM_GENERIC_PERCPU_H_ */
