/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_IRQ_H
#define __ASM_IRQ_H

#ifndef __ASSEMBLER__

#include <asm-generic/irq.h>

struct pt_regs;

static inline int nr_legacy_irqs(void)
{
	return 0;
}

void init_IRQ(void);

#endif /* !__ASSEMBLER__ */
#endif
