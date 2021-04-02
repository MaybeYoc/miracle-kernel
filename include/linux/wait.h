/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

#include <linux/spinlock.h>
#include <linux/list.h>

struct wait_queue_head {
	spinlock_t		lock;
	struct list_head	head;
};
typedef struct wait_queue_head wait_queue_head_t;

#define wait_event(wq_head, condition)

#define wake_up(x)

#define init_waitqueue_head(wq_head)

#define INIT_WORK(_work, _func)
#endif /* _LINUX_WAIT_H */