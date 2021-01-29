/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H

/*
 *	Generic SMP support
 *		Alan Cox. <alan@redhat.com>
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/init.h>
#include <asm/smp.h>

#ifdef CONFIG_SMP
void kick_all_cpus_sync(void);
#else
static inline void kick_all_cpus_sync(void) {  }
#endif
#endif /* __LINUX_SMP_H */
