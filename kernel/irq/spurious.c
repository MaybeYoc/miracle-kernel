// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 1992, 1998-2004 Linus Torvalds, Ingo Molnar
 *
 * This file contains spurious interrupt handling.
 */

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include "internal.h"

bool noirqdebug __read_mostly;

void note_interrupt(struct irq_desc *desc, irqreturn_t action_ret)
{
	/* TODO */
	WARN_ON(1);
}

bool irq_wait_for_poll(struct irq_desc *desc)
{
	WARN_ON(1);
	
	return false;
}
