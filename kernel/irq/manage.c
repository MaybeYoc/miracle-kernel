// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006 Thomas Gleixner
 *
 * This file contains driver APIs to the irq subsystem.
 */

#define pr_fmt(fmt) "genirq: " fmt

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

#include "internal.h"

#ifdef CONFIG_SMP
cpumask_var_t irq_default_affinity;
#endif /* CONFIG_SMP */