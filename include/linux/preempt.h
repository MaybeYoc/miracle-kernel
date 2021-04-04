#ifndef __LINUX_PREEMPT_H
#define __LINUX_PREEMPT_H

/*
 * include/linux/preempt.h - macros for accessing and manipulating
 * preempt_count (used for kernel preemption, interrupt count, etc.)
 */

#include <linux/linkage.h>
#include <linux/list.h>

#define PREEMPT_DISABLED	(1 + PREEMPT_ENABLED)

/*
 * We use the MSB mostly because its available; see <linux/preempt_mask.h> for
 * the other bits -- can't include that header due to inclusion hell.
 */
#define PREEMPT_NEED_RESCHED	0x80000000

#include <asm/preempt.h>

#define preempt_count_add(val)	__preempt_count_add(val)
#define preempt_count_sub(val)	__preempt_count_sub(val)
#define preempt_count_dec_and_test() __preempt_count_dec_and_test()

#define __preempt_count_inc() __preempt_count_add(1)
#define __preempt_count_dec() __preempt_count_sub(1)

#define preempt_count_inc() preempt_count_add(1)
#define preempt_count_dec() preempt_count_sub(1)

#define preempt_disable() \
do { \
	preempt_count_inc(); \
	barrier(); \
} while (0)

#define sched_preempt_enable_no_resched() \
do { \
	barrier(); \
	preempt_count_dec(); \
} while (0)

#define preempt_enable_no_resched() sched_preempt_enable_no_resched()

#define preempt_enable() \
do { \
	barrier(); \
	preempt_count_dec(); \
} while (0)
#define preempt_check_resched() do { } while (0)

#define preempt_disable_notrace() \
do { \
	__preempt_count_inc(); \
	barrier(); \
} while (0)

#define preempt_enable_no_resched_notrace() \
do { \
	barrier(); \
	__preempt_count_dec(); \
} while (0)

#define preempt_enable_notrace() \
do { \
	barrier(); \
	__preempt_count_dec(); \
} while (0)

#define preemptible()				0

/* TODO */
#define in_irq()		(0)
#define in_softirq()		(0)
#define in_interrupt()		(0)
#define in_serving_softirq()	(0)
#define in_nmi()		(0)
#define in_task()		(0)

#endif /* __LINUX_PREEMPT_H */
