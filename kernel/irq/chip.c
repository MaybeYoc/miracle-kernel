// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code, for irq-chip based
 * architectures. Detailed information is available in
 * Documentation/core-api/genericirq.rst
 */
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/irqdomain.h>
#include <linux/irqnr.h>

#include "internal.h"

void irq_modify_status(unsigned int irq, unsigned long clr, unsigned long set)
{

}

void
irq_set_chained_handler_and_data(unsigned int irq, irq_flow_handler_t handle,
				 void *data)
{

}

struct irq_data *irq_get_irq_data(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return desc ? &desc->irq_data : NULL;
}
