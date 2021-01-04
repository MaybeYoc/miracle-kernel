/*
 * Based on arch/arm/kernel/process.c
 *
 * Original Copyright (C) 1995  Linus Torvalds
 * Copyright (C) 1996-2000 Russell King - Converted to ARM.
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
#include <linux/sched.h>
#include <asm-generic/percpu.h>
#include <stdarg.h>

/*
 * We store our current task in sp_el0, which is clobbered by userspace. Keep a
 * shadow copy so that we can restore this upon entry from userspace.
 *
 * This is *only* for exception entry from EL0, and is not valid until we
 * __switch_to() a user task.
 */
DEFINE_PER_CPU(struct task_struct *, __entry_task);