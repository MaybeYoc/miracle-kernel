// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code. Detailed
 * information is available in Documentation/core-api/genericirq.rst
 *
 */
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/errno.h>

#include "internal.h"

void (*handle_arch_irq)(struct pt_regs *) __ro_after_init;

int __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	if (handle_arch_irq)
		return -EBUSY;

	handle_arch_irq = handle_irq;
	return 0;
}

/**
 * handle_bad_irq - handle spurious and unhandled irqs
 * @desc:      description of the interrupt
 *
 * Handles spurious and unhandled IRQ's. It also prints a debugmessage.
 */
void handle_bad_irq(struct irq_desc *desc)
{
	unsigned int irq = irq_desc_get_irq(desc);

	print_irq_desc(irq, desc);
	kstat_incr_irqs_this_cpu(desc);
	ack_bad_irq(irq);
}
