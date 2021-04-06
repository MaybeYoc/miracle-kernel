/* Atomic operations usable in machine independent code */
#ifndef _LINUX_ATOMIC_H
#define _LINUX_ATOMIC_H
#include <asm/atomic.h>

/* cmpxchg_relaxed */
#ifndef cmpxchg_relaxed
#define  cmpxchg_relaxed		cmpxchg
#define  cmpxchg_acquire		cmpxchg
#define  cmpxchg_release		cmpxchg
#endif

/* cmpxchg64_relaxed */
#ifndef cmpxchg64_relaxed
#define  cmpxchg64_relaxed		cmpxchg64
#define  cmpxchg64_acquire		cmpxchg64
#define  cmpxchg64_release		cmpxchg64
#endif

/* xchg_relaxed */
#ifndef xchg_relaxed
#define  xchg_relaxed			xchg
#define  xchg_acquire			xchg
#define  xchg_release			xchg
#endif

#ifndef atomic_read_acquire
#define  atomic_read_acquire(v)		smp_load_acquire(&(v)->counter)
#endif

#ifndef atomic_set_release
#define  atomic_set_release(v, i)	smp_store_release(&(v)->counter, (i))
#endif

/*
 * The idea here is to build acquire/release variants by adding explicit
 * barriers on top of the relaxed variant. In the case where the relaxed
 * variant is already fully ordered, no additional barriers are needed.
 *
 * If an architecture overrides __atomic_acquire_fence() it will probably
 * want to define smp_mb__after_spinlock().
 */
#ifndef __atomic_acquire_fence
#define __atomic_acquire_fence		smp_mb__after_atomic
#endif

#ifndef __atomic_release_fence
#define __atomic_release_fence		smp_mb__before_atomic
#endif

#ifndef __atomic_pre_full_fence
#define __atomic_pre_full_fence		smp_mb__before_atomic
#endif

#ifndef __atomic_post_full_fence
#define __atomic_post_full_fence	smp_mb__after_atomic
#endif

/**
 * atomic_add_unless - add unless the number is already a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as @v was not already @u.
 * Returns non-zero if @v was not @u, and zero otherwise.
 */
static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	return __atomic_add_unless(v, a, u) != u;
}

/**
 * atomic_inc_not_zero - increment unless the number is zero
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1, so long as @v is non-zero.
 * Returns non-zero if @v was non-zero, and zero otherwise.
 */
#ifndef atomic_inc_not_zero
#define atomic_inc_not_zero(v)		atomic_add_unless((v), 1, 0)
#endif

/**
 * atomic_inc_not_zero_hint - increment if not null
 * @v: pointer of type atomic_t
 * @hint: probable value of the atomic before the increment
 *
 * This version of atomic_inc_not_zero() gives a hint of probable
 * value of the atomic. This helps processor to not read the memory
 * before doing the atomic read/modify/write cycle, lowering
 * number of bus transactions on some arches.
 *
 * Returns: 0 if increment was not done, 1 otherwise.
 */
#ifndef atomic_inc_not_zero_hint
static inline int atomic_inc_not_zero_hint(atomic_t *v, int hint)
{
	int val, c = hint;

	/* sanity test, should be removed by compiler if hint is a constant */
	if (!hint)
		return atomic_inc_not_zero(v);

	do {
		val = atomic_cmpxchg(v, c, c + 1);
		if (val == c)
			return 1;
		c = val;
	} while (c);

	return 0;
}
#endif

#ifndef atomic_inc_unless_negative
static inline int atomic_inc_unless_negative(atomic_t *p)
{
	int v, v1;
	for (v = 0; v >= 0; v = v1) {
		v1 = atomic_cmpxchg(p, v, v + 1);
		if (likely(v1 == v))
			return 1;
	}
	return 0;
}
#endif

#ifndef atomic_dec_unless_positive
static inline int atomic_dec_unless_positive(atomic_t *p)
{
	int v, v1;
	for (v = 0; v <= 0; v = v1) {
		v1 = atomic_cmpxchg(p, v, v - 1);
		if (likely(v1 == v))
			return 1;
	}
	return 0;
}
#endif

/*
 * atomic_dec_if_positive - decrement by 1 if old value positive
 * @v: pointer of type atomic_t
 *
 * The function returns the old value of *v minus 1, even if
 * the atomic variable, v, was not decremented.
 */
#ifndef atomic_dec_if_positive
static inline int atomic_dec_if_positive(atomic_t *v)
{
	int c, old, dec;
	c = atomic_read(v);
	for (;;) {
		dec = c - 1;
		if (unlikely(dec < 0))
			break;
		old = atomic_cmpxchg((v), c, dec);
		if (likely(old == c))
			break;
		c = old;
	}
	return dec;
}
#endif

#ifndef CONFIG_ARCH_HAS_ATOMIC_OR
static inline void atomic_or(int i, atomic_t *v)
{
	int old;
	int new;

	do {
		old = atomic_read(v);
		new = old | i;
	} while (atomic_cmpxchg(v, old, new) != old);
}
#endif /* #ifndef CONFIG_ARCH_HAS_ATOMIC_OR */

#include <asm-generic/atomic-long.h>
#ifdef CONFIG_GENERIC_ATOMIC64
#include <asm-generic/atomic64.h>
#endif
#endif /* _LINUX_ATOMIC_H */
