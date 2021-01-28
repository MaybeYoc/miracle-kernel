#ifndef __LINUX_SPINLOCK_UP_H
#define __LINUX_SPINLOCK_UP_H

#ifndef __LINUX_SPINLOCK_H
# error "please don't include this file directly"
#endif

#include <asm/processor.h>	/* for cpu_relax() */

/*
 * include/linux/spinlock_up.h - UP-debug version of spinlocks.
 *
 * portions Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Released under the General Public License (GPL).
 *
 * In the debug case, 1 means unlocked, 0 means locked. (the values
 * are inverted, to catch initialization bugs)
 *
 * No atomicity anywhere, we are on UP. However, we still need
 * the compiler barriers, because we do not want the compiler to
 * move potentially faulting instructions (notably user accesses)
 * into the locked sequence, resulting in non-atomic execution.
 */
#define arch_spin_is_locked(lock)	((void)(lock), 0)
/* for sched/core.c and kernel_lock.c: */
# define arch_spin_lock(lock)		do { barrier(); (void)(lock); } while (0)
# define arch_spin_lock_flags(lock, flags)	do { barrier(); (void)(lock); } while (0)
# define arch_spin_unlock(lock)	do { barrier(); (void)(lock); } while (0)
# define arch_spin_trylock(lock)	({ barrier(); (void)(lock); 1; })

#define arch_spin_is_contended(lock)	(((void)(lock), 0))

#define arch_read_can_lock(lock)	(((void)(lock), 1))
#define arch_write_can_lock(lock)	(((void)(lock), 1))

#define arch_spin_unlock_wait(lock) \
		do { cpu_relax(); } while (arch_spin_is_locked(lock))

#endif /* __LINUX_SPINLOCK_UP_H */
