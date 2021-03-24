/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_PERCPU_H
#define __LINUX_PERCPU_H

#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/printk.h>
#include <linux/init.h>

#include <asm/percpu.h>

/* minimum unit size, also is the maximum supported allocation size */
#define PCPU_MIN_UNIT_SIZE		PFN_ALIGN(32 << 10)

/* minimum allocation size and shift in bytes */
#define PCPU_MIN_ALLOC_SHIFT		2
#define PCPU_MIN_ALLOC_SIZE		(1 << PCPU_MIN_ALLOC_SHIFT)

/* number of bits per page, used to trigger a scan if blocks are > PAGE_SIZE */
#define PCPU_BITS_PER_PAGE		(PAGE_SIZE >> PCPU_MIN_ALLOC_SHIFT)

/*
 * This determines the size of each metadata block.  There are several subtle
 * constraints around this constant.  The reserved region must be a multiple of
 * PCPU_BITMAP_BLOCK_SIZE.  Additionally, PCPU_BITMAP_BLOCK_SIZE must be a
 * multiple of PAGE_SIZE or PAGE_SIZE must be a multiple of
 * PCPU_BITMAP_BLOCK_SIZE to align with the populated page map. The unit_size
 * also has to be a multiple of PCPU_BITMAP_BLOCK_SIZE to ensure full blocks.
 */
#define PCPU_BITMAP_BLOCK_SIZE		PAGE_SIZE
#define PCPU_BITMAP_BLOCK_BITS		(PCPU_BITMAP_BLOCK_SIZE >>	\
					 PCPU_MIN_ALLOC_SHIFT)

/*
 * Percpu allocator can serve percpu allocations before slab is
 * initialized which allows slab to depend on the percpu allocator.
 * The following two parameters decide how much resource to
 * preallocate for this.  Keep PERCPU_DYNAMIC_RESERVE equal to or
 * larger than PERCPU_DYNAMIC_EARLY_SIZE.
 */
#define PERCPU_DYNAMIC_EARLY_SLOTS	128
#define PERCPU_DYNAMIC_EARLY_SIZE	(12 << 10)

/*
 * PERCPU_DYNAMIC_RESERVE indicates the amount of free area to piggy
 * back on the first chunk for dynamic percpu allocation if arch is
 * manually allocating and mapping it for faster access (as a part of
 * large page mapping for example).
 *
 * The following values give between one and two pages of free space
 * after typical minimal boot (2-way SMP, single disk and NIC) with
 * both defconfig and a distro config on x86_64 and 32.  More
 * intelligent way to determine this would be nice.
 */
#if BITS_PER_LONG > 32
#define PERCPU_DYNAMIC_RESERVE		(28 << 10)
#else
#define PERCPU_DYNAMIC_RESERVE		(20 << 10)
#endif

#endif /* __LINUX_PERCPU_H */
