/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_TASK_H
#define _LINUX_SCHED_TASK_H

/*
 * Interface between the scheduler and various task lifetime (fork()/exit())
 * functionality:
 */

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

static inline void put_task_struct(struct task_struct *t)
{

}

static inline __printf(4, 5)
struct task_struct *kthread_create_on_node(int (*threadfn)(void *data),
					   void *data,
					   int node,
					   const char namefmt[], ...) { return NULL; }

#define kthread_create(threadfn, data, namefmt, arg...) \
	kthread_create_on_node(threadfn, data, NUMA_NO_NODE, namefmt, ##arg)

/* TODO */
static inline int kthread_stop(struct task_struct *k) { return 0; }

static inline int wake_up_process(struct task_struct *p) { return 0; }

#define get_task_struct(tsk) do { atomic_inc(&(tsk)->usage); } while(0)

static inline int sched_setscheduler_nocheck(struct task_struct *p, int policy,
			       const struct sched_param *param) {return 0;}

/* Attach to any functions which should be ignored in wchan output. */
#define __sched		__attribute__((__section__(".sched.text")))

/* Linker adds these: start and end of __sched functions */
extern char __sched_text_start[], __sched_text_end[];

#endif /* _LINUX_SCHED_TASK_H */
