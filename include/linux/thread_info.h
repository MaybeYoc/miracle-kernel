/* thread_info.h: common low-level thread information accessors
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds
 */

#ifndef _LINUX_THREAD_INFO_H
#define _LINUX_THREAD_INFO_H

#include <linux/types.h>
#include <linux/bug.h>
#include <linux/bitops.h>

#include <asm/thread_info.h>

#ifdef __KERNEL__
/*
 * flag set/clear/test wrappers
 * - pass TIF_xxxx constants to these functions
 */

static inline void set_ti_thread_flag(struct thread_info *ti, int flag)
{
	set_bit(flag, (unsigned long *)&ti->flags);
}

static inline void clear_ti_thread_flag(struct thread_info *ti, int flag)
{
	clear_bit(flag, (unsigned long *)&ti->flags);
}

static inline void update_ti_thread_flag(struct thread_info *ti, int flag,
					 bool value)
{
	if (value)
		set_ti_thread_flag(ti, flag);
	else
		clear_ti_thread_flag(ti, flag);
}

static inline int test_and_set_ti_thread_flag(struct thread_info *ti, int flag)
{
	return test_and_set_bit(flag, (unsigned long *)&ti->flags);
}

static inline int test_and_clear_ti_thread_flag(struct thread_info *ti, int flag)
{
	return test_and_clear_bit(flag, (unsigned long *)&ti->flags);
}

static inline int test_ti_thread_flag(struct thread_info *ti, int flag)
{
	return test_bit(flag, (unsigned long *)&ti->flags);
}

#define set_thread_flag(flag) \
	set_ti_thread_flag(current_thread_info(), flag)
#define clear_thread_flag(flag) \
	clear_ti_thread_flag(current_thread_info(), flag)
#define test_and_set_thread_flag(flag) \
	test_and_set_ti_thread_flag(current_thread_info(), flag)
#define test_and_clear_thread_flag(flag) \
	test_and_clear_ti_thread_flag(current_thread_info(), flag)
#define test_thread_flag(flag) \
	test_ti_thread_flag(current_thread_info(), flag)

#define tif_need_resched() test_thread_flag(TIF_NEED_RESCHED)

#if defined TIF_RESTORE_SIGMASK && !defined HAVE_SET_RESTORE_SIGMASK
/*
 * An arch can define its own version of set_restore_sigmask() to get the
 * job done however works, with or without TIF_RESTORE_SIGMASK.
 */
#define HAVE_SET_RESTORE_SIGMASK	1

/**
 * set_restore_sigmask() - make sure saved_sigmask processing gets done
 *
 * This sets TIF_RESTORE_SIGMASK and ensures that the arch signal code
 * will run before returning to user mode, to process the flag.  For
 * all callers, TIF_SIGPENDING is already set or it's no harm to set
 * it.  TIF_RESTORE_SIGMASK need not be in the set of bits that the
 * arch code will notice on return to user mode, in case those bits
 * are scarce.  We set TIF_SIGPENDING here to ensure that the arch
 * signal code always gets run when TIF_RESTORE_SIGMASK is set.
 */
static inline void set_restore_sigmask(void)
{
	set_thread_flag(TIF_RESTORE_SIGMASK);
	WARN_ON(!test_thread_flag(TIF_SIGPENDING));
}
static inline void clear_restore_sigmask(void)
{
	clear_thread_flag(TIF_RESTORE_SIGMASK);
}
static inline bool test_restore_sigmask(void)
{
	return test_thread_flag(TIF_RESTORE_SIGMASK);
}
static inline bool test_and_clear_restore_sigmask(void)
{
	return test_and_clear_thread_flag(TIF_RESTORE_SIGMASK);
}
#endif	/* TIF_RESTORE_SIGMASK && !HAVE_SET_RESTORE_SIGMASK */

#ifndef HAVE_SET_RESTORE_SIGMASK
#error "no set_restore_sigmask() provided and default one won't work"
#endif

#endif	/* __KERNEL__ */

#endif /* _LINUX_THREAD_INFO_H */
