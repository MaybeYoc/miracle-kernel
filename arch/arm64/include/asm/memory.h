/*
 * Based on arch/arm/include/asm/memory.h
 *
 * Copyright (C) 2000-2002 Russell King
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
 *
 * Note: this file should not be included by non-asm/.h files
 */
#ifndef __ASM_MEMORY_H
#define __ASM_MEMORY_H

#include <linux/compiler.h>
#include <linux/const.h>
#include <linux/types.h>
#include <asm/bug.h>
#include <asm/page-def.h>
#include <asm/sizes.h>

/*
 * Size of the PCI I/O space. This must remain a power of two so that
 * IO_SPACE_LIMIT acts as a mask for the low bits of I/O addresses.
 */
#define PCI_IO_SIZE		SZ_16M

/*
 * VMEMMAP_SIZE - allows the whole linear region to be covered by
 *                a struct page array
 */
#define VMEMMAP_SIZE (UL(1) << (VA_BITS - PAGE_SHIFT - 1 + STRUCT_PAGE_MAX_SHIFT))

/*
 * PAGE_OFFSET - the virtual address of the start of the linear map (top
 *		 (VA_BITS - 1))
 * KIMAGE_VADDR - the virtual address of the start of the kernel image
 * VA_BITS - the maximum number of bits for virtual addresses.
 * VA_START - the first kernel virtual address.
 */
#define VA_BITS			(CONFIG_ARM64_VA_BITS)
#define VA_START		(UL(0xffffffffffffffff) - \
	(UL(1) << VA_BITS) + 1)
#define PAGE_OFFSET		(UL(0xffffffffffffffff) - \
	(UL(1) << (VA_BITS - 1)) + 1)
#define KIMAGE_VADDR		(VA_START)
#define VMEMMAP_START		(PAGE_OFFSET - VMEMMAP_SIZE)
#define PCI_IO_END		(VMEMMAP_START - SZ_2M)
#define PCI_IO_START		(PCI_IO_END - PCI_IO_SIZE)
#define FIXADDR_TOP		(PCI_IO_START - SZ_2M)

#define KERNEL_START      _text
#define KERNEL_END        _end

#ifdef CONFIG_ARM64_USER_VA_BITS_52
#define MAX_USER_VA_BITS	52
#else
#define MAX_USER_VA_BITS	VA_BITS
#endif

#define MIN_THREAD_SHIFT	(14)

/*
 * VMAP'd stacks are allocated at page granularity, so we must ensure that such
 * stacks are a multiple of page size.
 */
#if defined(CONFIG_VMAP_STACK) && (MIN_THREAD_SHIFT < PAGE_SHIFT)
#define THREAD_SHIFT		PAGE_SHIFT
#else
#define THREAD_SHIFT		MIN_THREAD_SHIFT
#endif

#if THREAD_SHIFT >= PAGE_SHIFT
#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)
#endif

#define THREAD_SIZE		(UL(1) << THREAD_SHIFT)

/*
 * By aligning VMAP'd stacks to 2 * THREAD_SIZE, we can detect overflow by
 * checking sp & (1 << THREAD_SHIFT), which we can do cheaply in the entry
 * assembly.
 */
#ifdef CONFIG_VMAP_STACK
#define THREAD_ALIGN		(2 * THREAD_SIZE)
#else
#define THREAD_ALIGN		THREAD_SIZE
#endif

#define IRQ_STACK_SIZE		THREAD_SIZE

#define OVERFLOW_STACK_SIZE	SZ_4K

/*
 * Alignment of kernel segments (e.g. .text, .data).
 */
/*
 *  4 KB granule:  16 level 3 entries, with contiguous bit
 * 16 KB granule:   4 level 3 entries, without contiguous bit
 * 64 KB granule:   1 level 3 entry
 */
#define SEGMENT_ALIGN			SZ_64K

/*
 * Memory types available.
 */
#define MT_DEVICE_nGnRnE	0
#define MT_DEVICE_nGnRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4
#define MT_NORMAL_WT		5

/*
 * Memory types for Stage-2 translation
 */
#define MT_S2_NORMAL		0xf
#define MT_S2_DEVICE_nGnRE	0x1

/*
 * Memory types for Stage-2 translation when ID_AA64MMFR2_EL1.FWB is 0001
 * Stage-2 enforces Normal-WB and Device-nGnRE
 */
#define MT_S2_FWB_NORMAL	6
#define MT_S2_FWB_DEVICE_nGnRE	1

#ifdef CONFIG_ARM64_4K_PAGES
#define IOREMAP_MAX_ORDER	(PUD_SHIFT)
#else
#define IOREMAP_MAX_ORDER	(PMD_SHIFT)
#endif

#ifndef __ASSEMBLY__

#include <linux/bitops.h>
#include <linux/mmdebug.h>

extern s64			memstart_addr;
/* PHYS_OFFSET - the physical address of the start of memory. */
#define PHYS_OFFSET		({ VM_BUG_ON(memstart_addr & 1); memstart_addr; })

/* the virtual base of the kernel image (minus TEXT_OFFSET) */
extern u64			kimage_vaddr;

/* the offset between the kernel virtual and physical mappings */
extern u64			kimage_voffset;

/* the actual size of a user virtual address */
extern u64			vabits_user;

/*
 * Allow all memory at the discovery stage. We will clip it later.
 */
#define MIN_MEMBLOCK_ADDR	0
#define MAX_MEMBLOCK_ADDR	U64_MAX

/*
 * PFNs are used to describe any physical page; this means
 * PFN 0 == physical address 0.
 *
 * This is the PFN of the first RAM page in the kernel
 * direct-mapped view.  We assume this is the first page
 * of RAM in the mem_map as well.
 */
#define PHYS_PFN_OFFSET	(PHYS_OFFSET >> PAGE_SHIFT)

/*
 * Physical vs virtual RAM address space conversion.  These are
 * private definitions which should NOT be used outside memory.h
 * files.  Use virt_to_phys/phys_to_virt/__pa/__va instead.
 */


/*
 * The linear kernel range starts in the middle of the virtual adddress
 * space. Testing the top bit for the start of the region is a
 * sufficient check.
 */
#define __is_lm_address(addr)	(!!((addr) & BIT(VA_BITS - 1)))

#define __lm_to_phys(addr)	(((addr) & ~PAGE_OFFSET) + PHYS_OFFSET)
#define __kimg_to_phys(addr)	((addr) - kimage_voffset)

#define __virt_to_phys_nodebug(x) ({					\
	phys_addr_t __x = (phys_addr_t)(x);				\
	__is_lm_address(__x) ? __lm_to_phys(__x) :			\
			       __kimg_to_phys(__x);			\
})

#define __pa_symbol_nodebug(x)	__kimg_to_phys((phys_addr_t)(x))

#define __virt_to_phys(x)	__virt_to_phys_nodebug(x)
#define __phys_addr_symbol(x)	__pa_symbol_nodebug(x)

#define __phys_to_virt(x)	((unsigned long)((x) - PHYS_OFFSET) | PAGE_OFFSET)
#define __phys_to_kimg(x)	((unsigned long)((x) + kimage_voffset))

/*
 * Drivers should NOT use these either.
 */
#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __pa_symbol(x)		__phys_addr_symbol(RELOC_HIDE((unsigned long)(x), 0))
#define __pa_nodebug(x)		__virt_to_phys_nodebug((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((phys_addr_t)(x)))
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

/*
 *  virt_to_page(k)	convert a _valid_ virtual address to struct page *
 *  virt_addr_valid(k)	indicates whether a virtual address is valid
 */
#define ARCH_PFN_OFFSET		((unsigned long)PHYS_PFN_OFFSET)

#endif

#include <asm-generic/memory_model.h>

#endif
