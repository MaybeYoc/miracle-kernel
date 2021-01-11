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
#include <linux/dma-direction.h>
#include <asm/cpufeature.h>
#include <asm/smp.h>

struct test_struct {
	int a;
	int stack_canary;
};
struct mm_struct {
	int aa;
};

int main(void)
{
	DEFINE(TSK_STACK_CANARY,	offsetof(struct test_struct, stack_canary));
	DEFINE(ARM64_FTR_SYSVAL,	offsetof(struct arm64_ftr_reg, sys_val));
	DEFINE(DMA_TO_DEVICE,		DMA_TO_DEVICE);
	DEFINE(DMA_FROM_DEVICE,	DMA_FROM_DEVICE);
	DEFINE(CPU_BOOT_STACK,	offsetof(struct secondary_data, stack));
	DEFINE(CPU_BOOT_TASK,		offsetof(struct secondary_data, task));
	DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, aa)); /* TODO test */
	return 0;
}
