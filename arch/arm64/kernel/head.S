/*
 * Low-level CPU initialisation
 * Based on arch/arm/kernel/head.S
 *
 * Copyright (C) 1994-2002 Russell King
 * Copyright (C) 2003-2012 ARM Ltd.
 * Authors:	Catalin Marinas <catalin.marinas@arm.com>
 *		Will Deacon <will.deacon@arm.com>
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

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/assembler.h>
#include <asm/boot.h>
#include <asm/asm-offsets.h>
#include <asm/cache.h>
#include <asm/cputype.h>
#include <asm/image.h>
#include <asm/kernel-pgtable.h>
#include <asm/memory.h>
#include <asm/pgtable-hwdef.h>
#include <asm/sysreg.h>
#include <asm/thread_info.h>

#define __PHYS_OFFSET	(KERNEL_START - TEXT_OFFSET)

#if (TEXT_OFFSET & 0xfff) != 0
#error TEXT_OFFSET must be at least 4KB aligned
#elif (PAGE_OFFSET & 0x1fffff) != 0
#error PAGE_OFFSET must be at least 2MB aligned
#elif TEXT_OFFSET > 0x1fffff
#error TEXT_OFFSET must be less than 2MB
#endif

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * The requirements are:
 *   MMU = off, D-cache = off, I-cache = on or off,
 *   x0 = physical address to the FDT blob.
 *
 * This code is mostly position independent so you call this at
 * __pa(PAGE_OFFSET + TEXT_OFFSET).
 *
 * Note that the callee-saved registers are used for storing variables
 * that are useful before the MMU is enabled. The allocations are described
 * in the entry routines.
 */
	__HEAD
_head:
	/*
	 * DO NOT MODIFY. Image header expected by Linux boot-loaders.
	 */
	b	stext
	.long	0
	le64sym	_kernel_offset_le		// Image load offset from start of RAM, little-endian
	le64sym	_kernel_size_le			// Effective size of kernel image, little-endian
	le64sym	_kernel_flags_le		// Informative flags, little-endian
	.quad	0				// reserved
	.quad	0				// reserved
	.quad	0				// reserved
	.ascii	ARM64_IMAGE_MAGIC		// Magic number
	.long	0				// reserved

	__INIT

	/*
	 * The following callee saved general purpose registers are used on the
	 * primary lowlevel boot path:
	 *
	 *  Register   Scope                      Purpose
	 *  x21        stext() .. start_kernel()  FDT pointer passed at boot in x0
	 *  x23        stext() .. start_kernel()  physical misalignment/KASLR offset
	 *  x28        __create_page_tables()     callee preserved temp register
	 *  x19/x20    __primary_switch()         callee preserved temp registers
	 */
ENTRY(stext)
	bl	preserve_boot_args

ENDPROC(stext)

/*
 * Preserve the arguments passed by the bootloader in x0 .. x3
 */
preserve_boot_args:
	mov	x21, x0				// x21=FDT

	adr_l	x0, boot_args			// record the contents of
	stp	x21, x1, [x0]			// x0 .. x3 at kernel entry
	stp	x2, x3, [x0, #16]

	dmb	sy				// needed before dc ivac with
						// MMU off

	mov	x1, #0x20			// 4 x 8 bytes
	//b	__inval_dcache_area		// tail call
ENDPROC(preserve_boot_args)
