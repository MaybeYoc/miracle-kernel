/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _TIMEKEEPING_INTERNAL_H
#define _TIMEKEEPING_INTERNAL_H
/*
 * timekeeping debug functions
 */
#include <linux/clocksource.h>
#include <linux/time.h>

#define tk_debug_account_sleep_time(x)

static inline u64 clocksource_delta(u64 now, u64 last, u64 mask)
{
	return (now - last) & mask;
}

#endif /* _TIMEKEEPING_INTERNAL_H */
