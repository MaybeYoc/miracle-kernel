/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX__INIT_TASK_H
#define _LINUX__INIT_TASK_H

#include <asm/thread_info.h>

extern struct files_struct init_files;
extern struct fs_struct init_fs;
extern struct nsproxy init_nsproxy;
extern struct group_info init_groups;
extern struct cred init_cred;

#ifndef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
#define INIT_PREV_CPUTIME(x)	.prev_cputime = {			\
	.lock = __RAW_SPIN_LOCK_UNLOCKED(x.prev_cputime.lock),		\
},
#else
#define INIT_PREV_CPUTIME(x)
#endif

#ifdef CONFIG_POSIX_TIMERS
#define INIT_CPU_TIMERS(s)						\
	.cpu_timers = {							\
		LIST_HEAD_INIT(s.cpu_timers[0]),			\
		LIST_HEAD_INIT(s.cpu_timers[1]),			\
		LIST_HEAD_INIT(s.cpu_timers[2]),			\
	},
#else
#define INIT_CPU_TIMERS(s)
#endif

#define INIT_TASK_COMM "swapper"

/* Attach to the init_task data structure for proper alignment */
#define __init_task_data __attribute__((__section__(".data..init_task")))

/* Attach to the thread_info data structure for proper alignment */
#define __init_thread_info __attribute__((__section__(".data..init_thread_info")))

#endif