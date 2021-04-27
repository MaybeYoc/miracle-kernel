/* SPDX-License-Identifier: GPL-2.0 */


# define TICK_DO_TIMER_NONE	-1
# define TICK_DO_TIMER_BOOT	-2

DECLARE_PER_CPU(struct tick_device, tick_cpu_device);
extern ktime_t tick_next_period;
extern ktime_t tick_period;
extern int tick_do_timer_cpu __read_mostly;

extern void tick_check_new_device(struct clock_event_device *dev);

/*
 * tick internal variable and functions used by low/high res code
 */

static inline enum clock_event_state clockevent_get_state(struct clock_event_device *dev)
{
	return dev->state_use_accessors;
}

static inline void clockevent_set_state(struct clock_event_device *dev,
					enum clock_event_state state)
{
	dev->state_use_accessors = state;
}

extern void tick_setup_periodic(struct clock_event_device *dev, int broadcast);
extern void tick_handle_periodic(struct clock_event_device *dev);
extern void tick_check_new_device(struct clock_event_device *dev);
extern void tick_shutdown(unsigned int cpu);
extern void tick_suspend(void);
extern void tick_resume(void);
extern bool tick_check_replacement(struct clock_event_device *curdev,
				   struct clock_event_device *newdev);
extern void tick_install_replacement(struct clock_event_device *dev);
extern int tick_is_oneshot_available(void);
extern struct tick_device *tick_get_device(int cpu);

static inline int tick_oneshot_mode_active(void) { return 1; }
