
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 *
 * Please note that ndelay(), udelay() and mdelay() may return early for
 * several reasons:
 *  1. computed loops_per_jiffy too low (due to the time taken to
 *     execute the timer interrupt.)
 *  2. cache behaviour affecting the time it takes to execute the
 *     loop function.
 *  3. CPU clock rate changes.
 *
 * Please see this thread:
 *   http://lists.openwall.net/linux-kernel/2011/01/09/56
 */

#include <linux/kernel.h>

/* TODO */
#ifndef mdelay
#define mdelay(n) do \
{				\
	for (int i = 0; i < 100000; i++)	\
		nop();				\
} while (0)
#endif

#endif /* defined(_LINUX_DELAY_H) */
