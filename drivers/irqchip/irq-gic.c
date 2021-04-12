/*
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Interrupt architecture for the GIC:
 *
 * o There is one Interrupt Distributor, which receives interrupts
 *   from system devices and sends them to the Interrupt Controllers.
 *
 * o There is one CPU Interface per CPU, which sends interrupts sent
 *   by the Distributor, and interrupts generated locally, to the
 *   associated CPU. The base address of the CPU interface is usually
 *   aliased so that the same address points to different chips depending
 *   on the CPU it is accessed from.
 *
 * Note that IRQs 0-31 are special - they are local to each CPU.
 * As such, the enable set/clear, pending set/clear and active bit
 * registers are banked per-cpu for these sources.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/irqchip.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/irqdesc.h>
#include <linux/irqnr.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/jump_label.h>
#include <linux/cpuhotplug.h>

#include <asm/exception.h>
#include <asm/ptrace.h>
#include <asm/virt.h>

#include "irq-gic-common.h"

#define gic_check_cpu_features()	do { } while(0)

union gic_base {
	void __iomem *common_base;
	void __percpu * __iomem *percpu_base;
};

struct gic_chip_data {
	struct irq_chip chip;
	union gic_base dist_base;
	union gic_base cpu_base;
	void __iomem *raw_dist_base;
	void __iomem *raw_cpu_base;
	u32 percpu_offset;
	struct irq_domain *domain;
	unsigned int gic_irqs;
};

#define gic_lock_irqsave(f)		do { (void)(f); } while(0)
#define gic_unlock_irqrestore(f)	do { (void)(f); } while(0)

#define gic_lock()			do { } while(0)
#define gic_unlock()			do { } while(0)

/*
 * The GIC mapping of CPU interfaces does not necessarily match
 * the logical CPU numbering.  Let's use a mapping as returned
 * by the GIC itself.
 */
#define NR_GIC_CPU_IF 8
static u8 gic_cpu_map[NR_GIC_CPU_IF] __read_mostly;

static DEFINE_STATIC_KEY_TRUE(supports_deactivate_key);

static struct gic_chip_data gic_data[CONFIG_ARM_GIC_MAX_NR] __read_mostly;

static struct gic_kvm_info gic_v2_kvm_info;

#define gic_data_dist_base(d)	((d)->dist_base.common_base)
#define gic_data_cpu_base(d)	((d)->cpu_base.common_base)
#define gic_set_base_accessor(d, f)

static inline void __iomem *gic_dist_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);
	return gic_data_dist_base(gic_data);
}

static inline void __iomem *gic_cpu_base(struct irq_data *d)
{
	struct gic_chip_data *gic_data = irq_data_get_irq_chip_data(d);
	return gic_data_cpu_base(gic_data);
}

static inline unsigned int gic_irq(struct irq_data *d)
{
	return d->hwirq;
}

static inline bool cascading_gic_irq(struct irq_data *d)
{
	void *data = irq_data_get_irq_handler_data(d);

	/*
	 * If handler_data is set, this is a cascading interrupt, and
	 * it cannot possibly be forwarded.
	 */
	return data != NULL;
}

/*
 * Routines to acknowledge, disable and enable interrupts
 */
static void gic_poke_irq(struct irq_data *d, u32 offset)
{
	u32 mask = 1 << (gic_irq(d) % 32);
	writel_relaxed(mask, gic_dist_base(d) + offset + (gic_irq(d) / 32) * 4);
}

static int gic_peek_irq(struct irq_data *d, u32 offset)
{
	u32 mask = 1 << (gic_irq(d) % 32);
	return !!(readl_relaxed(gic_dist_base(d) + offset + (gic_irq(d) / 32) * 4) & mask);
}

static void gic_mask_irq(struct irq_data *d)
{
	gic_poke_irq(d, GIC_DIST_ENABLE_CLEAR);
}

static void gic_eoimode1_mask_irq(struct irq_data *d)
{
	gic_mask_irq(d);
	/*
	 * When masking a forwarded interrupt, make sure it is
	 * deactivated as well.
	 *
	 * This ensures that an interrupt that is getting
	 * disabled/masked will not get "stuck", because there is
	 * noone to deactivate it (guest is being terminated).
	 */
	if (irqd_is_forwarded_to_vcpu(d))
		gic_poke_irq(d, GIC_DIST_ACTIVE_CLEAR);
}

static void gic_unmask_irq(struct irq_data *d)
{
	gic_poke_irq(d, GIC_DIST_ENABLE_SET);
}

static void gic_eoi_irq(struct irq_data *d)
{
	writel_relaxed(gic_irq(d), gic_cpu_base(d) + GIC_CPU_EOI);
}

static void gic_eoimode1_eoi_irq(struct irq_data *d)
{
	/* Do not deactivate an IRQ forwarded to a vcpu. */
	if (irqd_is_forwarded_to_vcpu(d))
		return;

	writel_relaxed(gic_irq(d), gic_cpu_base(d) + GIC_CPU_DEACTIVATE);
}

static int gic_irq_set_irqchip_state(struct irq_data *d,
				     enum irqchip_irq_state which, bool val)
{
	u32 reg;

	switch (which) {
	case IRQCHIP_STATE_PENDING:
		reg = val ? GIC_DIST_PENDING_SET : GIC_DIST_PENDING_CLEAR;
		break;

	case IRQCHIP_STATE_ACTIVE:
		reg = val ? GIC_DIST_ACTIVE_SET : GIC_DIST_ACTIVE_CLEAR;
		break;

	case IRQCHIP_STATE_MASKED:
		reg = val ? GIC_DIST_ENABLE_CLEAR : GIC_DIST_ENABLE_SET;
		break;

	default:
		return -EINVAL;
	}

	gic_poke_irq(d, reg);
	return 0;
}

static int gic_irq_get_irqchip_state(struct irq_data *d,
				      enum irqchip_irq_state which, bool *val)
{
	switch (which) {
	case IRQCHIP_STATE_PENDING:
		*val = gic_peek_irq(d, GIC_DIST_PENDING_SET);
		break;

	case IRQCHIP_STATE_ACTIVE:
		*val = gic_peek_irq(d, GIC_DIST_ACTIVE_SET);
		break;

	case IRQCHIP_STATE_MASKED:
		*val = !gic_peek_irq(d, GIC_DIST_ENABLE_SET);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int gic_set_type(struct irq_data *d, unsigned int type)
{
	void __iomem *base = gic_dist_base(d);
	unsigned int gicirq = gic_irq(d);

	/* Interrupt configuration for SGIs can't be changed */
	if (gicirq < 16)
		return -EINVAL;

	/* SPIs have restrictions on the supported types */
	if (gicirq >= 32 && type != IRQ_TYPE_LEVEL_HIGH &&
			    type != IRQ_TYPE_EDGE_RISING)
		return -EINVAL;

	return gic_configure_irq(gicirq, type, base, NULL);
}

static int gic_irq_set_vcpu_affinity(struct irq_data *d, void *vcpu)
{
	/* Only interrupts on the primary GIC can be forwarded to a vcpu. */
	if (cascading_gic_irq(d))
		return -EINVAL;

	if (vcpu)
		irqd_set_forwarded_to_vcpu(d);
	else
		irqd_clr_forwarded_to_vcpu(d);
	return 0;
}

#ifdef CONFIG_SMP
static int gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val,
			    bool force)
{
	void __iomem *reg = gic_dist_base(d) + GIC_DIST_TARGET + (gic_irq(d) & ~3);
	unsigned int cpu, shift = (gic_irq(d) % 4) * 8;
	u32 val, mask;
	unsigned long flags;

	if (!force)
	;
	//	cpu = cpumask_any_and(mask_val, cpu_online_mask);
	else
		cpu = cpumask_first(mask_val);

	//if (cpu >= NR_GIC_CPU_IF || cpu >= nr_possible_cpu_ids)
	//	return -EINVAL;

	gic_lock_irqsave(flags);
	mask = 0xff << shift;
	//bit = gic_cpu_map[cpu] << shift;
	val = readl_relaxed(reg) & ~mask;
	//writel_relaxed(val | bit, reg);
	gic_unlock_irqrestore(flags);

	//irq_data_update_effective_affinity(d, cpumask_of(cpu));

	return IRQ_SET_MASK_OK_DONE;
}
#endif

static void __exception_irq_entry gic_handle_irq(struct pt_regs *regs)
{
	u32 irqstat, irqnr;
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *cpu_base = gic_data_cpu_base(gic);

	do {
		irqstat = readl_relaxed(cpu_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		if (likely(irqnr > 15 && irqnr < 1020)) {
			if (static_branch_likely(&supports_deactivate_key))
				writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
			isb();
			handle_domain_irq(gic->domain, irqnr, regs);
			continue;
		}
		if (irqnr < 16) {
			writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
			if (static_branch_likely(&supports_deactivate_key))
				writel_relaxed(irqstat, cpu_base + GIC_CPU_DEACTIVATE);
#ifdef CONFIG_SMP
			/*
			 * Ensure any shared data written by the CPU sending
			 * the IPI is read after we've read the ACK register
			 * on the GIC.
			 *
			 * Pairs with the write barrier in gic_raise_softirq
			 */
			smp_rmb();
			handle_IPI(irqnr, regs);
#endif
			continue;
		}
		break;
	} while (1);
}

static void gic_handle_cascade_irq(struct irq_desc *desc)
{
	struct gic_chip_data *chip_data = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int cascade_irq, gic_irq;
	unsigned long status;

	chained_irq_enter(chip, desc);

	status = readl_relaxed(gic_data_cpu_base(chip_data) + GIC_CPU_INTACK);

	gic_irq = (status & GICC_IAR_INT_ID_MASK);
	if (gic_irq == GICC_INT_SPURIOUS)
		goto out;

	cascade_irq = irq_find_mapping(chip_data->domain, gic_irq);
	if (unlikely(gic_irq < 32 || gic_irq > 1020)) {
		handle_bad_irq(desc);
	} else {
		isb();
		generic_handle_irq(cascade_irq);
	}

 out:
	chained_irq_exit(chip, desc);
}

static const struct irq_chip gic_chip = {
	.irq_mask			= gic_mask_irq,
	.irq_unmask			= gic_unmask_irq,
	.irq_eoi			= gic_eoi_irq,
	.irq_set_type		= gic_set_type,
	.irq_get_irqchip_state	= gic_irq_get_irqchip_state,
	.irq_set_irqchip_state	= gic_irq_set_irqchip_state,
	.flags			= IRQCHIP_SET_TYPE_MASKED |
				  IRQCHIP_SKIP_SET_WAKE |
				  IRQCHIP_MASK_ON_SUSPEND,
};

void __init gic_cascade_irq(unsigned int gic_nr, unsigned int irq)
{
	BUG_ON(gic_nr >= CONFIG_ARM_GIC_MAX_NR);
	irq_set_chained_handler_and_data(irq, gic_handle_cascade_irq,
					 &gic_data[gic_nr]);
}

static u8 gic_get_cpumask(struct gic_chip_data *gic)
{
	void __iomem *base = gic_data_dist_base(gic);
	u32 mask, i;

	for (i = mask = 0; i < 32; i += 4) {
		mask = readl_relaxed(base + GIC_DIST_TARGET + i);
		mask |= mask >> 16;
		mask |= mask >> 8;
		if (mask)
			break;
	}

	if (!mask && nr_possible_cpu_ids > 1)
		pr_crit("GIC CPU mask not found - kernel will fail to boot.\n");

	return mask;
}

static bool gic_check_gicv2(void __iomem *base)
{
	u32 val = readl_relaxed(base + GIC_CPU_IDENT);
	return (val & 0xff0fff) == 0x02043B;
}

static void gic_cpu_if_up(struct gic_chip_data *gic)
{
	void __iomem *cpu_base = gic_data_cpu_base(gic);
	u32 bypass = 0;
	u32 mode = 0;
	int i;

	if (gic == &gic_data[0] && static_branch_likely(&supports_deactivate_key))
		mode = GIC_CPU_CTRL_EOImodeNS;

	if (gic_check_gicv2(cpu_base))
		for (i = 0; i < 4; i++)
			writel_relaxed(0, cpu_base + GIC_CPU_ACTIVEPRIO + i * 4);

	/*
	* Preserve bypass disable bits to be written back later
	*/
	bypass = readl(cpu_base + GIC_CPU_CTRL);
	bypass &= GICC_DIS_BYPASS_MASK;

	writel_relaxed(bypass | mode | GICC_ENABLE, cpu_base + GIC_CPU_CTRL);
}

static void gic_dist_init(struct gic_chip_data *gic)
{
	unsigned int i;
	u32 cpumask;
	unsigned int gic_irqs = gic->gic_irqs;
	void __iomem *base = gic_data_dist_base(gic);

	writel_relaxed(GICD_DISABLE, base + GIC_DIST_CTRL);

	/*
	 * Set all global interrupts to this CPU only.
	 */
	cpumask = gic_get_cpumask(gic);
	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;
	for (i = 32; i < gic_irqs; i += 4)
		writel_relaxed(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);

	gic_dist_config(base, gic_irqs, NULL);

	writel_relaxed(GICD_ENABLE, base + GIC_DIST_CTRL);
}

static int gic_cpu_init(struct gic_chip_data *gic)
{
	void __iomem *dist_base = gic_data_dist_base(gic);
	void __iomem *base = gic_data_cpu_base(gic);
	unsigned int cpu_mask, cpu = smp_processor_id();
	int i;

	/*
	 * Setting up the CPU map is only relevant for the primary GIC
	 * because any nested/secondary GICs do not directly interface
	 * with the CPU(s).
	 */
	if (gic == &gic_data[0]) {
		/*
		 * Get what the GIC says our CPU mask is.
		 */
		if (WARN_ON(cpu >= NR_GIC_CPU_IF))
			return -EINVAL;

		gic_check_cpu_features();
		cpu_mask = gic_get_cpumask(gic);
		gic_cpu_map[cpu] = cpu_mask;

		/*
		 * Clear our mask from the other map entries in case they're
		 * still undefined.
		 */
		for (i = 0; i < NR_GIC_CPU_IF; i++)
			if (i != cpu)
				gic_cpu_map[i] &= ~cpu_mask;
	}

	gic_cpu_config(dist_base, NULL);

	writel_relaxed(GICC_INT_PRI_THRESHOLD, base + GIC_CPU_PRIMASK);
	gic_cpu_if_up(gic);

	return 0;
}

int gic_cpu_if_down(unsigned int gic_nr)
{
	void __iomem *cpu_base;
	u32 val = 0;

	if (gic_nr >= CONFIG_ARM_GIC_MAX_NR)
		return -EINVAL;

	cpu_base = gic_data_cpu_base(&gic_data[gic_nr]);
	val = readl(cpu_base + GIC_CPU_CTRL);
	val &= ~GICC_ENABLE;
	writel_relaxed(val, cpu_base + GIC_CPU_CTRL);

	return 0;
}

static int gic_pm_init(struct gic_chip_data *gic)
{
	return 0;
}

#ifdef CONFIG_SMP
static void gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	int cpu;
	unsigned long flags, map = 0;

	if (unlikely(nr_possible_cpu_ids == 1)) {
		/* Only one CPU? let's do a self-IPI... */
		writel_relaxed(2 << 24 | irq,
			       gic_data_dist_base(&gic_data[0]) + GIC_DIST_SOFTINT);
		return;
	}

	gic_lock_irqsave(flags);

	/* Convert our logical CPU mask into a physical one. */
	for_each_cpu_mask(cpu, mask)
		map |= gic_cpu_map[cpu];

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before they observe us issuing the IPI.
	 */
	dmb(ishst);

	/* this always happens on GIC0 */
	writel_relaxed(map << 16 | irq, gic_data_dist_base(&gic_data[0]) + GIC_DIST_SOFTINT);

	gic_unlock_irqrestore(flags);
}
#endif

#define gic_init_physaddr(node)  do { } while (0)

static int gic_irq_domain_map(struct irq_domain *d, unsigned int irq,
				irq_hw_number_t hw)
{
	struct gic_chip_data *gic = d->host_data;

	if (hw < 32) {
		irq_set_percpu_devid(irq);
		irq_domain_set_info(d, irq, hw, &gic->chip, d->host_data,
				    handle_percpu_devid_irq, NULL, NULL);
		irq_set_status_flags(irq, IRQ_NOAUTOEN);
	} else {
		irq_domain_set_info(d, irq, hw, &gic->chip, d->host_data,
				    handle_fasteoi_irq, NULL, NULL);
		irq_set_probe(irq);
		irqd_set_single_target(irq_desc_get_irq_data(irq_to_desc(irq)));
	}
	return 0;
}

static void gic_irq_domain_unmap(struct irq_domain *d, unsigned int irq)
{
}

static int gic_irq_domain_translate(struct irq_domain *d,
				    struct irq_fwspec *fwspec,
				    unsigned long *hwirq,
				    unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count < 3)
			return -EINVAL;

		/* Get the interrupt number and add 16 to skip over SGIs */
		*hwirq = fwspec->param[1] + 16;

		/*
		 * For SPIs, we need to add 16 more to get the GIC irq
		 * ID number
		 */
		if (!fwspec->param[0])
			*hwirq += 16;

		*type = fwspec->param[2] & IRQ_TYPE_SENSE_MASK;

		/* Make it clear that broken DTs are... broken */
		WARN_ON(*type == IRQ_TYPE_NONE);
		return 0;
	}

	if (is_fwnode_irqchip(fwspec->fwnode)) {
		if(fwspec->param_count != 2)
			return -EINVAL;

		*hwirq = fwspec->param[0];
		*type = fwspec->param[1];

		WARN_ON(*type == IRQ_TYPE_NONE);
		return 0;
	}

	return -EINVAL;
}

static int gic_starting_cpu(unsigned int cpu)
{
	gic_cpu_init(&gic_data[0]);
	return 0;
}

static int gic_irq_domain_alloc(struct irq_domain *domain, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	int i, ret;
	irq_hw_number_t hwirq;
	unsigned int type = IRQ_TYPE_NONE;
	struct irq_fwspec *fwspec = arg;

	ret = gic_irq_domain_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; i++) {
		ret = gic_irq_domain_map(domain, virq + i, hwirq + i);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct irq_domain_ops gic_irq_domain_hierarchy_ops = {
	.translate = gic_irq_domain_translate,
	.alloc = gic_irq_domain_alloc,
	.free = irq_domain_free_irqs_top,
};

static const struct irq_domain_ops gic_irq_domain_ops = {
	.map = gic_irq_domain_map,
	.unmap = gic_irq_domain_unmap,
};

static void gic_init_chip(struct gic_chip_data *gic, struct device *dev,
			  const char *name, bool use_eoimode1)
{
	/* Initialize irq_chip */
	gic->chip = gic_chip;
	gic->chip.name = name;
	gic->chip.parent_device = dev;

	if (use_eoimode1) {
		gic->chip.irq_mask = gic_eoimode1_mask_irq;
		gic->chip.irq_eoi = gic_eoimode1_eoi_irq;
		gic->chip.irq_set_vcpu_affinity = gic_irq_set_vcpu_affinity;
	}

#ifdef CONFIG_SMP
	if (gic == &gic_data[0])
		gic->chip.irq_set_affinity = gic_set_affinity;
#endif
}

static int gic_init_bases(struct gic_chip_data *gic, int irq_start,
			  struct fwnode_handle *handle)
{
	irq_hw_number_t hwirq_base;
	int gic_irqs, irq_base, ret;

	if (IS_ENABLED(CONFIG_GIC_NON_BANKED) && gic->percpu_offset) {
		/* Frankein-GIC without banked registers... */
		unsigned int cpu;

		gic->dist_base.percpu_base = alloc_percpu(void __iomem *);
		gic->cpu_base.percpu_base = alloc_percpu(void __iomem *);
		if (WARN_ON(!gic->dist_base.percpu_base ||
			    !gic->cpu_base.percpu_base)) {
			ret = -ENOMEM;
			goto error;
		}

		for_each_possible_cpu(cpu) {
			u32 mpidr = cpu_logical_map(cpu);
			u32 core_id = MPIDR_AFFINITY_LEVEL(mpidr, 0);
			unsigned long offset = gic->percpu_offset * core_id;
			*per_cpu_ptr(gic->dist_base.percpu_base, cpu) =
				gic->raw_dist_base + offset;
			*per_cpu_ptr(gic->cpu_base.percpu_base, cpu) =
				gic->raw_cpu_base + offset;
		}

		gic_set_base_accessor(gic, gic_get_percpu_base);
	} else {
		/* Normal, sane GIC... */
		WARN(gic->percpu_offset,
		     "GIC_NON_BANKED not enabled, ignoring %08x offset!",
		     gic->percpu_offset);
		gic->dist_base.common_base = gic->raw_dist_base;
		gic->cpu_base.common_base = gic->raw_cpu_base;
		gic_set_base_accessor(gic, gic_get_common_base);
	}

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = readl_relaxed(gic_data_dist_base(gic) + GIC_DIST_CTR) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic->gic_irqs = gic_irqs;

	if (handle) {		/* DT/ACPI */
		gic->domain = irq_domain_create_linear(handle, gic_irqs,
						       &gic_irq_domain_hierarchy_ops,
						       gic);
	} else {		/* Legacy support */
		/*
		 * For primary GICs, skip over SGIs.
		 * For secondary GICs, skip over PPIs, too.
		 */
		if (gic == &gic_data[0] && (irq_start & 31) > 0) {
			hwirq_base = 16;
			if (irq_start != -1)
				irq_start = (irq_start & ~31) + 16;
		} else {
			hwirq_base = 32;
		}

		gic_irqs -= hwirq_base; /* calculate # of irqs to allocate */

		irq_base = irq_alloc_descs(irq_start, 16, gic_irqs,
					   this_cpu_numa_node_id());
		if (irq_base < 0) {
			WARN(1, "Cannot allocate irq_descs @ IRQ%d, assuming pre-allocated\n",
			     irq_start);
			irq_base = irq_start;
		}

		gic->domain = irq_domain_add_legacy(NULL, gic_irqs, irq_base,
					hwirq_base, &gic_irq_domain_ops, gic);
	}

	if (WARN_ON(!gic->domain)) {
		ret = -ENODEV;
		goto error;
	}

	gic_dist_init(gic);
	ret = gic_cpu_init(gic);
	if (ret)
		goto error;

	ret = gic_pm_init(gic);
	if (ret)
		goto error;

	return 0;

error:
	if (IS_ENABLED(CONFIG_GIC_NON_BANKED) && gic->percpu_offset) {
		free_percpu(gic->dist_base.percpu_base);
		free_percpu(gic->cpu_base.percpu_base);
	}

	return ret;
}

static int __init __gic_init_bases(struct gic_chip_data *gic,
				   int irq_start,
				   struct fwnode_handle *handle)
{
	char *name;
	int i, ret;

	if (WARN_ON(!gic || gic->domain))
		return -EINVAL;

	if (gic == &gic_data[0]) {
		/*
		 * Initialize the CPU interface map to all CPUs.
		 * It will be refined as each CPU probes its ID.
		 * This is only necessary for the primary GIC.
		 */
		for (i = 0; i < NR_GIC_CPU_IF; i++)
			gic_cpu_map[i] = 0xff;
#ifdef CONFIG_SMP
		set_smp_cross_call(gic_raise_softirq);
#endif
		cpuhp_setup_state_nocalls(CPUHP_AP_IRQ_GIC_STARTING,
					  "irqchip/arm/gic:starting",
					  gic_starting_cpu, NULL);
		set_handle_irq(gic_handle_irq);
		if (static_branch_likely(&supports_deactivate_key))
			pr_info("GIC: Using split EOI/Deactivate mode\n");
	}

	if (static_branch_likely(&supports_deactivate_key) && gic == &gic_data[0]) {
		name = kasprintf(GFP_KERNEL, "GICv2");
		gic_init_chip(gic, NULL, name, true);
	} else {
		name = kasprintf(GFP_KERNEL, "GIC-%d", (int)(gic-&gic_data[0]));
		gic_init_chip(gic, NULL, name, false);
	}

	ret = gic_init_bases(gic, irq_start, handle);
	if (ret)
		kfree(name);

	return ret;
}

void __init gic_init(unsigned int gic_nr, int irq_start,
		     void __iomem *dist_base, void __iomem *cpu_base)
{
	struct gic_chip_data *gic;

	if (WARN_ON(gic_nr >= CONFIG_ARM_GIC_MAX_NR))
		return;

	/*
	 * Non-DT/ACPI systems won't run a hypervisor, so let's not
	 * bother with these...
	 */
	static_branch_disable(&supports_deactivate_key);

	gic = &gic_data[gic_nr];
	gic->raw_dist_base = dist_base;
	gic->raw_cpu_base = cpu_base;

	__gic_init_bases(gic, irq_start, NULL);
}

static void gic_teardown(struct gic_chip_data *gic)
{
	if (WARN_ON(!gic))
		return;

	if (gic->raw_dist_base)
		iounmap(gic->raw_dist_base);
	if (gic->raw_cpu_base)
		iounmap(gic->raw_cpu_base);
}

#ifdef CONFIG_OF
static int gic_cnt __initdata;
static bool gicv2_force_probe;

static int __init gicv2_force_probe_cfg(char *buf)
{
	return strtobool(buf, &gicv2_force_probe);
}
early_param("irqchip.gicv2_force_probe", gicv2_force_probe_cfg);

static bool gic_check_eoimode(struct device_node *node, void __iomem **base)
{
	struct resource cpuif_res;

	of_address_to_resource(node, 1, &cpuif_res);

	if (!is_hyp_mode_available())
		return false;
	if (resource_size(&cpuif_res) < SZ_8K) {
		void __iomem *alt;
		/*
		 * Check for a stupid firmware that only exposes the
		 * first page of a GICv2.
		 */
		if (!gic_check_gicv2(*base))
			return false;

		if (!gicv2_force_probe) {
			pr_warn("GIC: GICv2 detected, but range too small and irqchip.gicv2_force_probe not set\n");
			return false;
		}

		alt = ioremap(cpuif_res.start, SZ_8K);
		if (!alt)
			return false;
		if (!gic_check_gicv2(alt + SZ_4K)) {
			/*
			 * The first page was that of a GICv2, and
			 * the second was *something*. Let's trust it
			 * to be a GICv2, and update the mapping.
			 */
			pr_warn("GIC: GICv2 at %pa, but range is too small (broken DT?), assuming 8kB\n",
				&cpuif_res.start);
			iounmap(*base);
			*base = alt;
			return true;
		}

		/*
		 * We detected *two* initial GICv2 pages in a
		 * row. Could be a GICv2 aliased over two 64kB
		 * pages. Update the resource, map the iospace, and
		 * pray.
		 */
		iounmap(alt);
		alt = ioremap(cpuif_res.start, SZ_128K);
		if (!alt)
			return false;
		pr_warn("GIC: Aliased GICv2 at %pa, trying to find the canonical range over 128kB\n",
			&cpuif_res.start);
		cpuif_res.end = cpuif_res.start + SZ_128K -1;
		iounmap(*base);
		*base = alt;
	}
	if (resource_size(&cpuif_res) == SZ_128K) {
		/*
		 * Verify that we have the first 4kB of a GICv2
		 * aliased over the first 64kB by checking the
		 * GICC_IIDR register on both ends.
		 */
		if (!gic_check_gicv2(*base) ||
		    !gic_check_gicv2(*base + 0xf000))
			return false;

		/*
		 * Move the base up by 60kB, so that we have a 8kB
		 * contiguous region, which allows us to use GICC_DIR
		 * at its normal offset. Please pass me that bucket.
		 */
		*base += 0xf000;
		cpuif_res.start += 0xf000;
		pr_warn("GIC: Adjusting CPU interface base to %pa\n",
			&cpuif_res.start);
	}

	return true;
}

static int gic_of_setup(struct gic_chip_data *gic, struct device_node *node)
{
	if (!gic || !node)
		return -EINVAL;

	gic->raw_dist_base = of_iomap(node, 0);
	if (WARN(!gic->raw_dist_base, "unable to map gic dist registers\n"))
		goto error;

	gic->raw_cpu_base = of_iomap(node, 1);
	if (WARN(!gic->raw_cpu_base, "unable to map gic cpu registers\n"))
		goto error;

	if (of_property_read_u32(node, "cpu-offset", &gic->percpu_offset))
		gic->percpu_offset = 0;

	return 0;

error:
	gic_teardown(gic);

	return -ENOMEM;
}

int gic_of_init_child(struct device *dev, struct gic_chip_data **gic, int irq)
{
	int ret;

	if (!dev || !dev->of_node || !gic || !irq)
		return -EINVAL;

	/* TODO */
	//*gic = devm_kzalloc(dev, sizeof(**gic), GFP_KERNEL);
	if (!*gic)
		return -ENOMEM;

	gic_init_chip(*gic, dev, dev->of_node->name, false);

	ret = gic_of_setup(*gic, dev->of_node);
	if (ret)
		return ret;

	ret = gic_init_bases(*gic, -1, &dev->of_node->fwnode);
	if (ret) {
		gic_teardown(*gic);
		return ret;
	}

	irq_set_chained_handler_and_data(irq, gic_handle_cascade_irq, *gic);

	return 0;
}

static void __init gic_of_setup_kvm_info(struct device_node *node)
{
	int ret;
	struct resource *vctrl_res = &gic_v2_kvm_info.vctrl;
	struct resource *vcpu_res = &gic_v2_kvm_info.vcpu;

	gic_v2_kvm_info.type = GIC_V2;

	gic_v2_kvm_info.maint_irq = irq_of_parse_and_map(node, 0);
	if (!gic_v2_kvm_info.maint_irq)
		return;

	ret = of_address_to_resource(node, 2, vctrl_res);
	if (ret)
		return;

	ret = of_address_to_resource(node, 3, vcpu_res);
	if (ret)
		return;

	if (static_branch_likely(&supports_deactivate_key))
		gic_set_kvm_info(&gic_v2_kvm_info);
}

int __init
gic_of_init(struct device_node *node, struct device_node *parent)
{
	struct gic_chip_data *gic;
	int irq, ret;

	if (WARN_ON(!node))
		return -ENODEV;

	if (WARN_ON(gic_cnt >= CONFIG_ARM_GIC_MAX_NR))
		return -EINVAL;

	gic = &gic_data[gic_cnt];

	ret = gic_of_setup(gic, node);
	if (ret)
		return ret;

	/*
	 * Disable split EOI/Deactivate if either HYP is not available
	 * or the CPU interface is too small.
	 */
	if (gic_cnt == 0 && !gic_check_eoimode(node, &gic->raw_cpu_base))
		static_branch_disable(&supports_deactivate_key);

	ret = __gic_init_bases(gic, -1, &node->fwnode);
	if (ret) {
		gic_teardown(gic);
		return ret;
	}

	if (!gic_cnt) {
		gic_init_physaddr(node);
		gic_of_setup_kvm_info(node);
	}

	if (parent) {
		irq = irq_of_parse_and_map(node, 0);
		gic_cascade_irq(gic_cnt, irq);
	}

	if (IS_ENABLED(CONFIG_ARM_GIC_V2M))
		gicv2m_init(&node->fwnode, gic_data[gic_cnt].domain);

	gic_cnt++;
	return 0;
}

IRQCHIP_DECLARE(gic_400, "arm,gic-400", gic_of_init);
IRQCHIP_DECLARE(arm11mp_gic, "arm,arm11mp-gic", gic_of_init);
IRQCHIP_DECLARE(arm1176jzf_dc_gic, "arm,arm1176jzf-devchip-gic", gic_of_init);
IRQCHIP_DECLARE(cortex_a15_gic, "arm,cortex-a15-gic", gic_of_init);
IRQCHIP_DECLARE(cortex_a9_gic, "arm,cortex-a9-gic", gic_of_init);
IRQCHIP_DECLARE(cortex_a7_gic, "arm,cortex-a7-gic", gic_of_init);
IRQCHIP_DECLARE(msm_8660_qgic, "qcom,msm-8660-qgic", gic_of_init);
IRQCHIP_DECLARE(msm_qgic2, "qcom,msm-qgic2", gic_of_init);
IRQCHIP_DECLARE(pl390, "arm,pl390", gic_of_init);

#else

int gic_of_init_child(struct device *dev, struct gic_chip_data **gic, int irq)
{
	return -ENOTSUPP;
}

#endif
