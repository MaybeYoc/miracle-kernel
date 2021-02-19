/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_IRQFLAGS_H
#define __ASM_GENERIC_IRQFLAGS_H

/* test hardware interrupt enable bit */
#ifndef arch_irqs_disabled
static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}
#endif

#endif /* __ASM_GENERIC_IRQFLAGS_H */
