/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_HARDIRQ_H
#define LINUX_HARDIRQ_H

#include <asm/hardirq.h>

/*
 * Enter irq context (on NO_HZ, update jiffies):
 */
extern void irq_enter(void);

/*
 * Exit irq context and process softirqs if needed:
 */
extern void irq_exit(void);

extern void synchronize_irq(unsigned int irq);

#endif /* LINUX_HARDIRQ_H */
