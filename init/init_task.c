#include <linux/init_task.h>
#include <asm/thread_info.h>
#include <asm/memory.h>

struct task_struct {
	struct thread_info		thread_info;
	/* -1 unrunnable, 0 runnable, >0 stopped: */
	volatile long			state;
	void				*stack;
	/* Per task flags (PF_*), defined further below: */
	unsigned int			flags;
	unsigned int			ptrace;
};

extern unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];

struct task_struct init_task
	__init_task_data
= {
	.thread_info	= INIT_THREAD_INFO(init_task),
	.state		= 0,
	.stack		= init_stack,
};

struct thread_info init_thread_info __init_thread_info = INIT_THREAD_INFO(init_task);
