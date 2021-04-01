// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the interrupt descriptor management code. Detailed
 * information is available in Documentation/core-api/genericirq.rst
 *
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/radix-tree.h>
#include <linux/bitmap.h>
#include <linux/irqdomain.h>
#include <linux/lockdep.h>

#include "internal.h"

/*
 * lockdep: we want to handle all irq_desc locks as a single lock-class:
 */
static struct lock_class_key irq_desc_lock_class;

#if defined(CONFIG_SMP)
static int __init irq_affinity_setup(char *str)
{
	alloc_bootmem_cpumask_var(&irq_default_affinity);
	cpulist_parse(str, irq_default_affinity);
	/*
	 * Set at least the boot cpu. We don't want to end up with
	 * bugreports caused by random comandline masks
	 */
	cpumask_set_cpu(smp_processor_id(), irq_default_affinity);
	return 1;
}
__setup("irqaffinity=", irq_affinity_setup);

static void __init init_irq_default_affinity(void)
{
	if (!cpumask_available(irq_default_affinity))
		zalloc_cpumask_var(&irq_default_affinity, GFP_KERNEL);
	if (cpumask_empty(irq_default_affinity))
		cpumask_setall(irq_default_affinity);
}
#else
static void __init init_irq_default_affinity(void)
{
}
#endif

#ifdef CONFIG_SMP
static int alloc_masks(struct irq_desc *desc, int node)
{
	if (!zalloc_cpumask_var_node(&desc->irq_common_data.affinity,
				     GFP_KERNEL, node))
		return -ENOMEM;

	if (!zalloc_cpumask_var_node(&desc->irq_common_data.effective_affinity,
				     GFP_KERNEL, node)) {
		free_cpumask_var(desc->irq_common_data.affinity);
		return -ENOMEM;
	}

	return 0;
}

static void desc_smp_init(struct irq_desc *desc, int node,
			  const struct cpumask *affinity)
{
	if (!affinity)
		affinity = irq_default_affinity;
	cpumask_copy(desc->irq_common_data.affinity, affinity);

	desc->irq_common_data.node = node;
}

#else
static inline int
alloc_masks(struct irq_desc *desc, int node) { return 0; }
static inline void
desc_smp_init(struct irq_desc *desc, int node, const struct cpumask *affinity) { }
#endif

static void desc_set_defaults(unsigned int irq, struct irq_desc *desc, int node,
			      const struct cpumask *affinity, struct module *owner)
{
	int cpu;

	desc->irq_common_data.handler_data = NULL;
	desc->irq_common_data.msi_desc = NULL;

	desc->irq_data.common = &desc->irq_common_data;
	desc->irq_data.irq = irq;
	desc->irq_data.chip = &no_irq_chip;
	desc->irq_data.chip_data = NULL;
	irq_settings_clr_and_set(desc, ~0, _IRQ_DEFAULT_INIT_FLAGS);
	irqd_set(&desc->irq_data, IRQD_IRQ_DISABLED);
	irqd_set(&desc->irq_data, IRQD_IRQ_MASKED);
	desc->handle_irq = handle_bad_irq;
	desc->depth = 1;
	desc->irq_count = 0;
	desc->irqs_unhandled = 0;
	desc->name = NULL;
	desc->owner = owner;
	for_each_possible_cpu(cpu)
		*per_cpu_ptr(desc->kstat_irqs, cpu) = 0;
	desc_smp_init(desc, node, affinity);
}

int nr_irqs = NR_IRQS;

static DECLARE_BITMAP(allocated_irqs, IRQ_BITMAP_BITS);

static RADIX_TREE(irq_desc_tree, GFP_KERNEL);

static void irq_insert_desc(unsigned int irq, struct irq_desc *desc)
{
	radix_tree_insert(&irq_desc_tree, irq, desc);
}

struct irq_desc *irq_to_desc(unsigned int irq)
{
	return radix_tree_lookup(&irq_desc_tree, irq);
}

static void delete_irq_desc(unsigned int irq)
{
	radix_tree_delete(&irq_desc_tree, irq);
}

#ifdef CONFIG_SMP
static void free_masks(struct irq_desc *desc)
{
	free_cpumask_var(desc->irq_common_data.affinity);
	free_cpumask_var(desc->irq_common_data.effective_affinity);
}
#else
static inline void free_masks(struct irq_desc *desc) { }
#endif

void irq_lock_sparse(void)
{
	/* TODO */
}

void irq_unlock_sparse(void)
{
	/* TODO */
}

static struct irq_desc *alloc_desc(int irq, int node, unsigned int flags,
				   const struct cpumask *affinity,
				   struct module *owner)
{
	struct irq_desc *desc;

	desc = kzalloc_node(sizeof(*desc), GFP_KERNEL, node);
	if (!desc)
		return NULL;
	/* allocate based on nr_cpu_ids */
	desc->kstat_irqs = alloc_percpu(unsigned int);
	if (!desc->kstat_irqs)
		goto err_desc;

	if (alloc_masks(desc, node))
		goto err_kstat;

	raw_spin_lock_init(&desc->lock);
	lockdep_set_class(&desc->lock, &irq_desc_lock_class);
	//mutex_init(&desc->request_mutex); /* TODO */
	//init_rcu_head(&desc->rcu);

	desc_set_defaults(irq, desc, node, affinity, owner);
	irqd_set(&desc->irq_data, flags);

	return desc;

err_kstat:
	free_percpu(desc->kstat_irqs);
err_desc:
	kfree(desc);
	return NULL;
}

static void irq_release(struct irq_desc *desc)
{
	free_masks(desc);
	free_percpu(desc->kstat_irqs);
	kfree(desc);
}

static void free_desc(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	/*
	 * sparse_irq_lock protects also show_interrupts() and
	 * kstat_irq_usr(). Once we deleted the descriptor from the
	 * sparse tree we can free it. Access in proc will fail to
	 * lookup the descriptor.
	 *
	 * The sysfs entry must be serialized against a concurrent
	 * irq_sysfs_init() as well.
	 */
	delete_irq_desc(irq);

	/*
	 * We free the descriptor, masks and stat fields via RCU. That
	 * allows demultiplex interrupts to do rcu based management of
	 * the child interrupts.
	 * This also allows us to use rcu in kstat_irqs_usr().
	 */
	irq_release(desc);
}

static int alloc_descs(unsigned int start, unsigned int cnt, int node,
		       const struct irq_affinity_desc *affinity,
		       struct module *owner)
{
	struct irq_desc *desc;
	int i;

	/* Validate affinity mask(s) */
	if (affinity) {
		for (i = 0; i < cnt; i++) {
			if (cpumask_empty(&affinity[i].mask))
				return -EINVAL;
		}
	}

	for (i = 0; i < cnt; i++) {
		const struct cpumask *mask = NULL;
		unsigned int flags = 0;

		if (affinity) {
			if (affinity->is_managed) {
				flags = IRQD_AFFINITY_MANAGED |
					IRQD_MANAGED_SHUTDOWN;
			}
			mask = &affinity->mask;
			node = cpu_to_node(cpumask_first(mask));
			affinity++;
		}

		desc = alloc_desc(start + i, node, flags, mask, owner);
		if (!desc)
			goto err;
		irq_insert_desc(start + i, desc);
	}
	bitmap_set(allocated_irqs, start, cnt);
	return start;

err:
	for (i--; i >= 0; i--)
		free_desc(start + i);
	return -ENOMEM;
}

static int irq_expand_nr_irqs(unsigned int nr)
{
	if (nr > IRQ_BITMAP_BITS)
		return -ENOMEM;
	nr_irqs = nr;
	return 0;
}

int __init early_irq_init(void)
{
	int i, initcnt, node = first_online_node;
	struct irq_desc *desc;

	init_irq_default_affinity();

	/* Let arch update nr_irqs and return the nr of preallocated irqs */
	initcnt = arch_probe_nr_irqs();
	printk(KERN_INFO "NR_IRQS: %d, nr_irqs: %d, preallocated irqs: %d\n",
	       NR_IRQS, nr_irqs, initcnt);

	if (WARN_ON(nr_irqs > IRQ_BITMAP_BITS))
		nr_irqs = IRQ_BITMAP_BITS;

	if (WARN_ON(initcnt > IRQ_BITMAP_BITS))
		initcnt = IRQ_BITMAP_BITS;

	if (initcnt > nr_irqs)
		nr_irqs = initcnt;

	for (i = 0; i < initcnt; i++) {
		desc = alloc_desc(i, node, 0, NULL, NULL);
		set_bit(i, allocated_irqs);
		irq_insert_desc(i, desc);
	}
	return arch_early_irq_init();
}

/**
 * generic_handle_irq - Invoke the handler for a particular irq
 * @irq:	The irq number to handle
 *
 */
int generic_handle_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc)
		return -EINVAL;
	generic_handle_irq_desc(desc);
	return 0;
}

/**
 * __handle_domain_irq - Invoke the handler for a HW irq belonging to a domain
 * @domain:	The domain where to perform the lookup
 * @hwirq:	The HW irq number to convert to a logical one
 * @lookup:	Whether to perform the domain lookup or not
 * @regs:	Register file coming from the low-level handling code
 *
 * Returns:	0 on success, or -EINVAL if conversion has failed
 */
int __handle_domain_irq(struct irq_domain *domain, unsigned int hwirq,
			bool lookup, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	unsigned int irq = hwirq;
	int ret = 0;

	irq_enter();

	if (lookup)
		irq = irq_find_mapping(domain, hwirq);

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (unlikely(!irq || irq >= nr_irqs)) {
		ack_bad_irq(irq);
		ret = -EINVAL;
	} else {
		generic_handle_irq(irq);
	}

	irq_exit();
	set_irq_regs(old_regs);
	return ret;
}

/* Dynamic interrupt handling */

/**
 * irq_free_descs - free irq descriptors
 * @from:	Start of descriptor range
 * @cnt:	Number of consecutive irqs to free
 */
void irq_free_descs(unsigned int from, unsigned int cnt)
{
	int i;

	if (from >= nr_irqs || (from + cnt) > nr_irqs)
		return;

	//mutex_lock(&sparse_irq_lock);
	for (i = 0; i < cnt; i++)
		free_desc(from + i);

	bitmap_clear(allocated_irqs, from, cnt);
	//mutex_unlock(&sparse_irq_lock);
}

/**
 * irq_alloc_descs - allocate and initialize a range of irq descriptors
 * @irq:	Allocate for specific irq number if irq >= 0
 * @from:	Start the search from this irq number
 * @cnt:	Number of consecutive irqs to allocate.
 * @node:	Preferred node on which the irq descriptor should be allocated
 * @owner:	Owning module (can be NULL)
 * @affinity:	Optional pointer to an affinity mask array of size @cnt which
 *		hints where the irq descriptors should be allocated and which
 *		default affinities to use
 *
 * Returns the first irq number or error code
 */
int __ref
__irq_alloc_descs(int irq, unsigned int from, unsigned int cnt, int node,
		  struct module *owner, const struct irq_affinity_desc *affinity)
{
	int start, ret;

	if (!cnt)
		return -EINVAL;

	if (irq >= 0) {
		if (from > irq)
			return -EINVAL;
		from = irq;
	} else {
		/*
		 * For interrupts which are freely allocated the
		 * architecture can force a lower bound to the @from
		 * argument. x86 uses this to exclude the GSI space.
		 */
		from = arch_dynirq_lower_bound(from);
	}

	//mutex_lock(&sparse_irq_lock);

	start = bitmap_find_next_zero_area(allocated_irqs, IRQ_BITMAP_BITS,
					   from, cnt, 0);
	ret = -EEXIST;
	if (irq >=0 && start != irq)
		goto unlock;

	if (start + cnt > nr_irqs) {
		ret = irq_expand_nr_irqs(start + cnt);
		if (ret)
			goto unlock;
	}
	ret = alloc_descs(start, cnt, node, affinity, owner);
unlock:
	//mutex_unlock(&sparse_irq_lock);
	return ret;
}

/**
 * irq_get_next_irq - get next allocated irq number
 * @offset:	where to start the search
 *
 * Returns next irq number after offset or nr_irqs if none is found.
 */
unsigned int irq_get_next_irq(unsigned int offset)
{
	return find_next_bit(allocated_irqs, nr_irqs, offset);
}

struct irq_desc *
__irq_get_desc_lock(unsigned int irq, unsigned long *flags, bool bus,
		    unsigned int check)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (desc) {
		if (check & _IRQ_DESC_CHECK) {
			if ((check & _IRQ_DESC_PERCPU) &&
			    !irq_settings_is_per_cpu_devid(desc))
				return NULL;

			if (!(check & _IRQ_DESC_PERCPU) &&
			    irq_settings_is_per_cpu_devid(desc))
				return NULL;
		}

		if (bus)
			chip_bus_lock(desc);
		raw_spin_lock_irqsave(&desc->lock, *flags);
	}
	return desc;
}

void __irq_put_desc_unlock(struct irq_desc *desc, unsigned long flags, bool bus)
{
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	if (bus)
		chip_bus_sync_unlock(desc);
}

int irq_set_percpu_devid_partition(unsigned int irq,
				   const struct cpumask *affinity)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc)
		return -EINVAL;

	if (desc->percpu_enabled)
		return -EINVAL;

	desc->percpu_enabled = kzalloc(sizeof(*desc->percpu_enabled), GFP_KERNEL);

	if (!desc->percpu_enabled)
		return -ENOMEM;

	if (affinity)
		desc->percpu_affinity = affinity;
	else
		desc->percpu_affinity = cpu_possible_mask;

	irq_set_percpu_devid_flags(irq);
	return 0;
}

int irq_set_percpu_devid(unsigned int irq)
{
	return irq_set_percpu_devid_partition(irq, NULL);
}

int irq_get_percpu_devid_partition(unsigned int irq, struct cpumask *affinity)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc || !desc->percpu_enabled)
		return -EINVAL;

	if (affinity)
		cpumask_copy(affinity, desc->percpu_affinity);

	return 0;
}

void kstat_incr_irq_this_cpu(unsigned int irq)
{
	kstat_incr_irqs_this_cpu(irq_to_desc(irq));
}

/**
 * kstat_irqs_cpu - Get the statistics for an interrupt on a cpu
 * @irq:	The interrupt number
 * @cpu:	The cpu number
 *
 * Returns the sum of interrupt counts on @cpu since boot for
 * @irq. The caller must ensure that the interrupt is not removed
 * concurrently.
 */
unsigned int kstat_irqs_cpu(unsigned int irq, int cpu)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return desc && desc->kstat_irqs ?
			*per_cpu_ptr(desc->kstat_irqs, cpu) : 0;
}

/**
 * kstat_irqs - Get the statistics for an interrupt
 * @irq:	The interrupt number
 *
 * Returns the sum of interrupt counts on all cpus since boot for
 * @irq. The caller must ensure that the interrupt is not removed
 * concurrently.
 */
unsigned int kstat_irqs(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int cpu;
	unsigned int sum = 0;

	if (!desc || !desc->kstat_irqs)
		return 0;
	for_each_possible_cpu(cpu)
		sum += *per_cpu_ptr(desc->kstat_irqs, cpu);
	return sum;
}

/**
 * kstat_irqs_usr - Get the statistics for an interrupt
 * @irq:	The interrupt number
 *
 * Returns the sum of interrupt counts on all cpus since boot for @irq.
 * Contrary to kstat_irqs() this can be called from any context.
 * It uses rcu since a concurrent removal of an interrupt descriptor is
 * observing an rcu grace period before delayed_free_desc()/irq_kobj_release().
 */
unsigned int kstat_irqs_usr(unsigned int irq)
{
	unsigned int sum;

	rcu_read_lock();
	sum = kstat_irqs(irq);
	rcu_read_unlock();
	return sum;
}
