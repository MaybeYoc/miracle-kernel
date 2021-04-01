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
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/irqchip.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>

#include "irq-gic-common.h"

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

static struct gic_chip_data gic_data[CONFIG_ARM_GIC_MAX_NR] __read_mostly;

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
	u32 val, mask, bit;
	unsigned long flags;

	if (!force)
		cpu = cpumask_any_and(mask_val, cpu_online_mask);
	else
		cpu = cpumask_first(mask_val);

	if (cpu >= NR_GIC_CPU_IF || cpu >= nr_cpu_ids)
		return -EINVAL;

	gic_lock_irqsave(flags);
	mask = 0xff << shift;
	bit = gic_cpu_map[cpu] << shift;
	val = readl_relaxed(reg) & ~mask;
	writel_relaxed(val | bit, reg);
	gic_unlock_irqrestore(flags);

	irq_data_update_effective_affinity(d, cpumask_of(cpu));

	return IRQ_SET_MASK_OK_DONE;
}
#endif

int __init
gic_of_init(struct device_node *node, struct device_node *parent)
{
	printk("%s %d\n", __func__, __LINE__);
	printk("name %s %p\n", node->name, parent);
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
