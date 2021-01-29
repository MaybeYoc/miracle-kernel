#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#include <linux/mm_types.h>
#include <uapi/linux/sched.h>

#include <asm/thread_info.h>

#define PREEMPT_DISABLED	(1 + PREEMPT_ENABLED)

struct task_struct {
	struct thread_info		thread_info;
	volatile long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	void *stack;
	atomic_t usage;
	unsigned int flags;	/* per process flags, defined below */
	unsigned int ptrace;

	int on_rq;

	struct mm_struct *mm, *active_mm;

	/* per-thread vma caching */
	u32 vmacache_seqnum;

	/* task state */
	int exit_state;
	int exit_code, exit_signal;

	/* Used for emulating ABI behavior of previous Linux versions */
	unsigned int personality;

	unsigned long atomic_flags; /* Flags needing atomic access. */

	pid_t pid;
	pid_t tgid;

	/*
	 * children/sibling forms the list of my natural children
	 */
	struct list_head children;	/* list of my children */
	struct list_head sibling;	/* linkage in my parent's children list */
	struct task_struct *group_leader;	/* threadgroup leader */
};

#endif
