/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX__INIT_TASK_H
#define _LINUX__INIT_TASK_H

/* TODO */

#define __init_task_data __attribute__((__section__(".data..init_task")))

/* Attach to the thread_info data structure for proper alignment */
#define __init_thread_info __attribute__((__section__(".data..init_thread_info")))

#endif
