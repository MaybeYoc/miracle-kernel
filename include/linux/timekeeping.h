/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#include <linux/errno.h>

/* Included from linux/ktime.h */

void timekeeping_init(void);

/*
 * Get and set timeofday
 */
extern int do_settimeofday64(const struct timespec64 *ts);

extern void ktime_get_ts64(struct timespec64 *ts);
extern void ktime_get_real_ts64(struct timespec64 *tv);
extern void ktime_get_coarse_ts64(struct timespec64 *ts);
extern void ktime_get_coarse_real_ts64(struct timespec64 *ts);

/*
 * time64_t base interfaces
 */
extern time64_t ktime_get_seconds(void);
extern time64_t ktime_get_real_seconds(void);

extern ktime_t ktime_get(void);
extern u32 ktime_get_resolution_ns(void);

extern ktime_t ktime_get_real(void);
extern ktime_t ktime_get_coarse_real(void);

extern u64 ktime_get_cycles(void);

static inline u64 ktime_get_ns(void)
{
	return ktime_to_ns(ktime_get());
}

static inline u64 ktime_get_real_ns(void)
{
	return ktime_to_ns(ktime_get_real());
}

/*
 * struct system_counterval_t - system counter value with the pointer to the
 *	corresponding clocksource
 * @cycles:	System counter value
 * @cs:		Clocksource corresponding to system counter value. Used by
 *	timekeeping code to verify comparibility of two cycle values
 */
struct system_counterval_t {
	u64			cycles;
	struct clocksource	*cs;
};

/*
 * Persistent clock related interfaces
 */
extern int persistent_clock_is_local;

extern void read_persistent_clock64(struct timespec64 *ts);
void read_persistent_wall_and_boot_offset(struct timespec64 *wall_clock,
					  struct timespec64 *boot_offset);
extern int update_persistent_clock64(struct timespec64 now);

#endif
