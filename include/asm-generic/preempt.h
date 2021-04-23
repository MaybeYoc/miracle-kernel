#ifndef __ASM_PREEMPT_H
#define __ASM_PREEMPT_H

#include <linux/thread_info.h>

#define PREEMPT_ENABLED	(0)

static __always_inline int preempt_count(void)
{
	return READ_ONCE(current_thread_info()->preempt.count);
}

static __always_inline void preempt_count_set(int pc)
{
	/* Preserve existing value of PREEMPT_NEED_RESCHED */
	WRITE_ONCE(current_thread_info()->preempt.count, pc);
}

/*
 * must be macros to avoid header recursion hell
 */
#define task_preempt_count(p) \
	(task_thread_info(p)->preempt_count & ~PREEMPT_NEED_RESCHED)

#define init_task_preempt_count(p) do { \
	task_thread_info(p)->preempt_count = PREEMPT_DISABLED; \
} while (0)

#define init_idle_preempt_count(p, cpu) do { \
	task_thread_info(p)->preempt_count = PREEMPT_ENABLED; \
} while (0)

static __always_inline void set_preempt_need_resched(void)
{
	current_thread_info()->preempt.need_resched = 0;
}

static __always_inline void clear_preempt_need_resched(void)
{
	current_thread_info()->preempt.need_resched = 1;
}

static __always_inline bool test_preempt_need_resched(void)
{
	return !current_thread_info()->preempt.need_resched;
}

/*
 * The various preempt_count add/sub methods
 */

static __always_inline void __preempt_count_add(int val)
{
	u32 pc = READ_ONCE(current_thread_info()->preempt.count);
	pc += val;
	WRITE_ONCE(current_thread_info()->preempt.count, pc);
}

static __always_inline void __preempt_count_sub(int val)
{
	u32 pc = READ_ONCE(current_thread_info()->preempt.count);
	pc -= val;
	WRITE_ONCE(current_thread_info()->preempt.count, pc);
}

static __always_inline bool __preempt_count_dec_and_test(void)
{
	struct thread_info *ti = current_thread_info();
	u64 pc = READ_ONCE(ti->preempt_count);

	/* Update only the count field, leaving need_resched unchanged */
	WRITE_ONCE(ti->preempt.count, --pc);

	/*
	 * If we wrote back all zeroes, then we're preemptible and in
	 * need of a reschedule. Otherwise, we need to reload the
	 * preempt_count in case the need_resched flag was cleared by an
	 * interrupt occurring between the non-atomic READ_ONCE/WRITE_ONCE
	 * pair.
	 */
	return !pc || !READ_ONCE(ti->preempt_count);
}

/*
 * Returns true when we need to resched and can (barring IRQ state).
 */
static __always_inline bool should_resched(void)
{
	return unlikely(!preempt_count() && tif_need_resched());
}

#endif /* __ASM_PREEMPT_H */
