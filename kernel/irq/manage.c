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
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/wait.h>

#include "internal.h"

__read_mostly bool force_irqthreads = false;

static void __synchronize_hardirq(struct irq_desc *desc)
{
	bool inprogress;

	do {
		unsigned long flags;

		/*
		 * Wait until we're out of the critical section.  This might
		 * give the wrong answer due to the lack of memory barriers.
		 */
		while (irqd_irq_inprogress(&desc->irq_data))
			cpu_relax();

		/* Ok, that indicated we're done: double-check carefully. */
		raw_spin_lock_irqsave(&desc->lock, flags);
		inprogress = irqd_irq_inprogress(&desc->irq_data);
		raw_spin_unlock_irqrestore(&desc->lock, flags);

		/* Oops, that failed? */
	} while (inprogress);
}

/**
 *	synchronize_hardirq - wait for pending hard IRQ handlers (on other CPUs)
 *	@irq: interrupt number to wait for
 *
 *	This function waits for any pending hard IRQ handlers for this
 *	interrupt to complete before returning. If you use this
 *	function while holding a resource the IRQ handler may need you
 *	will deadlock. It does not take associated threaded handlers
 *	into account.
 *
 *	Do not use this for shutdown scenarios where you must be sure
 *	that all parts (hardirq and threaded handler) have completed.
 *
 *	Returns: false if a threaded handler is active.
 *
 *	This function may be called - with care - from IRQ context.
 */
bool synchronize_hardirq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc) {
		__synchronize_hardirq(desc);
		return !atomic_read(&desc->threads_active);
	}

	return true;
}

/**
 *	synchronize_irq - wait for pending IRQ handlers (on other CPUs)
 *	@irq: interrupt number to wait for
 *
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */
void synchronize_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc) {
		__synchronize_hardirq(desc);
		/*
		 * We made sure that no hardirq handler is
		 * running. Now verify that no threaded handlers are
		 * active.
		 */
		wait_event(desc->wait_for_threads,
			   !atomic_read(&desc->threads_active));
	}
}

#ifdef CONFIG_SMP
cpumask_var_t irq_default_affinity;

static bool __irq_can_set_affinity(struct irq_desc *desc)
{
	if (!desc || !irqd_can_balance(&desc->irq_data) ||
	    !desc->irq_data.chip || !desc->irq_data.chip->irq_set_affinity)
		return false;
	return true;
}

/**
 *	irq_can_set_affinity - Check if the affinity of a given irq can be set
 *	@irq:		Interrupt to check
 *
 */
int irq_can_set_affinity(unsigned int irq)
{
	return __irq_can_set_affinity(irq_to_desc(irq));
}

/**
 * irq_can_set_affinity_usr - Check if affinity of a irq can be set from user space
 * @irq:	Interrupt to check
 *
 * Like irq_can_set_affinity() above, but additionally checks for the
 * AFFINITY_MANAGED flag.
 */
bool irq_can_set_affinity_usr(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return __irq_can_set_affinity(desc) &&
		!irqd_affinity_is_managed(&desc->irq_data);
}

/**
 *	irq_set_thread_affinity - Notify irq threads to adjust affinity
 *	@desc:		irq descriptor which has affitnity changed
 *
 *	We just set IRQTF_AFFINITY and delegate the affinity setting
 *	to the interrupt thread itself. We can not call
 *	set_cpus_allowed_ptr() here as we hold desc->lock and this
 *	code can be called from hard interrupt context.
 */
void irq_set_thread_affinity(struct irq_desc *desc)
{
	struct irqaction *action;

	for_each_action_of_desc(desc, action)
		if (action->thread)
			set_bit(IRQTF_AFFINITY, &action->thread_flags);
}

static void irq_validate_effective_affinity(struct irq_data *data)
{
	const struct cpumask *m = irq_data_get_effective_affinity_mask(data);
	struct irq_chip *chip = irq_data_get_irq_chip(data);

	if (!cpumask_empty(m))
		return;
	pr_warn_once("irq_chip %s did not update eff. affinity mask of irq %u\n",
		     chip->name, data->irq);
}

int irq_do_set_affinity(struct irq_data *data, const struct cpumask *mask,
			bool force)
{
	struct irq_desc *desc = irq_data_to_desc(data);
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	int ret;

	if (!chip || !chip->irq_set_affinity)
		return -EINVAL;

	ret = chip->irq_set_affinity(data, mask, force);
	switch (ret) {
	case IRQ_SET_MASK_OK:
	case IRQ_SET_MASK_OK_DONE:
		cpumask_copy(desc->irq_common_data.affinity, mask);
	case IRQ_SET_MASK_OK_NOCOPY:
		irq_validate_effective_affinity(data);
		irq_set_thread_affinity(desc);
		ret = 0;
	}

	return ret;
}

static inline int irq_set_affinity_pending(struct irq_data *data,
					   const struct cpumask *dest)
{
	return -EBUSY;
}

static int irq_try_set_affinity(struct irq_data *data,
				const struct cpumask *dest, bool force)
{
	int ret = irq_do_set_affinity(data, dest, force);

	/*
	 * In case that the underlying vector management is busy and the
	 * architecture supports the generic pending mechanism then utilize
	 * this to avoid returning an error to user space.
	 */
	if (ret == -EBUSY && !force)
		ret = irq_set_affinity_pending(data, dest);
	return ret;
}

int irq_set_affinity_locked(struct irq_data *data, const struct cpumask *mask,
			    bool force)
{
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	struct irq_desc *desc = irq_data_to_desc(data);
	int ret = 0;

	if (!chip || !chip->irq_set_affinity)
		return -EINVAL;

	if (irq_can_move_pcntxt(data) && !irqd_is_setaffinity_pending(data)) {
		ret = irq_try_set_affinity(data, mask, force);
	} else {
		irqd_set_move_pending(data);
		irq_copy_pending(desc, mask);
	}

	if (desc->affinity_notify) {
		;
		//kref_get(&desc->affinity_notify->kref);
		//schedule_work(&desc->affinity_notify->work);
	}
	irqd_set(data, IRQD_AFFINITY_SET);

	return ret;
}

int __irq_set_affinity(unsigned int irq, const struct cpumask *mask, bool force)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;
	int ret;

	if (!desc)
		return -EINVAL;

	raw_spin_lock_irqsave(&desc->lock, flags);
	ret = irq_set_affinity_locked(irq_desc_get_irq_data(desc), mask, force);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return ret;
}

int irq_set_affinity_hint(unsigned int irq, const struct cpumask *m)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return -EINVAL;
	desc->affinity_hint = m;
	irq_put_desc_unlock(desc, flags);
	/* set the initial affinity to prevent every interrupt being on CPU0 */
	if (m)
		__irq_set_affinity(irq, m, false);
	return 0;
}

/**
 *	irq_set_affinity_notifier - control notification of IRQ affinity changes
 *	@irq:		Interrupt for which to enable/disable notification
 *	@notify:	Context for notification, or %NULL to disable
 *			notification.  Function pointers must be initialised;
 *			the other fields will be initialised by this function.
 *
 *	Must be called in process context.  Notification may only be enabled
 *	after the IRQ is allocated and must be disabled before the IRQ is
 *	freed using free_irq().
 */
int
irq_set_affinity_notifier(unsigned int irq, struct irq_affinity_notify *notify)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_affinity_notify *old_notify;
	unsigned long flags;

	/* The release function is promised process context */
	//might_sleep();

	if (!desc)
		return -EINVAL;

	/* Complete initialisation of *notify */
	if (notify) {
		notify->irq = irq;
		//kref_init(&notify->kref);
		INIT_WORK(&notify->work, irq_affinity_notify);
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	old_notify = desc->affinity_notify;
	desc->affinity_notify = notify;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	//if (old_notify)
	//	kref_put(&old_notify->kref, old_notify->release);

	return 0;
}

/*
 * Generic version of the affinity autoselector.
 */
int irq_setup_affinity(struct irq_desc *desc)
{
	struct cpumask *set = irq_default_affinity;
	int ret, node = irq_desc_get_node(desc);
	static DEFINE_RAW_SPINLOCK(mask_lock);
	static struct cpumask mask;

	/* Excludes PER_CPU and NO_BALANCE interrupts */
	if (!__irq_can_set_affinity(desc))
		return 0;

	raw_spin_lock(&mask_lock);
	/*
	 * Preserve the managed affinity setting and a userspace affinity
	 * setup, but make sure that one of the targets is online.
	 */
	if (irqd_affinity_is_managed(&desc->irq_data) ||
	    irqd_has_set(&desc->irq_data, IRQD_AFFINITY_SET)) {
		if (cpumask_intersects(desc->irq_common_data.affinity,
				       cpu_online_mask))
			set = desc->irq_common_data.affinity;
		else
			irqd_clear(&desc->irq_data, IRQD_AFFINITY_SET);
	}

	cpumask_and(&mask, cpu_online_mask, set);
	if (cpumask_empty(&mask))
		cpumask_copy(&mask, cpu_online_mask);

	if (node != NUMA_NO_NODE) {
		const struct cpumask *nodemask = cpumask_of_node(node);

		/* make sure at least one of the cpus in nodemask is online */
		if (cpumask_intersects(&mask, nodemask))
			cpumask_and(&mask, &mask, nodemask);
	}
	ret = irq_do_set_affinity(&desc->irq_data, &mask, false);
	raw_spin_unlock(&mask_lock);
	return ret;
}

/*
 * Called when a bogus affinity is set via /proc/irq
 */
int irq_select_affinity_usr(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&desc->lock, flags);
	ret = irq_setup_affinity(desc);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return ret;
}
#endif

/**
 *	irq_set_vcpu_affinity - Set vcpu affinity for the interrupt
 *	@irq: interrupt number to set affinity
 *	@vcpu_info: vCPU specific data or pointer to a percpu array of vCPU
 *	            specific data for percpu_devid interrupts
 *
 *	This function uses the vCPU specific data to set the vCPU
 *	affinity for an irq. The vCPU specific data is passed from
 *	outside, such as KVM. One example code path is as below:
 *	KVM -> IOMMU -> irq_set_vcpu_affinity().
 */
int irq_set_vcpu_affinity(unsigned int irq, void *vcpu_info)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, 0);
	struct irq_data *data;
	struct irq_chip *chip;
	int ret = -ENOSYS;

	if (!desc)
		return -EINVAL;

	data = irq_desc_get_irq_data(desc);
	do {
		chip = irq_data_get_irq_chip(data);
		if (chip && chip->irq_set_vcpu_affinity)
			break;
		data = data->parent_data;
	} while (data);

	if (data)
		ret = chip->irq_set_vcpu_affinity(data, vcpu_info);
	irq_put_desc_unlock(desc, flags);

	return ret;
}

void __disable_irq(struct irq_desc *desc)
{
	if (!desc->depth++)
		irq_disable(desc);
}

static int __disable_irq_nosync(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return -EINVAL;
	__disable_irq(desc);
	irq_put_desc_busunlock(desc, flags);
	return 0;
}

/**
 *	disable_irq_nosync - disable an irq without waiting
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Disables and Enables are
 *	nested.
 *	Unlike disable_irq(), this function does not ensure existing
 *	instances of the IRQ handler have completed before returning.
 *
 *	This function may be called from IRQ context.
 */
void disable_irq_nosync(unsigned int irq)
{
	__disable_irq_nosync(irq);
}

/**
 *	disable_irq - disable an irq and wait for completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Enables and Disables are
 *	nested.
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */
void disable_irq(unsigned int irq)
{
	if (!__disable_irq_nosync(irq))
		synchronize_irq(irq);
}

/**
 *	disable_hardirq - disables an irq and waits for hardirq completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Enables and Disables are
 *	nested.
 *	This function waits for any pending hard IRQ handlers for this
 *	interrupt to complete before returning. If you use this function while
 *	holding a resource the hard IRQ handler may need you will deadlock.
 *
 *	When used to optimistically disable an interrupt from atomic context
 *	the return value must be checked.
 *
 *	Returns: false if a threaded handler is active.
 *
 *	This function may be called - with care - from IRQ context.
 */
bool disable_hardirq(unsigned int irq)
{
	if (!__disable_irq_nosync(irq))
		return synchronize_hardirq(irq);

	return false;
}

void __enable_irq(struct irq_desc *desc)
{
	switch (desc->depth) {
	case 0:
 err_out:
		WARN(1, KERN_WARNING "Unbalanced enable for IRQ %d\n",
		     irq_desc_get_irq(desc));
		break;
	case 1: {
		if (desc->istate & IRQS_SUSPENDED)
			goto err_out;
		/* Prevent probing on this irq: */
		irq_settings_set_noprobe(desc);
		/*
		 * Call irq_startup() not irq_enable() here because the
		 * interrupt might be marked NOAUTOEN. So irq_startup()
		 * needs to be invoked when it gets enabled the first
		 * time. If it was already started up, then irq_startup()
		 * will invoke irq_enable() under the hood.
		 */
		irq_startup(desc, IRQ_RESEND, IRQ_START_FORCE);
		break;
	}
	default:
		desc->depth--;
	}
}

/**
 *	enable_irq - enable handling of an irq
 *	@irq: Interrupt to enable
 *
 *	Undoes the effect of one call to disable_irq().  If this
 *	matches the last disable, processing of interrupts on this
 *	IRQ line is re-enabled.
 *
 *	This function may be called from IRQ context only when
 *	desc->irq_data.chip->bus_lock and desc->chip->bus_sync_unlock are NULL !
 */
void enable_irq(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);

	if (!desc)
		return;
	if (WARN(!desc->irq_data.chip,
		 KERN_ERR "enable_irq before setup/request_irq: irq %u\n", irq))
		goto out;

	__enable_irq(desc);
out:
	irq_put_desc_busunlock(desc, flags);
}

static int set_irq_wake_real(unsigned int irq, unsigned int on)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int ret = -ENXIO;

	if (irq_desc_get_chip(desc)->flags &  IRQCHIP_SKIP_SET_WAKE)
		return 0;

	if (desc->irq_data.chip->irq_set_wake)
		ret = desc->irq_data.chip->irq_set_wake(&desc->irq_data, on);

	return ret;
}

/**
 *	irq_set_irq_wake - control irq power management wakeup
 *	@irq:	interrupt to control
 *	@on:	enable/disable power management wakeup
 *
 *	Enable/disable power management wakeup mode, which is
 *	disabled by default.  Enables and disables must match,
 *	just as they match for non-wakeup mode support.
 *
 *	Wakeup mode lets this IRQ wake the system from sleep
 *	states like "suspend to RAM".
 */
int irq_set_irq_wake(unsigned int irq, unsigned int on)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, IRQ_GET_DESC_CHECK_GLOBAL);
	int ret = 0;

	if (!desc)
		return -EINVAL;

	/* wakeup-capable irqs can be shared between drivers that
	 * don't need to have the same sleep mode behaviors.
	 */
	if (on) {
		if (desc->wake_depth++ == 0) {
			ret = set_irq_wake_real(irq, on);
			if (ret)
				desc->wake_depth = 0;
			else
				irqd_set(&desc->irq_data, IRQD_WAKEUP_STATE);
		}
	} else {
		if (desc->wake_depth == 0) {
			WARN(1, "Unbalanced IRQ %d wake disable\n", irq);
		} else if (--desc->wake_depth == 0) {
			ret = set_irq_wake_real(irq, on);
			if (ret)
				desc->wake_depth = 1;
			else
				irqd_clear(&desc->irq_data, IRQD_WAKEUP_STATE);
		}
	}
	irq_put_desc_busunlock(desc, flags);
	return ret;
}

/*
 * Internal function that tells the architecture code whether a
 * particular irq has been exclusively allocated or is available
 * for driver use.
 */
int can_request_irq(unsigned int irq, unsigned long irqflags)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, 0);
	int canrequest = 0;

	if (!desc)
		return 0;

	if (irq_settings_can_request(desc)) {
		if (!desc->action ||
		    irqflags & desc->action->flags & IRQF_SHARED)
			canrequest = 1;
	}
	irq_put_desc_unlock(desc, flags);
	return canrequest;
}

int __irq_set_trigger(struct irq_desc *desc, unsigned long flags)
{
	struct irq_chip *chip = desc->irq_data.chip;
	int ret, unmask = 0;

	if (!chip || !chip->irq_set_type) {
		/*
		 * IRQF_TRIGGER_* but the PIC does not support multiple
		 * flow-types?
		 */
		pr_debug("No set_type function for IRQ %d (%s)\n",
			 irq_desc_get_irq(desc),
			 chip ? (chip->name ? : "unknown") : "unknown");
		return 0;
	}

	if (chip->flags & IRQCHIP_SET_TYPE_MASKED) {
		if (!irqd_irq_masked(&desc->irq_data))
			mask_irq(desc);
		if (!irqd_irq_disabled(&desc->irq_data))
			unmask = 1;
	}

	/* Mask all flags except trigger mode */
	flags &= IRQ_TYPE_SENSE_MASK;
	ret = chip->irq_set_type(&desc->irq_data, flags);

	switch (ret) {
	case IRQ_SET_MASK_OK:
	case IRQ_SET_MASK_OK_DONE:
		irqd_clear(&desc->irq_data, IRQD_TRIGGER_MASK);
		irqd_set(&desc->irq_data, flags);

	case IRQ_SET_MASK_OK_NOCOPY:
		flags = irqd_get_trigger_type(&desc->irq_data);
		irq_settings_set_trigger_mask(desc, flags);
		irqd_clear(&desc->irq_data, IRQD_LEVEL);
		irq_settings_clr_level(desc);
		if (flags & IRQ_TYPE_LEVEL_MASK) {
			irq_settings_set_level(desc);
			irqd_set(&desc->irq_data, IRQD_LEVEL);
		}

		ret = 0;
		break;
	default:
		pr_err("Setting trigger mode %lu for irq %u failed (%pF)\n",
		       flags, irq_desc_get_irq(desc), chip->irq_set_type);
	}
	if (unmask)
		unmask_irq(desc);
	return ret;
}

int irq_set_parent(int irq, int parent_irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, 0);

	if (!desc)
		return -EINVAL;

	desc->parent_irq = parent_irq;

	irq_put_desc_unlock(desc, flags);
	return 0;
}

/*
 * Default primary interrupt handler for threaded interrupts. Is
 * assigned as primary handler when request_threaded_irq is called
 * with handler == NULL. Useful for oneshot interrupts.
 */
static irqreturn_t irq_default_primary_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

/**
 *	irq_wake_thread - wake the irq thread for the action identified by dev_id
 *	@irq:		Interrupt line
 *	@dev_id:	Device identity for which the thread should be woken
 *
 */
void irq_wake_thread(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	unsigned long flags;

	if (!desc || WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return;

	raw_spin_lock_irqsave(&desc->lock, flags);
	for_each_action_of_desc(desc, action) {
		if (action->dev_id == dev_id) {
			if (action->thread)
				__irq_wake_thread(desc, action);
			break;
		}
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

static void irq_release_resources(struct irq_desc *desc)
{
	struct irq_data *d = &desc->irq_data;
	struct irq_chip *c = d->chip;

	if (c->irq_release_resources)
		c->irq_release_resources(d);
}

/*
 * Internal function to unregister an irqaction - used to free
 * regular and special interrupts that are part of the architecture.
 */
static struct irqaction *__free_irq(struct irq_desc *desc, void *dev_id)
{
	unsigned irq = desc->irq_data.irq;
	struct irqaction *action, **action_ptr;
	unsigned long flags;

	WARN(in_interrupt(), "Trying to free IRQ %d from IRQ context!\n", irq);

	mutex_lock(&desc->request_mutex);
	chip_bus_lock(desc);
	raw_spin_lock_irqsave(&desc->lock, flags);

	/*
	 * There can be multiple actions per IRQ descriptor, find the right
	 * one based on the dev_id:
	 */
	action_ptr = &desc->action;
	for (;;) {
		action = *action_ptr;

		if (!action) {
			WARN(1, "Trying to free already-free IRQ %d\n", irq);
			raw_spin_unlock_irqrestore(&desc->lock, flags);
			chip_bus_sync_unlock(desc);
			mutex_unlock(&desc->request_mutex);
			return NULL;
		}

		if (action->dev_id == dev_id)
			break;
		action_ptr = &action->next;
	}

	/* Found it - now remove it from the list of entries: */
	*action_ptr = action->next;

	irq_pm_remove_action(desc, action);

	/* If this was the last handler, shut down the IRQ line: */
	if (!desc->action) {
		irq_settings_clr_disable_unlazy(desc);
		irq_shutdown(desc);
	}

#ifdef CONFIG_SMP
	/* make sure affinity_hint is cleaned up */
	if (WARN_ON_ONCE(desc->affinity_hint))
		desc->affinity_hint = NULL;
#endif

	raw_spin_unlock_irqrestore(&desc->lock, flags);
	/*
	 * Drop bus_lock here so the changes which were done in the chip
	 * callbacks above are synced out to the irq chips which hang
	 * behind a slow bus (I2C, SPI) before calling synchronize_hardirq().
	 *
	 * Aside of that the bus_lock can also be taken from the threaded
	 * handler in irq_finalize_oneshot() which results in a deadlock
	 * because kthread_stop() would wait forever for the thread to
	 * complete, which is blocked on the bus lock.
	 *
	 * The still held desc->request_mutex() protects against a
	 * concurrent request_irq() of this irq so the release of resources
	 * and timing data is properly serialized.
	 */
	chip_bus_sync_unlock(desc);

	unregister_handler_proc(irq, action);

	/* Make sure it's not being used on another CPU: */
	synchronize_hardirq(irq);

	/*
	 * The action has already been removed above, but the thread writes
	 * its oneshot mask bit when it completes. Though request_mutex is
	 * held across this which prevents __setup_irq() from handing out
	 * the same bit to a newly requested action.
	 */
	if (action->thread) {
		kthread_stop(action->thread);
		put_task_struct(action->thread);
		if (action->secondary && action->secondary->thread) {
			kthread_stop(action->secondary->thread);
			put_task_struct(action->secondary->thread);
		}
	}

	/* Last action releases resources */
	if (!desc->action) {
		/*
		 * Reaquire bus lock as irq_release_resources() might
		 * require it to deallocate resources over the slow bus.
		 */
		chip_bus_lock(desc);
		irq_release_resources(desc);
		chip_bus_sync_unlock(desc);
		irq_remove_timings(desc);
	}

	mutex_unlock(&desc->request_mutex);

	irq_chip_pm_put(&desc->irq_data);
	module_put(desc->owner);
	kfree(action->secondary);
	return action;
}

/**
 *	remove_irq - free an interrupt
 *	@irq: Interrupt line to free
 *	@act: irqaction for the interrupt
 *
 * Used to remove interrupts statically setup by the early boot process.
 */
void remove_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc && !WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		__free_irq(desc, act->dev_id);
}

/**
 *	free_irq - free an interrupt allocated with request_irq
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove an interrupt handler. The handler is removed and if the
 *	interrupt line is no longer in use by any driver it is disabled.
 *	On a shared IRQ the caller must ensure the interrupt is disabled
 *	on the card it drives before calling this function. The function
 *	does not return until any executing interrupts for this IRQ
 *	have completed.
 *
 *	This function must not be called from interrupt context.
 *
 *	Returns the devname argument passed to request_irq.
 */
const void *free_irq(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	const char *devname;

	if (!desc || WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return NULL;

#ifdef CONFIG_SMP
	if (WARN_ON(desc->affinity_notify))
		desc->affinity_notify = NULL;
#endif

	action = __free_irq(desc, dev_id);

	if (!action)
		return NULL;

	devname = action->name;
	kfree(action);
	return devname;
}

/**
 *	request_threaded_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *		  Primary handler for threaded interrupts
 *		  If NULL and thread_fn != NULL the default
 *		  primary handler is installed
 *	@thread_fn: Function called from the irq handler thread
 *		    If NULL, no irq thread is created
 *	@irqflags: Interrupt type flags
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. From the point this
 *	call is made your handler function may be invoked. Since
 *	your handler function must clear any interrupt the board
 *	raises, you must take care both to initialise your hardware
 *	and to set up the interrupt handler in the right order.
 *
 *	If you want to set up a threaded irq handler for your device
 *	then you need to supply @handler and @thread_fn. @handler is
 *	still called in hard interrupt context and has to check
 *	whether the interrupt originates from the device. If yes it
 *	needs to disable the interrupt on the device and return
 *	IRQ_WAKE_THREAD which will wake up the handler thread and run
 *	@thread_fn. This split handler design is necessary to support
 *	shared interrupts.
 *
 *	Dev_id must be globally unique. Normally the address of the
 *	device data structure is used as the cookie. Since the handler
 *	receives this value it makes sense to use it.
 *
 *	If your interrupt is shared you must pass a non NULL dev_id
 *	as this is required when freeing the interrupt.
 *
 *	Flags:
 *
 *	IRQF_SHARED		Interrupt is shared
 *	IRQF_TRIGGER_*		Specify active edge(s) or level
 *
 */
int request_threaded_irq(unsigned int irq, irq_handler_t handler,
			 irq_handler_t thread_fn, unsigned long irqflags,
			 const char *devname, void *dev_id)
{
	struct irqaction *action;
	struct irq_desc *desc;
	int retval;

	if (irq == IRQ_NOTCONNECTED)
		return -ENOTCONN;

	/*
	 * Sanity-check: shared interrupts must pass in a real dev-ID,
	 * otherwise we'll have trouble later trying to figure out
	 * which interrupt is which (messes up the interrupt freeing
	 * logic etc).
	 *
	 * Also IRQF_COND_SUSPEND only makes sense for shared interrupts and
	 * it cannot be set along with IRQF_NO_SUSPEND.
	 */
	if (((irqflags & IRQF_SHARED) && !dev_id) ||
	    (!(irqflags & IRQF_SHARED) && (irqflags & IRQF_COND_SUSPEND)) ||
	    ((irqflags & IRQF_NO_SUSPEND) && (irqflags & IRQF_COND_SUSPEND)))
		return -EINVAL;

	desc = irq_to_desc(irq);
	if (!desc)
		return -EINVAL;

	if (!irq_settings_can_request(desc) ||
	    WARN_ON(irq_settings_is_per_cpu_devid(desc)))
		return -EINVAL;

	if (!handler) {
		if (!thread_fn)
			return -EINVAL;
		handler = irq_default_primary_handler;
	}

	action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->thread_fn = thread_fn;
	action->flags = irqflags;
	action->name = devname;
	action->dev_id = dev_id;

	retval = irq_chip_pm_get(&desc->irq_data);
	if (retval < 0) {
		kfree(action);
		return retval;
	}

	//retval = __setup_irq(irq, desc, action);

	if (retval) {
		irq_chip_pm_put(&desc->irq_data);
		kfree(action->secondary);
		kfree(action);
	}

	return retval;
}

/**
 *	request_any_context_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *		  Threaded handler for threaded interrupts.
 *	@flags: Interrupt type flags
 *	@name: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. It selects either a
 *	hardirq or threaded handling method depending on the
 *	context.
 *
 *	On failure, it returns a negative value. On success,
 *	it returns either IRQC_IS_HARDIRQ or IRQC_IS_NESTED.
 */
int request_any_context_irq(unsigned int irq, irq_handler_t handler,
			    unsigned long flags, const char *name, void *dev_id)
{
	struct irq_desc *desc;
	int ret;

	if (irq == IRQ_NOTCONNECTED)
		return -ENOTCONN;

	desc = irq_to_desc(irq);
	if (!desc)
		return -EINVAL;

	if (irq_settings_is_nested_thread(desc)) {
		ret = request_threaded_irq(irq, NULL, handler,
					   flags, name, dev_id);
		return !ret ? IRQC_IS_NESTED : ret;
	}

	ret = request_irq(irq, handler, flags, name, dev_id);
	return !ret ? IRQC_IS_HARDIRQ : ret;
}

void enable_percpu_irq(unsigned int irq, unsigned int type)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_PERCPU);

	if (!desc)
		return;

	/*
	 * If the trigger type is not specified by the caller, then
	 * use the default for this interrupt.
	 */
	type &= IRQ_TYPE_SENSE_MASK;
	if (type == IRQ_TYPE_NONE)
		type = irqd_get_trigger_type(&desc->irq_data);

	if (type != IRQ_TYPE_NONE) {
		int ret;

		ret = __irq_set_trigger(desc, type);

		if (ret) {
			WARN(1, "failed to set type for IRQ%d\n", irq);
			goto out;
		}
	}

	irq_percpu_enable(desc, cpu);
out:
	irq_put_desc_unlock(desc, flags);
}

/**
 * irq_percpu_is_enabled - Check whether the per cpu irq is enabled
 * @irq:	Linux irq number to check for
 *
 * Must be called from a non migratable context. Returns the enable
 * state of a per cpu interrupt on the current cpu.
 */
bool irq_percpu_is_enabled(unsigned int irq)
{
	unsigned int cpu = smp_processor_id();
	struct irq_desc *desc;
	unsigned long flags;
	bool is_enabled;

	desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_PERCPU);
	if (!desc)
		return false;

	is_enabled = cpumask_test_cpu(cpu, desc->percpu_enabled);
	irq_put_desc_unlock(desc, flags);

	return is_enabled;
}

void disable_percpu_irq(unsigned int irq)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_lock(irq, &flags, IRQ_GET_DESC_CHECK_PERCPU);

	if (!desc)
		return;

	irq_percpu_disable(desc, cpu);
	irq_put_desc_unlock(desc, flags);
}

/*
 * Internal function to unregister a percpu irqaction.
 */
static struct irqaction *__free_percpu_irq(unsigned int irq, void __percpu *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	unsigned long flags;

	WARN(in_interrupt(), "Trying to free IRQ %d from IRQ context!\n", irq);

	if (!desc)
		return NULL;

	raw_spin_lock_irqsave(&desc->lock, flags);

	action = desc->action;
	if (!action || action->percpu_dev_id != dev_id) {
		WARN(1, "Trying to free already-free IRQ %d\n", irq);
		goto bad;
	}

	if (!cpumask_empty(desc->percpu_enabled)) {
		WARN(1, "percpu IRQ %d still enabled on CPU%d!\n",
		     irq, cpumask_first(desc->percpu_enabled));
		goto bad;
	}

	/* Found it - now remove it from the list of entries: */
	desc->action = NULL;

	raw_spin_unlock_irqrestore(&desc->lock, flags);

	unregister_handler_proc(irq, action);

	irq_chip_pm_put(&desc->irq_data);
	module_put(desc->owner);
	return action;

bad:
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return NULL;
}

/**
 *	remove_percpu_irq - free a per-cpu interrupt
 *	@irq: Interrupt line to free
 *	@act: irqaction for the interrupt
 *
 * Used to remove interrupts statically setup by the early boot process.
 */
void remove_percpu_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc && irq_settings_is_per_cpu_devid(desc))
	    __free_percpu_irq(irq, act->percpu_dev_id);
}

/**
 *	free_percpu_irq - free an interrupt allocated with request_percpu_irq
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove a percpu interrupt handler. The handler is removed, but
 *	the interrupt line is not disabled. This must be done on each
 *	CPU before calling this function. The function does not return
 *	until any executing interrupts for this IRQ have completed.
 *
 *	This function must not be called from interrupt context.
 */
void free_percpu_irq(unsigned int irq, void __percpu *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc || !irq_settings_is_per_cpu_devid(desc))
		return;

	chip_bus_lock(desc);
	kfree(__free_percpu_irq(irq, dev_id));
	chip_bus_sync_unlock(desc);
}

/**
 *	setup_percpu_irq - setup a per-cpu interrupt
 *	@irq: Interrupt line to setup
 *	@act: irqaction for the interrupt
 *
 * Used to statically setup per-cpu interrupts in the early boot process.
 */
int setup_percpu_irq(unsigned int irq, struct irqaction *act)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int retval;

	if (!desc || !irq_settings_is_per_cpu_devid(desc))
		return -EINVAL;

	retval = irq_chip_pm_get(&desc->irq_data);
	if (retval < 0)
		return retval;

	//retval = __setup_irq(irq, desc, act);

	if (retval)
		irq_chip_pm_put(&desc->irq_data);

	return retval;
}

/**
 *	__request_percpu_irq - allocate a percpu interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs.
 *	@flags: Interrupt type flags (IRQF_TIMER only)
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A percpu cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt on the local CPU. If the interrupt is supposed to be
 *	enabled on other CPUs, it has to be done on each CPU using
 *	enable_percpu_irq().
 *
 *	Dev_id must be globally unique. It is a per-cpu variable, and
 *	the handler gets called with the interrupted CPU's instance of
 *	that variable.
 */
int __request_percpu_irq(unsigned int irq, irq_handler_t handler,
			 unsigned long flags, const char *devname,
			 void __percpu *dev_id)
{
	struct irqaction *action;
	struct irq_desc *desc;
	int retval;

	if (!dev_id)
		return -EINVAL;

	desc = irq_to_desc(irq);
	if (!desc || !irq_settings_can_request(desc) ||
	    !irq_settings_is_per_cpu_devid(desc))
		return -EINVAL;

	if (flags && flags != IRQF_TIMER)
		return -EINVAL;

	action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = flags | IRQF_PERCPU | IRQF_NO_SUSPEND;
	action->name = devname;
	action->percpu_dev_id = dev_id;

	retval = irq_chip_pm_get(&desc->irq_data);
	if (retval < 0) {
		kfree(action);
		return retval;
	}

	//retval = __setup_irq(irq, desc, action);

	if (retval) {
		irq_chip_pm_put(&desc->irq_data);
		kfree(action);
	}

	return retval;
}

/**
 *	irq_get_irqchip_state - returns the irqchip state of a interrupt.
 *	@irq: Interrupt line that is forwarded to a VM
 *	@which: One of IRQCHIP_STATE_* the caller wants to know about
 *	@state: a pointer to a boolean where the state is to be storeed
 *
 *	This call snapshots the internal irqchip state of an
 *	interrupt, returning into @state the bit corresponding to
 *	stage @which
 *
 *	This function should be called with preemption disabled if the
 *	interrupt controller has per-cpu registers.
 */
int irq_get_irqchip_state(unsigned int irq, enum irqchip_irq_state which,
			  bool *state)
{
	struct irq_desc *desc;
	struct irq_data *data;
	struct irq_chip *chip;
	unsigned long flags;
	int err = -EINVAL;

	desc = irq_get_desc_buslock(irq, &flags, 0);
	if (!desc)
		return err;

	data = irq_desc_get_irq_data(desc);

	do {
		chip = irq_data_get_irq_chip(data);
		if (chip->irq_get_irqchip_state)
			break;
		data = data->parent_data;
	} while (data);

	if (data)
		err = chip->irq_get_irqchip_state(data, which, state);

	irq_put_desc_busunlock(desc, flags);
	return err;
}

/**
 *	irq_set_irqchip_state - set the state of a forwarded interrupt.
 *	@irq: Interrupt line that is forwarded to a VM
 *	@which: State to be restored (one of IRQCHIP_STATE_*)
 *	@val: Value corresponding to @which
 *
 *	This call sets the internal irqchip state of an interrupt,
 *	depending on the value of @which.
 *
 *	This function should be called with preemption disabled if the
 *	interrupt controller has per-cpu registers.
 */
int irq_set_irqchip_state(unsigned int irq, enum irqchip_irq_state which,
			  bool val)
{
	struct irq_desc *desc;
	struct irq_data *data;
	struct irq_chip *chip;
	unsigned long flags;
	int err = -EINVAL;

	desc = irq_get_desc_buslock(irq, &flags, 0);
	if (!desc)
		return err;

	data = irq_desc_get_irq_data(desc);

	do {
		chip = irq_data_get_irq_chip(data);
		if (chip->irq_set_irqchip_state)
			break;
		data = data->parent_data;
	} while (data);

	if (data)
		err = chip->irq_set_irqchip_state(data, which, val);

	irq_put_desc_busunlock(desc, flags);
	return err;
}
