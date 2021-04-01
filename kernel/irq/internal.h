/* SPDX-License-Identifier: GPL-2.0 */
/*
 * IRQ subsystem internal functions and variables:
 *
 * Do not ever include this file from anything else than
 * kernel/irq/. Do not even think about using any information outside
 * of this file for your non core code.
 */
#ifndef __KERNEL_IRQ_INTERNAL_H_
#define __KERNEL_IRQ_INTERNAL_H_

#include <linux/irqdesc.h>
#include <linux/kernel_stat.h>

#define istate core_internal_state__do_not_mess_with_it

/*
 * Bit masks for desc->core_internal_state__do_not_mess_with_it
 *
 * IRQS_AUTODETECT		- autodetection in progress
 * IRQS_SPURIOUS_DISABLED	- was disabled due to spurious interrupt
 *				  detection
 * IRQS_POLL_INPROGRESS		- polling in progress
 * IRQS_ONESHOT			- irq is not unmasked in primary handler
 * IRQS_REPLAY			- irq is replayed
 * IRQS_WAITING			- irq is waiting
 * IRQS_PENDING			- irq is pending and replayed later
 * IRQS_SUSPENDED		- irq is suspended
 */
enum {
	IRQS_AUTODETECT		= 0x00000001,
	IRQS_SPURIOUS_DISABLED	= 0x00000002,
	IRQS_POLL_INPROGRESS	= 0x00000008,
	IRQS_ONESHOT		= 0x00000020,
	IRQS_REPLAY		= 0x00000040,
	IRQS_WAITING		= 0x00000080,
	IRQS_PENDING		= 0x00000200,
	IRQS_SUSPENDED		= 0x00000800,
	IRQS_TIMINGS		= 0x00001000,
};

#include "settings.h"
#include "debug.h"

#ifdef CONFIG_SMP
extern int irq_setup_affinity(struct irq_desc *desc);
#else
static inline int irq_setup_affinity(struct irq_desc *desc) { return 0; }
#endif

/* Inline functions for support of irq chips on slow busses */
static inline void chip_bus_lock(struct irq_desc *desc)
{
	if (unlikely(desc->irq_data.chip->irq_bus_lock))
		desc->irq_data.chip->irq_bus_lock(&desc->irq_data);
}

static inline void chip_bus_sync_unlock(struct irq_desc *desc)
{
	if (unlikely(desc->irq_data.chip->irq_bus_sync_unlock))
		desc->irq_data.chip->irq_bus_sync_unlock(&desc->irq_data);
}

#define _IRQ_DESC_CHECK		(1 << 0)
#define _IRQ_DESC_PERCPU	(1 << 1)

#define IRQ_GET_DESC_CHECK_GLOBAL	(_IRQ_DESC_CHECK)
#define IRQ_GET_DESC_CHECK_PERCPU	(_IRQ_DESC_CHECK | _IRQ_DESC_PERCPU)

#define for_each_action_of_desc(desc, act)			\
	for (act = desc->action; act; act = act->next)

struct irq_desc *
__irq_get_desc_lock(unsigned int irq, unsigned long *flags, bool bus,
		    unsigned int check);
void __irq_put_desc_unlock(struct irq_desc *desc, unsigned long flags, bool bus);

static inline struct irq_desc *
irq_get_desc_buslock(unsigned int irq, unsigned long *flags, unsigned int check)
{
	return __irq_get_desc_lock(irq, flags, true, check);
}

static inline void
irq_put_desc_busunlock(struct irq_desc *desc, unsigned long flags)
{
	__irq_put_desc_unlock(desc, flags, true);
}

static inline struct irq_desc *
irq_get_desc_lock(unsigned int irq, unsigned long *flags, unsigned int check)
{
	return __irq_get_desc_lock(irq, flags, false, check);
}

static inline void
irq_put_desc_unlock(struct irq_desc *desc, unsigned long flags)
{
	__irq_put_desc_unlock(desc, flags, false);
}

static inline void kstat_incr_irqs_this_cpu(struct irq_desc *desc)
{
	__this_cpu_inc(*desc->kstat_irqs);
	__this_cpu_inc(kstat.irqs_sum);
}

#define __irqd_to_state(d) ACCESS_PRIVATE((d)->common, state_use_accessors)

static inline unsigned int irqd_get(struct irq_data *d)
{
	return __irqd_to_state(d);
}

/*
 * Manipulation functions for irq_data.state
 */
static inline void irqd_set_move_pending(struct irq_data *d)
{
	__irqd_to_state(d) |= IRQD_SETAFFINITY_PENDING;
}

static inline void irqd_clr_move_pending(struct irq_data *d)
{
	__irqd_to_state(d) &= ~IRQD_SETAFFINITY_PENDING;
}

static inline void irqd_set_managed_shutdown(struct irq_data *d)
{
	__irqd_to_state(d) |= IRQD_MANAGED_SHUTDOWN;
}

static inline void irqd_clr_managed_shutdown(struct irq_data *d)
{
	__irqd_to_state(d) &= ~IRQD_MANAGED_SHUTDOWN;
}

static inline void irqd_clear(struct irq_data *d, unsigned int mask)
{
	__irqd_to_state(d) &= ~mask;
}

static inline void irqd_set(struct irq_data *d, unsigned int mask)
{
	__irqd_to_state(d) |= mask;
}

static inline bool irqd_has_set(struct irq_data *d, unsigned int mask)
{
	return __irqd_to_state(d) & mask;
}

static inline void irq_state_set_disabled(struct irq_desc *desc)
{
	irqd_set(&desc->irq_data, IRQD_IRQ_DISABLED);
}

static inline void irq_state_set_masked(struct irq_desc *desc)
{
	irqd_set(&desc->irq_data, IRQD_IRQ_MASKED);
}

#define IRQ_BITMAP_BITS	(NR_IRQS + 8196)

#endif /* __KERNEL_IRQ_INTERNAL_H_ */
