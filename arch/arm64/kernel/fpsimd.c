/*
 * FP/SIMD context switching and fault handling
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
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
#include <linux/bug.h>
#include <linux/kernel.h>

#include <asm/ptrace.h>

/*
 * Trapped FP/ASIMD access.
 */
asmlinkage void do_fpsimd_acc(unsigned int esr, struct pt_regs *regs)
{
	/* TODO: implement lazy context saving/restoring */
	WARN_ON(1);
}

/*
 * Trapped SVE access
 *
 * Storage is allocated for the full SVE state, the current FPSIMD
 * register contents are migrated across, and TIF_SVE is set so that
 * the SVE access trap will be disabled the next time this task
 * reaches ret_to_user.
 *
 * TIF_SVE should be clear on entry: otherwise, task_fpsimd_load()
 * would have disabled the SVE access trap for userspace during
 * ret_to_user, making an SVE access trap impossible in that case.
 */
asmlinkage void do_sve_acc(unsigned int esr, struct pt_regs *regs)
{
	/* TODO */
	WARN_ON(1);
}

/*
 * Raise a SIGFPE for the current process.
 */
asmlinkage void do_fpsimd_exc(unsigned int esr, struct pt_regs *regs)
{
	/* TODO */
	WARN_ON(1);
}
