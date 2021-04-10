/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_TASK_STACK_H
#define _LINUX_SCHED_TASK_STACK_H

/*
 * task->stack (kernel stack) handling interfaces:
 */

#include <linux/sched.h>
#include <linux/magic.h>

static inline unsigned long *end_of_stack(const struct task_struct *task)
{
	return task->stack;
}

extern void set_task_stack_end_magic(struct task_struct *tsk);

#endif /* _LINUX_SCHED_TASK_STACK_H */
