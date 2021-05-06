// SPDX-License-Identifier: GPL-2.0
/*
 *  Kernel timekeeping code and accessor functions. Based on code from
 *  timer.c, moved in commit 8524070b7982.
 */
#include <linux/bug.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/clocksource.h>
#include <linux/timekeeping.h>
#include <linux/sched.h>

#include "timekeeping_internal.h"

/*
 * The most important data for readout fits into a single 64 byte
 * cache line.
 */
static struct {
	seqcount_t		seq;
	struct timekeeper	timekeeper;
} tk_core ____cacheline_aligned = {
	.seq = SEQCNT_ZERO(tk_core.seq),
};

static DEFINE_RAW_SPINLOCK(timekeeper_lock);

/*
 * tk_clock_read - atomic clocksource read() helper
 *
 * This helper is necessary to use in the read paths because, while the
 * seqlock ensures we don't return a bad value while structures are updated,
 * it doesn't protect from potential crashes. There is the possibility that
 * the tkr's clocksource may change between the read reference, and the
 * clock reference passed to the read function.  This can cause crashes if
 * the wrong clocksource is passed to the wrong read function.
 * This isn't necessary to use when holding the timekeeper_lock or doing
 * a read of the fast-timekeeper tkrs (which is protected by its own locking
 * and update logic).
 */
static inline u64 tk_clock_read(const struct tk_read_base *tkr)
{
	struct clocksource *clock = READ_ONCE(tkr->clock);

	return clock->read(clock);
}

static inline u64 timekeeping_get_delta(const struct tk_read_base *tkr)
{
	u64 cycle_now, delta;

	/* read clocksource */
	cycle_now = tk_clock_read(tkr);

	/* calculate the delta since the last update_wall_time */
	delta = clocksource_delta(cycle_now, tkr->cycle_last, tkr->mask);

	return delta;
}

static inline u64 timekeeping_delta_to_ns(const struct tk_read_base *tkr, u64 delta)
{
	u64 nsec;

	nsec = delta * tkr->mult + tkr->ktime_nsec;
	nsec >>= tkr->shift;

	return nsec;
}

static inline u64 timekeeping_get_ns(const struct tk_read_base *tkr)
{
	u64 delta;

	delta = timekeeping_get_delta(tkr);
	return timekeeping_delta_to_ns(tkr, delta);
}

u32 ktime_get_resolution_ns(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u32 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		nsecs = tk->tkr_mono.mult >> tk->tkr_mono.shift;
	} while (read_seqcount_retry(&tk_core.seq, seq));

	return nsecs;
}

/**
 * timekeeping_max_deferment - Returns max time the clocksource can be deferred
 */
u64 timekeeping_max_deferment(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long seq;
	u64 ret;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ret = tk->tkr_mono.clock->max_idle_ns;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ret;
}

void ktime_get_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u64 nsec;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		ts->tv_sec = tk->ktime_sec;
		nsec = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	ts->tv_nsec = 0;
	timespec64_add_ns(ts, nsec);
}

ktime_t ktime_get(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = tk->ktime_sec;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);
}

time64_t ktime_get_seconds(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	return tk->ktime_sec;
}

/**
 * ktime_get_real_ts64 - Returns the time of day in a timespec64.
 * @ts:		pointer to the timespec to be set
 *
 * Returns the time of day in a timespec64 (WARN if suspended).
 */
void ktime_get_real_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long seq;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ts->tv_sec = tk->ktime_sec + tk->offs_real;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	ts->tv_nsec = 0;
	timespec64_add_ns(ts, nsecs);
}

ktime_t ktime_get_real(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = tk->ktime_sec + tk->offs_real;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);
}

time64_t ktime_get_real_seconds(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	return tk->ktime_sec + tk->offs_real;
}

/**
 * tk_setup_internals - Set up internals to use clocksource clock.
 *
 * @tk:		The target timekeeper to setup.
 * @clock:		Pointer to clocksource.
 *
 * Calculates a fixed cycle/nsec interval for a given clocksource/adjustment
 * pair and interval request.
 *
 * Unless you're the timekeeping code, you should not be using this!
 */
static void tk_setup_internals(struct timekeeper *tk, struct clocksource *clock)
{
	tk->tkr_mono.clock = clock;
	tk->tkr_mono.mask = clock->mask;
	tk->tkr_mono.mult = clock->mult;
	tk->tkr_mono.shift = clock->shift;
	tk->tkr_mono.cycle_last = tk_clock_read(&tk->tkr_mono);
	tk->tkr_mono.ktime_nsec = 0;

	tk->offs_real = 0;
	tk->ktime_sec = 0;
}

/**
 * read_persistent_clock64 -  Return time from the persistent clock.
 *
 * Weak dummy function for arches that do not yet support it.
 * Reads the time from the battery backed persistent clock.
 * Returns a timespec with tv_sec=0 and tv_nsec=0 if unsupported.
 *
 *  XXX - Do be sure to remove it once all arches implement it.
 */
void __weak read_persistent_clock64(struct timespec64 *ts)
{
	ts->tv_sec = 0;
	ts->tv_nsec = 0;
}

static void tk_set_xtime(struct timekeeper *tk, const struct timespec64 *ts)
{
	tk->offs_real =	timespec64_to_ns(ts) - ktime_get();
}

/*
 * timekeeping_init - Initializes the clocksource and common timekeeping values
 */
void __init timekeeping_init(void)
{
	struct timespec64 wall_time;
	struct timekeeper *tk = &tk_core.timekeeper;
	struct clocksource *clock;
	unsigned long flags;

	read_persistent_clock64(&wall_time);
	
	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	clock = clocksource_curr_clock();
	if (clock->enable)
		clock->enable(clock);
	tk_setup_internals(tk, clock);

	write_seqcount_end(&tk_core.seq);

	tk_set_xtime(tk, &wall_time);

	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);
}

static inline void tk_normalize_xtime(struct timekeeper *tk)
{
	while (tk->tkr_mono.ktime_nsec >= ((u64)NSEC_PER_SEC << tk->tkr_mono.shift)) {
		tk->tkr_mono.ktime_nsec -= (u64)NSEC_PER_SEC << tk->tkr_mono.shift;
		tk->ktime_sec++;
	}
}

/**
 * timekeeping_forward_now - update clock to the current time
 *
 * Forward the current clock to update its state since the last call to
 * update_wall_time(). This is useful before significant clock changes,
 * as it avoids having to deal with this time offset explicitly.
 */
static void timekeeping_forward_now(struct timekeeper *tk)
{
	u64 cycle_now, delta;

	cycle_now = tk_clock_read(&tk->tkr_mono);
	delta = clocksource_delta(cycle_now, tk->tkr_mono.cycle_last, tk->tkr_mono.mask);
	tk->tkr_mono.cycle_last = cycle_now;

	tk->tkr_mono.ktime_nsec += delta * tk->tkr_mono.mult;

	tk_normalize_xtime(tk);
}

/**
 * do_settimeofday64 - Sets the time of day.
 * @ts:     pointer to the timespec64 variable containing the new time
 *
 * Sets the time of day to the new time and update NTP and notify hrtimers
 */
int do_settimeofday64(const struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long flags;
	

	if (!timespec64_valid_strict(ts))
		return -EINVAL;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	timekeeping_forward_now(tk);

	write_seqcount_end(&tk_core.seq);

	tk_set_xtime(tk, ts);

	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	return 0;
}

/**
 * timekeeping_valid_for_hres - Check if timekeeping is suitable for hres
 */
int timekeeping_valid_for_hres(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long seq;
	int ret;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ret = tk->tkr_mono.clock->flags & CLOCK_SOURCE_VALID_FOR_HRES;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ret;
}

/**
 * update_wall_time - Uses the current clocksource to increment the wall time
 *
 */
void update_wall_time(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	timekeeping_forward_now(tk);
}


static inline struct timespec64 tk_xtime(const struct timekeeper *tk)
{
	struct timespec64 ts;

	ts.tv_sec = tk->ktime_sec;
	ts.tv_nsec = (long)(tk->tkr_mono.ktime_nsec >> tk->tkr_mono.shift);
	return ts;
}

ktime_t ktime_get_coarse(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	struct timespec64 ts;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		ts = tk_xtime(tk);
		nsecs =  timespec64_to_ns(&ts);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return nsecs;
}

void ktime_get_coarse_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long seq;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		*ts = tk_xtime(tk);
	} while (read_seqcount_retry(&tk_core.seq, seq));
}

ktime_t ktime_get_coarse_real(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	struct timespec64 ts;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		ts = tk_xtime(tk);
		nsecs =  timespec64_to_ns(&ts);
		nsecs += tk->offs_real;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return nsecs;
}

void ktime_get_coarse_real_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct timespec64 now, mono;
	unsigned long seq;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		now = tk_xtime(tk);
		mono = ns_to_timespec64(tk->offs_real);
	} while (read_seqcount_retry(&tk_core.seq, seq));

	set_normalized_timespec64(ts, now.tv_sec + mono.tv_sec,
				now.tv_nsec + mono.tv_nsec);
}

/*
 * Must hold jiffies_lock
 */
void do_timer(unsigned long ticks)
{
	jiffies_64 += ticks;
	calc_global_load(ticks);
}

/**
 * ktime_get_update_offsets_now - hrtimer helper
 * @cwsseq:	pointer to check and store the clock was set sequence number
 * @offs_real:	pointer to storage for monotonic -> realtime offset
 * @offs_boot:	pointer to storage for monotonic -> boottime offset
 * @offs_tai:	pointer to storage for monotonic -> clock tai offset
 *
 * Returns current monotonic time and updates the offsets if the
 * sequence number in @cwsseq and timekeeper.clock_was_set_seq are
 * different.
 *
 * Called from hrtimer_interrupt() or retrigger_next_event()
 */
ktime_t ktime_get_update_offsets_now(unsigned int *cwsseq, ktime_t *offs_real,
				     ktime_t *offs_boot, ktime_t *offs_tai)
{
#if 0
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		base = tk->tkr_mono.base;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);
		base = ktime_add_ns(base, nsecs);

		if (*cwsseq != tk->clock_was_set_seq) {
			*cwsseq = tk->clock_was_set_seq;
			*offs_real = tk->offs_real;
			*offs_boot = tk->offs_boot;
			*offs_tai = tk->offs_tai;
		}

		/* Handle leapsecond insertion adjustments */
		if (unlikely(base >= tk->next_leap_ktime))
			*offs_real = ktime_sub(tk->offs_real, ktime_set(1, 0));

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return base;
#endif
	return 0;
}
