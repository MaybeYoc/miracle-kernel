// SPDX-License-Identifier: GPL-2.0
/*
 * NTP state machine interfaces and logic.
 *
 * This code was mainly moved from kernel/timer.c and kernel/time.c
 * Please see those files for relevant copyright info and historical
 * changelogs.
 */
#include <linux/clocksource.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/timex.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/module.h>

#include "ntp_internal.h"
#include "timekeeping_internal.h"


/*
 * NTP timekeeping variables:
 *
 * Note: All of the NTP state is protected by the timekeeping locks.
 */


/* USER_HZ period (usecs): */
unsigned long			tick_usec = USER_TICK_USEC;

/* SHIFTED_HZ period (nsecs): */
unsigned long			tick_nsec;

static u64			tick_length;
static u64			tick_length_base;

#define SECS_PER_DAY		86400
#define MAX_TICKADJ		500LL		/* usecs */
#define MAX_TICKADJ_SCALED \
	(((MAX_TICKADJ * NSEC_PER_USEC) << NTP_SCALE_SHIFT) / NTP_INTERVAL_FREQ)

/*
 * phase-lock loop variables
 */

/*
 * clock synchronization status
 *
 * (TIME_ERROR prevents overwriting the CMOS clock)
 */
static int			time_state = TIME_OK;

/* clock status bits:							*/
static int			time_status = STA_UNSYNC;

/* time adjustment (nsecs):						*/
static s64			time_offset;

/* pll time constant:							*/
//static long			time_constant = 2;

/* maximum error (usecs):						*/
static long			time_maxerror = NTP_PHASE_LIMIT;

/* estimated error (usecs):						*/
static long			time_esterror = NTP_PHASE_LIMIT;

/* frequency offset (scaled nsecs/secs):				*/
static s64			time_freq;

/* time at last adjustment (secs):					*/
//static time64_t		time_reftime;

static long			time_adjust;

/* constant (boot-param configurable) NTP tick adjustment (upscaled)	*/
static s64			ntp_tick_adj;

/* second value of the next pending leapsecond, or TIME64_MAX if no leap */
static time64_t			ntp_next_leap_sec = TIME64_MAX;

/**
 * ntp_get_next_leap - Returns the next leapsecond in CLOCK_REALTIME ktime_t
 *
 * Provides the time of the next leapsecond against CLOCK_REALTIME in
 * a ktime_t format. Returns KTIME_MAX if no leapsecond is pending.
 */
ktime_t ntp_get_next_leap(void)
{
	ktime_t ret;

	if ((time_state == TIME_INS) && (time_status & STA_INS))
		return ktime_set(ntp_next_leap_sec, 0);
	ret = KTIME_MAX;
	return ret;
}

/*
 * Update (tick_length, tick_length_base, tick_nsec), based
 * on (tick_usec, ntp_tick_adj, time_freq):
 */
static void ntp_update_frequency(void)
{
	u64 second_length;
	u64 new_base;

	second_length		 = (u64)(tick_usec * NSEC_PER_USEC * USER_HZ)
						<< NTP_SCALE_SHIFT;

	second_length		+= ntp_tick_adj;
	second_length		+= time_freq;

	tick_nsec		 = div_u64(second_length, HZ) >> NTP_SCALE_SHIFT;
	new_base		 = div_u64(second_length, NTP_INTERVAL_FREQ);

	/*
	 * Don't wait for the next second_overflow, apply
	 * the change to the tick length immediately:
	 */
	tick_length		+= new_base - tick_length_base;
	tick_length_base	 = new_base;
}

static inline void pps_clear(void) {}

/**
 * ntp_clear - Clears the NTP state variables
 */
void ntp_clear(void)
{
	time_adjust	= 0;		/* stop active adjtime() */
	time_status	|= STA_UNSYNC;
	time_maxerror	= NTP_PHASE_LIMIT;
	time_esterror	= NTP_PHASE_LIMIT;

	ntp_update_frequency();

	tick_length	= tick_length_base;
	time_offset	= 0;

	ntp_next_leap_sec = TIME64_MAX;
	/* Clear PPS state variables */
	pps_clear();
}

void ntp_notify_cmos_timer(void)
{
	//if (!ntp_synced())
	//	return;

	//if (IS_ENABLED(CONFIG_GENERIC_CMOS_UPDATE) ||
	 //   IS_ENABLED(CONFIG_RTC_SYSTOHC))
	//	queue_delayed_work(system_power_efficient_wq, &sync_work, 0);
}

/*
 * adjtimex mainly allows reading (and writing, if superuser) of
 * kernel time-keeping variables. used by xntpd.
 */
int __do_adjtimex(struct timex *txc, const struct timespec64 *ts, s32 *time_tai)
{
	return 0;
}

u64 ntp_tick_length(void)
{
	return tick_length;
}
