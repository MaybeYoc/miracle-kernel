/*
 * Based on arch/arm/kernel/asm-offsets.c
 *
 * Copyright (C) 1995-2003 Russell King
 *               2001-2002 Keith Owens
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
#include <linux/kbuild.h>
#include <linux/stddef.h>
#include <linux/mm_types.h>
#include <linux/dma-direction.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/arm-smccc.h>

struct test_struct {
	int a;
	int stack_canary;
};

int main(void)
{
	DEFINE(TSK_STACK,		offsetof(struct task_struct, stack));
#ifdef CONFIG_STACKPROTECTOR
	DEFINE(TSK_STACK_CANARY,	offsetof(struct task_struct, stack_canary));
#endif
	DEFINE(TSK_TI_FLAGS,		offsetof(struct task_struct, thread_info.flags));
	DEFINE(TSK_TI_ADDR_LIMIT,	offsetof(struct task_struct, thread_info.addr_limit));
	DEFINE(THREAD_CPU_CONTEXT,	offsetof(struct task_struct, thread.cpu_context));
	BLANK();
	DEFINE(CPU_BOOT_STACK,	offsetof(struct secondary_data, stack));
	DEFINE(CPU_BOOT_TASK,		offsetof(struct secondary_data, task));
	BLANK();
	DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, context.id.counter));
	BLANK();
	DEFINE(VMA_VM_MM,		offsetof(struct vm_area_struct, vm_mm));
	BLANK();
	DEFINE(DMA_TO_DEVICE,		DMA_TO_DEVICE);
	DEFINE(DMA_FROM_DEVICE,	DMA_FROM_DEVICE);
	BLANK();
	DEFINE(S_X0,			offsetof(struct pt_regs, regs[0]));
	DEFINE(S_X1,			offsetof(struct pt_regs, regs[1]));
	DEFINE(S_X2,			offsetof(struct pt_regs, regs[2]));
	DEFINE(S_X3,			offsetof(struct pt_regs, regs[3]));
	DEFINE(S_X4,			offsetof(struct pt_regs, regs[4]));
	DEFINE(S_X5,			offsetof(struct pt_regs, regs[5]));
	DEFINE(S_X6,			offsetof(struct pt_regs, regs[6]));
	DEFINE(S_X7,			offsetof(struct pt_regs, regs[7]));
	DEFINE(S_X8,			offsetof(struct pt_regs, regs[8]));
	DEFINE(S_X10,			offsetof(struct pt_regs, regs[10]));
	DEFINE(S_X12,			offsetof(struct pt_regs, regs[12]));
	DEFINE(S_X14,			offsetof(struct pt_regs, regs[14]));
	DEFINE(S_X16,			offsetof(struct pt_regs, regs[16]));
	DEFINE(S_X18,			offsetof(struct pt_regs, regs[18]));
	DEFINE(S_X20,			offsetof(struct pt_regs, regs[20]));
	DEFINE(S_X22,			offsetof(struct pt_regs, regs[22]));
	DEFINE(S_X24,			offsetof(struct pt_regs, regs[24]));
	DEFINE(S_X26,			offsetof(struct pt_regs, regs[26]));
	DEFINE(S_X28,			offsetof(struct pt_regs, regs[28]));
	DEFINE(S_LR,			offsetof(struct pt_regs, regs[30]));
	DEFINE(S_SP,			offsetof(struct pt_regs, sp));
	DEFINE(S_PSTATE,		offsetof(struct pt_regs, pstate));
	DEFINE(S_PC,			offsetof(struct pt_regs, pc));
	DEFINE(S_ORIG_X0,		offsetof(struct pt_regs, orig_x0));
	DEFINE(S_SYSCALLNO,		offsetof(struct pt_regs, syscallno));
	DEFINE(S_ORIG_ADDR_LIMIT,	offsetof(struct pt_regs, orig_addr_limit));
	DEFINE(S_STACKFRAME,		offsetof(struct pt_regs, stackframe));
	DEFINE(S_FRAME_SIZE,		sizeof(struct pt_regs));
	BLANK();
	DEFINE(ARM_SMCCC_RES_X0_OFFS,		offsetof(struct arm_smccc_res, a0));
	DEFINE(ARM_SMCCC_RES_X2_OFFS,		offsetof(struct arm_smccc_res, a2));
	DEFINE(ARM_SMCCC_QUIRK_ID_OFFS,	offsetof(struct arm_smccc_quirk, id));
	DEFINE(ARM_SMCCC_QUIRK_STATE_OFFS,	offsetof(struct arm_smccc_quirk, state));

	return 0;
}
