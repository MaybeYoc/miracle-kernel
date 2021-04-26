#include <linux/init_task.h>
#include <linux/sched.h>

#include <asm/memory.h>

extern unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];

struct task_struct init_task
	__init_task_data
= {
	.thread_info	= INIT_THREAD_INFO(init_task),
	.state		= 0,
	.stack		= init_stack,
	.active_mm	= &init_mm,
	.comm		= INIT_TASK_COMM,
};

struct thread_info init_thread_info __init_thread_info = INIT_THREAD_INFO(init_task);
