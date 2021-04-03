/* SPDX-License-Identifier: GPL-2.0 */

extern void tick_check_new_device(struct clock_event_device *dev);

/*
 * tick internal variable and functions used by low/high res code
 */

static inline void clockevent_set_state(struct clock_event_device *dev,
					enum clock_event_state state)
{
	dev->state_use_accessors = state;
}
