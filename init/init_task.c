// SPDX-License-Identifier: GPL-2.0
#include <linux/init_task.h>
#include <linux/init.h>
#include <linux/sched.h>

/*
 * Set up the first task table, touch at your own risk!. Base=0,
 * limit=0x1fffff (=2MB)
 */
struct task_struct init_task __init_task_data = {

	.thread_info	= INIT_THREAD_INFO(init_task),
	.state		= 0,
	.stack		= init_stack,
};

struct thread_info init_thread_info __init_thread_info = INIT_THREAD_INFO(init_task);
