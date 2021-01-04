/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_SMP_H
#define __ASM_SMP_H

#include <linux/const.h>

/* Values for secondary_data.status */
#define CPU_STUCK_REASON_SHIFT		(8)
#define CPU_BOOT_STATUS_MASK		((UL(1) << CPU_STUCK_REASON_SHIFT) - 1)

#define CPU_MMU_OFF			(-1)
#define CPU_BOOT_SUCCESS		(0)
/* The cpu invoked ops->cpu_die, synchronise it with cpu_kill */
#define CPU_KILL_ME			(1)
/* The cpu couldn't die gracefully and is looping in the kernel */
#define CPU_STUCK_IN_KERNEL		(2)
/* Fatal system error detected by secondary CPU, crash the system */
#define CPU_PANIC_KERNEL		(3)

#define CPU_STUCK_REASON_52_BIT_VA	(UL(1) << CPU_STUCK_REASON_SHIFT)
#define CPU_STUCK_REASON_NO_GRAN	(UL(2) << CPU_STUCK_REASON_SHIFT)

#ifndef __ASSEMBLY__

struct secondary_data {
	void *stack;
	struct task_struct *task;
	long status;
};

#endif /* ifndef __ASSEMBLY__ */

#endif /* ifndef __ASM_SMP_H */
