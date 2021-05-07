// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright(C) 2005-2006, Thomas Gleixner <tglx@linutronix.de>
 *  Copyright(C) 2005-2007, Red Hat, Inc., Ingo Molnar
 *  Copyright(C) 2006-2007  Timesys Corp., Thomas Gleixner
 *
 *  High-resolution kernel timers
 *
 *  In contrast to the low-resolution timeout API, aka timer wheel,
 *  hrtimers provide finer resolution and accuracy depending on system
 *  configuration and capabilities.
 *
 *  Started by: Thomas Gleixner and Ingo Molnar
 *
 *  Credits:
 *	Based on the original timer wheel code
 *
 *	Help, testing, suggestions, bugfixes, improvements were
 *	provided by:
 *
 *	George Anzinger, Andrew Morton, Steven Rostedt, Roman Zippel
 *	et. al.
 */
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/timekeeping.h>
#include <linux/sched/task.h>
#include <linux/uaccess.h>

#include "tick-internal.h"

/*
 * Masks for selecting the soft and hard context timers from
 * cpu_base->active
 */
#define MASK_SHIFT		(HRTIMER_MAX_CLOCK_BASES)
#define HRTIMER_ACTIVE_HARD	((1U << MASK_SHIFT) - 1)
#define HRTIMER_ACTIVE_ALL	(HRTIMER_ACTIVE_HARD)

/*
 * The timer bases:
 *
 * There are more clockids than hrtimer bases. Thus, we index
 * into the timer bases by the hrtimer_base_type enum. When trying
 * to reach a base using a clockid, hrtimer_clockid_to_base()
 * is used to convert from clockid to the proper hrtimer_base_type.
 */
DEFINE_PER_CPU(struct hrtimer_cpu_base, hrtimer_bases) =
{
	.lock = __RAW_SPIN_LOCK_UNLOCKED(hrtimer_bases.lock),
	.clock_base =
	{
		{
			.index = HRTIMER_BASE_MONOTONIC,
			.clockid = CLOCK_MONOTONIC,
			.get_time = &ktime_get,
		},
		{
			.index = HRTIMER_BASE_REALTIME,
			.clockid = CLOCK_REALTIME,
			.get_time = &ktime_get_real,
		},
	}
};

static const int hrtimer_clock_to_base_table[MAX_CLOCKS] = {
	/* Make sure we catch unsupported clockids */
	[0 ... MAX_CLOCKS - 1]	= HRTIMER_MAX_CLOCK_BASES,

	[CLOCK_REALTIME]	= HRTIMER_BASE_REALTIME,
	[CLOCK_MONOTONIC]	= HRTIMER_BASE_MONOTONIC,
};

static inline struct hrtimer_clock_base *
lock_hrtimer_base(const struct hrtimer *timer, unsigned long *flags)
{
	struct hrtimer_clock_base *base = timer->base;

	raw_spin_lock_irqsave(&base->cpu_base->lock, *flags);

	return base;
}

/*
 * Counterpart to lock_hrtimer_base above:
 */
static inline
void unlock_hrtimer_base(const struct hrtimer *timer, unsigned long *flags)
{
	raw_spin_unlock_irqrestore(&timer->base->cpu_base->lock, *flags);
}

/*
 * Functions for the union type storage format of ktime_t which are
 * too large for inlining:
 */
#if BITS_PER_LONG < 64
/*
 * Divide a ktime value by a nanosecond value
 */
s64 __ktime_divns(const ktime_t kt, s64 div)
{
	int sft = 0;
	s64 dclc;
	u64 tmp;

	dclc = ktime_to_ns(kt);
	tmp = dclc < 0 ? -dclc : dclc;

	/* Make sure the divisor is less than 2^32: */
	while (div >> 32) {
		sft++;
		div >>= 1;
	}
	tmp >>= sft;
	do_div(tmp, (unsigned long) div);
	return dclc < 0 ? -tmp : tmp;
}
#endif /* BITS_PER_LONG >= 64 */

/*
 * Add two ktime values and do a safety check for overflow:
 */
ktime_t ktime_add_safe(const ktime_t lhs, const ktime_t rhs)
{
	ktime_t res = ktime_add_unsafe(lhs, rhs);

	/*
	 * We use KTIME_SEC_MAX here, the maximum timeout which we can
	 * return to user space in a timespec:
	 */
	if (res < 0 || res < lhs || res < rhs)
		res = ktime_set(KTIME_SEC_MAX, 0);

	return res;
}

static struct hrtimer_clock_base *
__next_base(struct hrtimer_cpu_base *cpu_base, unsigned int *active)
{
	unsigned int idx;

	if (!*active)
		return NULL;

	idx = __ffs(*active);
	*active &= ~(1U << idx);

	return &cpu_base->clock_base[idx];
}

#define for_each_active_base(base, cpu_base, active)	\
	while ((base = __next_base((cpu_base), &(active))))

static ktime_t __hrtimer_next_event_base(struct hrtimer_cpu_base *cpu_base,
					 const struct hrtimer *exclude,
					 unsigned int active,
					 ktime_t expires_next)
{
	struct hrtimer_clock_base *base;
	ktime_t expires;

	for_each_active_base(base, cpu_base, active) {
		struct timerqueue_node *next;
		struct hrtimer *timer;

		next = timerqueue_getnext(&base->active);
		timer = container_of(next, struct hrtimer, node);
		if (timer == exclude) {
			/* Get to the next timer in the queue. */
			next = timerqueue_iterate_next(next);
			if (!next)
				continue;

			timer = container_of(next, struct hrtimer, node);
		}
		expires = ktime_sub(hrtimer_get_expires(timer), base->offset);
		if (expires < expires_next) {
			expires_next = expires;

			/* Skip cpu_base update if a timer is being excluded. */
			if (exclude)
				continue;

			cpu_base->next_timer = timer;
		}
	}
	/*
	 * clock_was_set() might have changed base->offset of any of
	 * the clock bases so the result might be negative. Fix it up
	 * to prevent a false positive in clockevents_program_event().
	 */
	if (expires_next < 0)
		expires_next = 0;
	return expires_next;
}

/*
 * Recomputes cpu_base::*next_timer and returns the earliest expires_next but
 * does not set cpu_base::*expires_next, that is done by hrtimer_reprogram.
 *
 * When a softirq is pending, we can ignore the HRTIMER_ACTIVE_SOFT bases,
 * those timers will get run whenever the softirq gets handled, at the end of
 * hrtimer_run_softirq(), hrtimer_update_softirq_timer() will re-add these bases.
 *
 * Therefore softirq values are those from the HRTIMER_ACTIVE_SOFT clock bases.
 * The !softirq values are the minima across HRTIMER_ACTIVE_ALL, unless an actual
 * softirq is pending, in which case they're the minima of HRTIMER_ACTIVE_HARD.
 *
 * @active_mask must be one of:
 *  - HRTIMER_ACTIVE_ALL,
 *  - HRTIMER_ACTIVE_SOFT, or
 *  - HRTIMER_ACTIVE_HARD.
 */
static ktime_t
__hrtimer_get_next_event(struct hrtimer_cpu_base *cpu_base, unsigned int active_mask)
{
	unsigned int active;
	struct hrtimer *next_timer = NULL;
	ktime_t expires_next = KTIME_MAX;

	if (likely(active_mask & HRTIMER_ACTIVE_HARD)) {
		active = cpu_base->active_bases & HRTIMER_ACTIVE_HARD;
		cpu_base->next_timer = next_timer;
		expires_next = __hrtimer_next_event_base(cpu_base, NULL, active,
							 expires_next);
	} else
		BUG_ON(1);

	return expires_next;
}

static inline ktime_t hrtimer_update_base(struct hrtimer_cpu_base *base)
{
	ktime_t *offs_real = &base->clock_base[HRTIMER_BASE_REALTIME].offset;

	ktime_t now = ktime_get_update_offsets_now(&base->clock_was_set_seq, offs_real);

	return now;
}

/*
 * Reprogram the event source with checking both queues for the
 * next event
 * Called with interrupts disabled and base->lock held
 */
static void
hrtimer_force_reprogram(struct hrtimer_cpu_base *cpu_base, int skip_equal)
{
	ktime_t expires_next;

	/*
	 * Find the current next expiration time.
	 */
	expires_next = __hrtimer_get_next_event(cpu_base, HRTIMER_ACTIVE_ALL);

	if (skip_equal && expires_next == cpu_base->expires_next)
		return;

	cpu_base->expires_next = expires_next;

	/*
	 * If hres is not active, hardware does not have to be
	 * reprogrammed yet.
	 *
	 * If a hang was detected in the last timer interrupt then we
	 * leave the hang delay active in the hardware. We want the
	 * system to make progress. That also prevents the following
	 * scenario:
	 * T1 expires 50ms from now
	 * T2 expires 5s from now
	 *
	 * T1 is removed, so this code is called and would reprogram
	 * the hardware to 5s from now. Any hrtimer_start after that
	 * will not reprogram the hardware due to hang_detected being
	 * set. So we'd effectivly block all timers until the T2 event
	 * fires.
	 */
	if (cpu_base->hang_detected)
		return;

	tick_program_event(cpu_base->expires_next, 1);
}

unsigned int hrtimer_resolution __read_mostly = HIGH_RES_NSEC;

/*
 * Retrigger next event is called after clock was set
 *
 * Called with interrupts disabled via on_each_cpu()
 */
static void retrigger_next_event(void *arg)
{
	struct hrtimer_cpu_base *base = this_cpu_ptr(&hrtimer_bases);

	raw_spin_lock(&base->lock);
	hrtimer_update_base(base);
	hrtimer_force_reprogram(base, 0);
	raw_spin_unlock(&base->lock);
}

/*
 * When a timer is enqueued and expires earlier than the already enqueued
 * timers, we have to check, whether it expires earlier than the timer for
 * which the clock event device was armed.
 *
 * Called with interrupts disabled and base->cpu_base.lock held
 */
static void hrtimer_reprogram(struct hrtimer *timer, bool reprogram)
{
	struct hrtimer_cpu_base *cpu_base = this_cpu_ptr(&hrtimer_bases);
	struct hrtimer_clock_base *base = timer->base;
	ktime_t expires = ktime_sub(hrtimer_get_expires(timer), base->offset);

	WARN_ON_ONCE(hrtimer_get_expires_tv64(timer) < 0);

	/*
	 * CLOCK_REALTIME timer might be requested with an absolute
	 * expiry time which is less than base->offset. Set it to 0.
	 */
	if (expires < 0)
		expires = 0;

	/*
	 * If the timer is not on the current cpu, we cannot reprogram
	 * the other cpus clock event device.
	 */
	if (base->cpu_base != cpu_base)
		return;

	/*
	 * If the hrtimer interrupt is running, then it will
	 * reevaluate the clock bases and reprogram the clock event
	 * device. The callbacks are always executed in hard interrupt
	 * context so we don't need an extra check for a running
	 * callback.
	 */
	if (cpu_base->in_hrtirq)
		return;

	if (expires >= cpu_base->expires_next)
		return;

	/* Update the pointer to the next expiring timer */
	cpu_base->next_timer = timer;
	cpu_base->expires_next = expires;

	/*
	 * If hres is not active, hardware does not have to be
	 * programmed yet.
	 *
	 * If a hang was detected in the last timer interrupt then we
	 * do not schedule a timer which is earlier than the expiry
	 * which we enforced in the hang detection. We want the system
	 * to make progress.
	 */
	if (cpu_base->hang_detected)
		return;

	/*
	 * Program the timer hardware. We enforce the expiry for
	 * events which are already in the past.
	 */
	tick_program_event(expires, 1);
}

/*
 * During resume we might have to reprogram the high resolution timer
 * interrupt on all online CPUs.  However, all other CPUs will be
 * stopped with IRQs interrupts disabled so the clock_was_set() call
 * must be deferred.
 */
void hrtimers_resume(void)
{
	lockdep_assert_irqs_disabled();
	/* Retrigger on the local CPU */
	retrigger_next_event(NULL);
}

/**
 * hrtimer_forward - forward the timer expiry
 * @timer:	hrtimer to forward
 * @now:	forward past this time
 * @interval:	the interval to forward
 *
 * Forward the timer expiry so it will expire in the future.
 * Returns the number of overruns.
 *
 * Can be safely called from the callback function of @timer. If
 * called from other contexts @timer must neither be enqueued nor
 * running the callback and the caller needs to take care of
 * serialization.
 *
 * Note: This only updates the timer expiry value and does not requeue
 * the timer.
 */
u64 hrtimer_forward(struct hrtimer *timer, ktime_t now, ktime_t interval)
{
	u64 orun = 1;
	ktime_t delta;

	delta = ktime_sub(now, hrtimer_get_expires(timer));

	if (delta < 0)
		return 0;

	if (WARN_ON(timer->state & HRTIMER_STATE_ENQUEUED))
		return 0;

	if (interval < hrtimer_resolution)
		interval = hrtimer_resolution;

	if (unlikely(delta >= interval)) {
		s64 incr = ktime_to_ns(interval);

		orun = ktime_divns(delta, incr);
		hrtimer_add_expires_ns(timer, incr * orun);
		if (hrtimer_get_expires_tv64(timer) > now)
			return orun;
		/*
		 * This (and the ktime_add() below) is the
		 * correction for exact:
		 */
		orun++;
	}
	hrtimer_add_expires(timer, interval);

	return orun;
}

/*
 * enqueue_hrtimer - internal function to (re)start a timer
 *
 * The timer is inserted in expiry order. Insertion into the
 * red black tree is O(log(n)). Must hold the base lock.
 *
 * Returns 1 when the new timer is the leftmost timer in the tree.
 */
static int enqueue_hrtimer(struct hrtimer *timer,
			   struct hrtimer_clock_base *base,
			   enum hrtimer_mode mode)
{
	base->cpu_base->active_bases |= 1 << base->index;

	timer->state = HRTIMER_STATE_ENQUEUED;

	return timerqueue_add(&base->active, &timer->node);
}

/*
 * __remove_hrtimer - internal function to remove a timer
 *
 * Caller must hold the base lock.
 *
 * High resolution timer mode reprograms the clock event device when the
 * timer is the one which expires next. The caller can disable this by setting
 * reprogram to zero. This is useful, when the context does a reprogramming
 * anyway (e.g. timer interrupt)
 */
static void __remove_hrtimer(struct hrtimer *timer,
			     struct hrtimer_clock_base *base,
			     u8 newstate, int reprogram)
{
	struct hrtimer_cpu_base *cpu_base = base->cpu_base;
	u8 state = timer->state;

	timer->state = newstate;
	if (!(state & HRTIMER_STATE_ENQUEUED))
		return;

	if (!timerqueue_del(&base->active, &timer->node))
		cpu_base->active_bases &= ~(1 << base->index);

	/*
	 * Note: If reprogram is false we do not update
	 * cpu_base->next_timer. This happens when we remove the first
	 * timer on a remote cpu. No harm as we never dereference
	 * cpu_base->next_timer. So the worst thing what can happen is
	 * an superflous call to hrtimer_force_reprogram() on the
	 * remote cpu later on if the same timer gets enqueued again.
	 */
	if (reprogram && timer == cpu_base->next_timer)
		hrtimer_force_reprogram(cpu_base, 1);
}

/*
 * remove hrtimer, called with base lock held
 */
static inline int
remove_hrtimer(struct hrtimer *timer, struct hrtimer_clock_base *base, bool restart)
{
	if (hrtimer_is_queued(timer)) {
		u8 state = timer->state;
		int reprogram;

		/*
		 * Remove the timer and force reprogramming when high
		 * resolution mode is active and the timer is on the current
		 * CPU. If we remove a timer on another CPU, reprogramming is
		 * skipped. The interrupt event on this CPU is fired and
		 * reprogramming happens in the interrupt handler. This is a
		 * rare case and less expensive than a smp call.
		 */
		reprogram = base->cpu_base == this_cpu_ptr(&hrtimer_bases);

		if (!restart)
			state = HRTIMER_STATE_INACTIVE;

		__remove_hrtimer(timer, base, state, reprogram);
		return 1;
	}
	return 0;
}

static int __hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim,
				    u64 delta_ns, const enum hrtimer_mode mode,
				    struct hrtimer_clock_base *base)
{
	/* Remove an active timer from the queue: */
	remove_hrtimer(timer, base, true);

	if (mode & HRTIMER_MODE_REL)
		tim = ktime_add_safe(tim, base->get_time());

	hrtimer_set_expires_range_ns(timer, tim, delta_ns);

	return enqueue_hrtimer(timer, base, mode);
}

/**
 * hrtimer_start_range_ns - (re)start an hrtimer
 * @timer:	the timer to be added
 * @tim:	expiry time
 * @delta_ns:	"slack" range for the timer
 * @mode:	timer mode: absolute (HRTIMER_MODE_ABS) or
 *		relative (HRTIMER_MODE_REL), and pinned (HRTIMER_MODE_PINNED);
 *		softirq based mode is considered for debug purpose only!
 */
void hrtimer_start_range_ns(struct hrtimer *timer, ktime_t tim,
			    u64 delta_ns, const enum hrtimer_mode mode)
{
	struct hrtimer_clock_base *base;
	unsigned long flags;

	base = lock_hrtimer_base(timer, &flags);

	if (__hrtimer_start_range_ns(timer, tim, delta_ns, mode, base))
		hrtimer_reprogram(timer, true);

	unlock_hrtimer_base(timer, &flags);
}

/**
 * hrtimer_try_to_cancel - try to deactivate a timer
 * @timer:	hrtimer to stop
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 * -1 when the timer is currently executing the callback function and
 *    cannot be stopped
 */
int hrtimer_try_to_cancel(struct hrtimer *timer)
{
	struct hrtimer_clock_base *base;
	unsigned long flags;
	int ret = -1;

	/*
	 * Check lockless first. If the timer is not active (neither
	 * enqueued nor running the callback, nothing to do here.  The
	 * base lock does not serialize against a concurrent enqueue,
	 * so we can avoid taking it.
	 */
	if (!hrtimer_active(timer))
		return 0;

	base = lock_hrtimer_base(timer, &flags);

	if (!hrtimer_callback_running(timer))
		ret = remove_hrtimer(timer, base, false);

	unlock_hrtimer_base(timer, &flags);

	return ret;
}

/**
 * hrtimer_cancel - cancel a timer and wait for the handler to finish.
 * @timer:	the timer to be cancelled
 *
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 */
int hrtimer_cancel(struct hrtimer *timer)
{
	for (;;) {
		int ret = hrtimer_try_to_cancel(timer);

		if (ret >= 0)
			return ret;
		cpu_relax();
	}
}

/**
 * hrtimer_get_remaining - get remaining time for the timer
 * @timer:	the timer to read
 * @adjust:	adjust relative timers when CONFIG_TIME_LOW_RES=y
 */
ktime_t __hrtimer_get_remaining(const struct hrtimer *timer, bool adjust)
{
	unsigned long flags;
	ktime_t rem;

	lock_hrtimer_base(timer, &flags);
	rem = hrtimer_expires_remaining(timer);
	unlock_hrtimer_base(timer, &flags);

	return rem;
}

/**
 * hrtimer_next_event_without - time until next expiry event w/o one timer
 * @exclude:	timer to exclude
 *
 * Returns the next expiry time over all timers except for the @exclude one or
 * KTIME_MAX if none of them is pending.
 */
u64 hrtimer_next_event_without(const struct hrtimer *exclude)
{
	struct hrtimer_cpu_base *cpu_base = this_cpu_ptr(&hrtimer_bases);
	u64 expires = KTIME_MAX;
	unsigned int active;
	unsigned long flags;

	raw_spin_lock_irqsave(&cpu_base->lock, flags);

	active = cpu_base->active_bases & HRTIMER_ACTIVE_HARD;
	expires = __hrtimer_next_event_base(cpu_base, exclude, active,
						expires);

	raw_spin_unlock_irqrestore(&cpu_base->lock, flags);

	return expires;
}

static inline int hrtimer_clockid_to_base(clockid_t clock_id)
{
	if (likely(clock_id < MAX_CLOCKS)) {
		int base = hrtimer_clock_to_base_table[clock_id];

		if (likely(base != HRTIMER_MAX_CLOCK_BASES))
			return base;
	}
	WARN(1, "Invalid clockid %d. Using MONOTONIC\n", clock_id);
	return HRTIMER_BASE_MONOTONIC;
}

static void __hrtimer_init(struct hrtimer *timer, clockid_t clock_id,
			   enum hrtimer_mode mode)
{
	int base = 0;
	struct hrtimer_cpu_base *cpu_base;

	memset(timer, 0, sizeof(struct hrtimer));

	cpu_base = this_cpu_ptr(&hrtimer_bases);

	/*
	 * POSIX magic: Relative CLOCK_REALTIME timers are not affected by
	 * clock modifications, so they needs to become CLOCK_MONOTONIC to
	 * ensure POSIX compliance.
	 */
	if (clock_id == CLOCK_REALTIME && mode & HRTIMER_MODE_REL)
		clock_id = CLOCK_MONOTONIC;

	base += hrtimer_clockid_to_base(clock_id);
	timer->base = &cpu_base->clock_base[base];
	timerqueue_init(&timer->node);
}

/**
 * hrtimer_init - initialize a timer to the given clock
 * @timer:	the timer to be initialized
 * @clock_id:	the clock to be used
 * @mode:       The modes which are relevant for intitialization:
 *              HRTIMER_MODE_ABS, HRTIMER_MODE_REL, HRTIMER_MODE_ABS_SOFT,
 *              HRTIMER_MODE_REL_SOFT
 *
 *              The PINNED variants of the above can be handed in,
 *              but the PINNED bit is ignored as pinning happens
 *              when the hrtimer is started
 */
void hrtimer_init(struct hrtimer *timer, clockid_t clock_id,
		  enum hrtimer_mode mode)
{
	__hrtimer_init(timer, clock_id, mode);
}

/*
 * A timer is active, when it is enqueued into the rbtree or the
 * callback function is running or it's in the state of being migrated
 * to another cpu.
 *
 * It is important for this function to not return a false negative.
 */
bool hrtimer_active(const struct hrtimer *timer)
{
	struct hrtimer_clock_base *base;
	unsigned int seq;

	do {
		base = READ_ONCE(timer->base);
		seq = raw_read_seqcount_begin(&base->seq);

		if (timer->state != HRTIMER_STATE_INACTIVE ||
		    base->running == timer)
			return true;

	} while (read_seqcount_retry(&base->seq, seq) ||
		 base != READ_ONCE(timer->base));

	return false;
}

/*
 * The write_seqcount_barrier()s in __run_hrtimer() split the thing into 3
 * distinct sections:
 *
 *  - queued:	the timer is queued
 *  - callback:	the timer is being ran
 *  - post:	the timer is inactive or (re)queued
 *
 * On the read side we ensure we observe timer->state and cpu_base->running
 * from the same section, if anything changed while we looked at it, we retry.
 * This includes timer->base changing because sequence numbers alone are
 * insufficient for that.
 *
 * The sequence numbers are required because otherwise we could still observe
 * a false negative if the read side got smeared over multiple consequtive
 * __run_hrtimer() invocations.
 */

static void __run_hrtimer(struct hrtimer_cpu_base *cpu_base,
			  struct hrtimer_clock_base *base,
			  struct hrtimer *timer, ktime_t *now,
			  unsigned long flags)
{
	enum hrtimer_restart (*fn)(struct hrtimer *);
	int restart;

	lockdep_assert_held(&cpu_base->lock);

	base->running = timer;

	/*
	 * Separate the ->running assignment from the ->state assignment.
	 *
	 * As with a regular write barrier, this ensures the read side in
	 * hrtimer_active() cannot observe base->running == NULL &&
	 * timer->state == INACTIVE.
	 */
	raw_write_seqcount_barrier(&base->seq);

	__remove_hrtimer(timer, base, HRTIMER_STATE_INACTIVE, 0);
	fn = timer->function;

	/*
	 * The timer is marked as running in the CPU base, so it is
	 * protected against migration to a different CPU even if the lock
	 * is dropped.
	 */
	raw_spin_unlock_irqrestore(&cpu_base->lock, flags);
	restart = fn(timer);
	raw_spin_lock_irq(&cpu_base->lock);

	/*
	 * Note: We clear the running state after enqueue_hrtimer and
	 * we do not reprogram the event hardware. Happens either in
	 * hrtimer_start_range_ns() or in hrtimer_interrupt()
	 *
	 * Note: Because we dropped the cpu_base->lock above,
	 * hrtimer_start_range_ns() can have popped in and enqueued the timer
	 * for us already.
	 */
	if (restart != HRTIMER_NORESTART &&
	    !(timer->state & HRTIMER_STATE_ENQUEUED))
		enqueue_hrtimer(timer, base, HRTIMER_MODE_ABS);

	/*
	 * Separate the ->running assignment from the ->state assignment.
	 *
	 * As with a regular write barrier, this ensures the read side in
	 * hrtimer_active() cannot observe base->running.timer == NULL &&
	 * timer->state == INACTIVE.
	 */
	raw_write_seqcount_barrier(&base->seq);

	WARN_ON_ONCE(base->running != timer);
	base->running = NULL;
}

static void __hrtimer_run_queues(struct hrtimer_cpu_base *cpu_base, ktime_t now,
				 unsigned long flags, unsigned int active_mask)
{
	struct hrtimer_clock_base *base;
	unsigned int active = cpu_base->active_bases & active_mask;

	for_each_active_base(base, cpu_base, active) {
		struct timerqueue_node *node;
		ktime_t basenow;

		basenow = ktime_add(now, base->offset);

		while ((node = timerqueue_getnext(&base->active))) {
			struct hrtimer *timer;

			timer = container_of(node, struct hrtimer, node);

			/*
			 * The immediate goal for using the softexpires is
			 * minimizing wakeups, not running timers at the
			 * earliest interrupt after their soft expiration.
			 * This allows us to avoid using a Priority Search
			 * Tree, which can answer a stabbing querry for
			 * overlapping intervals and instead use the simple
			 * BST we already have.
			 * We don't add extra wakeups by delaying timers that
			 * are right-of a not yet expired timer, because that
			 * timer will have to trigger a wakeup anyway.
			 */
			if (basenow < hrtimer_get_softexpires_tv64(timer))
				break;

			__run_hrtimer(cpu_base, base, timer, &basenow, flags);
		}
	}
}

/*
 * High resolution timer interrupt
 * Called with interrupts disabled
 */
void hrtimer_interrupt(struct clock_event_device *dev)
{
	struct hrtimer_cpu_base *cpu_base = this_cpu_ptr(&hrtimer_bases);
	ktime_t expires_next, now, entry_time, delta;
	unsigned long flags;
	int retries = 0;

	cpu_base->nr_events++;
	dev->next_event = KTIME_MAX;

	raw_spin_lock_irqsave(&cpu_base->lock, flags);
	entry_time = now = hrtimer_update_base(cpu_base);
retry:
	cpu_base->in_hrtirq = 1;
	/*
	 * We set expires_next to KTIME_MAX here with cpu_base->lock
	 * held to prevent that a timer is enqueued in our queue via
	 * the migration code. This does not affect enqueueing of
	 * timers which run their callback and need to be requeued on
	 * this CPU.
	 */
	cpu_base->expires_next = KTIME_MAX;

	__hrtimer_run_queues(cpu_base, now, flags, HRTIMER_ACTIVE_HARD);

	/* Reevaluate the clock bases for the next expiry */
	expires_next = __hrtimer_get_next_event(cpu_base, HRTIMER_ACTIVE_ALL);
	/*
	 * Store the new expiry value so the migration code can verify
	 * against it.
	 */
	cpu_base->expires_next = expires_next;
	cpu_base->in_hrtirq = 0;
	raw_spin_unlock_irqrestore(&cpu_base->lock, flags);

	/* Reprogramming necessary ? */
	if (!tick_program_event(expires_next, 0)) {
		cpu_base->hang_detected = 0;
		return;
	}

	/*
	 * The next timer was already expired due to:
	 * - tracing
	 * - long lasting callbacks
	 * - being scheduled away when running in a VM
	 *
	 * We need to prevent that we loop forever in the hrtimer
	 * interrupt routine. We give it 3 attempts to avoid
	 * overreacting on some spurious event.
	 *
	 * Acquire base lock for updating the offsets and retrieving
	 * the current time.
	 */
	raw_spin_lock_irqsave(&cpu_base->lock, flags);
	now = hrtimer_update_base(cpu_base);
	cpu_base->nr_retries++;
	if (++retries < 3)
		goto retry;
	/*
	 * Give the system a chance to do something else than looping
	 * here. We stored the entry time, so we know exactly how long
	 * we spent here. We schedule the next event this amount of
	 * time away.
	 */
	cpu_base->nr_hangs++;
	cpu_base->hang_detected = 1;
	raw_spin_unlock_irqrestore(&cpu_base->lock, flags);

	delta = ktime_sub(now, entry_time);
	if ((unsigned int)delta > cpu_base->max_hang_time)
		cpu_base->max_hang_time = (unsigned int) delta;
	/*
	 * Limit it to a sensible value as we enforce a longer
	 * delay. Give the CPU at least 100ms to catch up.
	 */
	if (delta > 100 * NSEC_PER_MSEC)
		expires_next = ktime_add_ns(now, 100 * NSEC_PER_MSEC);
	else
		expires_next = ktime_add(now, delta);
	tick_program_event(expires_next, 1);
	pr_warn_once("hrtimer: interrupt took %llu ns\n", ktime_to_ns(delta));
}

/*
 * Functions related to boot-time initialization:
 */
int hrtimers_prepare_cpu(unsigned int cpu)
{
	struct hrtimer_cpu_base *cpu_base = &per_cpu(hrtimer_bases, cpu);
	int i;

	for (i = 0; i < HRTIMER_MAX_CLOCK_BASES; i++) {
		cpu_base->clock_base[i].cpu_base = cpu_base;
		timerqueue_init_head(&cpu_base->clock_base[i].active);
	}

	cpu_base->cpu = cpu;
	cpu_base->active_bases = 0;
	cpu_base->hang_detected = 0;
	cpu_base->next_timer = NULL;
	cpu_base->expires_next = KTIME_MAX;
	return 0;
}

void __init hrtimers_init(void)
{
	hrtimers_prepare_cpu(smp_processor_id());
}
