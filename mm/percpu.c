/*
 * mm/percpu.c - percpu memory allocator
 *
 * Copyright (C) 2009		SUSE Linux Products GmbH
 * Copyright (C) 2009		Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 *
 * This is percpu allocator which can handle both static and dynamic
 * areas.  Percpu areas are allocated in chunks.  Each chunk is
 * consisted of boot-time determined number of units and the first
 * chunk is used for static percpu variables in the kernel image
 * (special boot time alloc/init handling necessary as these areas
 * need to be brought up before allocation services are running).
 * Unit grows as necessary and all units grow or shrink in unison.
 * When a chunk is filled up, another chunk is allocated.
 *
 *  c0                           c1                         c2
 *  -------------------          -------------------        ------------
 * | u0 | u1 | u2 | u3 |        | u0 | u1 | u2 | u3 |      | u0 | u1 | u
 *  -------------------  ......  -------------------  ....  ------------
 *
 * Allocation is done in offset-size areas of single unit space.  Ie,
 * an area of 512 bytes at 6k in c1 occupies 512 bytes at 6k of c1:u0,
 * c1:u1, c1:u2 and c1:u3.  On UMA, units corresponds directly to
 * cpus.  On NUMA, the mapping can be non-linear and even sparse.
 * Percpu access can be done by configuring percpu base registers
 * according to cpu to unit mapping and pcpu_unit_size.
 *
 * There are usually many small percpu allocations many of them being
 * as small as 4 bytes.  The allocator organizes chunks into lists
 * according to free size and tries to allocate from the fullest one.
 * Each chunk keeps the maximum contiguous area size hint which is
 * guaranteed to be equal to or larger than the maximum contiguous
 * area in the chunk.  This helps the allocator not to iterate the
 * chunk maps unnecessarily.
 *
 * Allocation state in each chunk is kept using an array of integers
 * on chunk->map.  A positive value in the map represents a free
 * region and negative allocated.  Allocation inside a chunk is done
 * by scanning this map sequentially and serving the first matching
 * entry.  This is mostly copied from the percpu_modalloc() allocator.
 * Chunks can be determined from the address using the index field
 * in the page struct. The index field contains a pointer to the chunk.
 *
 * To use this allocator, arch code should do the followings.
 *
 * - define __addr_to_pcpu_ptr() and __pcpu_ptr_to_addr() to translate
 *   regular address to percpu pointer and back if they need to be
 *   different from the default
 *
 * - use pcpu_setup_first_chunk() during percpu area initialization to
 *   setup the first chunk containing the kernel static percpu area
 */
#include <linux/bitmap.h>
#include <linux/gfp.h>
#include <linux/percpu.h>
#include <linux/memblock.h>

#include <asm/sections.h>

#ifdef CONFIG_SMP

/*
 * Generic SMP percpu area setup.
 *
 * The embedding helper is used because its behavior closely resembles
 * the original non-dynamic generic percpu area setup.  This is
 * important because many archs have addressing restrictions and might
 * fail if the percpu area is located far away from the previous
 * location.  As an added bonus, in non-NUMA cases, embedding is
 * generally a good idea TLB-wise because percpu area can piggy back
 * on the physical linear memory mapping which uses large page
 * mappings on applicable archs.
 */
unsigned long __per_cpu_offset[NR_CPUS] __read_mostly;
unsigned long static_offsets __read_mostly;
unsigned long dy_offsets __read_mostly;
unsigned long dy_addr_start __read_mostly;

static unsigned long current_dy_addr;
static unsigned long current_dy_size;

/**
 * free_percpu - free percpu area
 * @ptr: pointer to area to free
 *
 * Free percpu area @ptr.
 *
 * CONTEXT:
 * Can be called from atomic context.
 */
void free_percpu(void __percpu *ptr)
{
	WARN(1, "percpu free address %p\n", ptr);
}

/**
 * pcpu_alloc - the percpu allocator
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 * @reserved: allocate from the reserved chunk if available
 * @gfp: allocation flags
 *
 * Allocate percpu area of @size bytes aligned at @align.  If @gfp doesn't
 * contain %GFP_KERNEL, the allocation is atomic. If @gfp has __GFP_NOWARN
 * then no warning will be triggered on invalid or failed allocation
 * requests.
 *
 * RETURNS:
 * Percpu pointer to the allocated area on success, NULL on failure.
 */
static void __percpu *pcpu_alloc(size_t size, size_t align, bool reserved,
				 gfp_t gfp)
{
	void __percpu *addr;

	/* TODO */
	size = SZ_16K;
	/*
	 * There is now a minimum allocation size of PCPU_MIN_ALLOC_SIZE,
	 * therefore alignment must be a minimum of that many bytes.
	 * An allocation may have internal fragmentation from rounding up
	 * of up to PCPU_MIN_ALLOC_SIZE - 1 bytes.
	 */
	if (unlikely(align < PCPU_MIN_ALLOC_SIZE))
		align = PCPU_MIN_ALLOC_SIZE;

	size = ALIGN(size, align);
	if (unlikely(!size || size > PCPU_MIN_UNIT_SIZE || align > PAGE_SIZE ||
		     !is_power_of_2(align))) {
		WARN(1, "illegal size (%zu) or align (%zu) for percpu allocation\n",
		     size, align);
		return NULL;
	}

	if (size > current_dy_size) {
		WARN(1, "current not enought percpu alloc current 0x%lx size 0x%lx\n",
					current_dy_size, size);
		return NULL;
	}

	addr = (void __percpu *)current_dy_addr;

	size = ALIGN(size, SMP_CACHE_BYTES);
	current_dy_addr = current_dy_addr + size;
	current_dy_size -= size;

	return addr;
}

/**
 * __alloc_percpu - allocate dynamic percpu area
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 *
 * Equivalent to __alloc_percpu_gfp(size, align, %GFP_KERNEL).
 */
void __percpu *__alloc_percpu(size_t size, size_t align)
{
	return pcpu_alloc(size, align, false, GFP_KERNEL);
}

void __init setup_per_cpu_areas(void)
{
	unsigned int cpu;
	unsigned long offset;
	void *virt;
	unsigned long re = 0;

	static_offsets = ALIGN(__per_cpu_end - __per_cpu_start, SMP_CACHE_BYTES);
	dy_offsets = ALIGN(PERCPU_DYNAMIC_EARLY_SLOTS * PERCPU_DYNAMIC_EARLY_SIZE, SMP_CACHE_BYTES);
	offset = memblock_phys_alloc((static_offsets + dy_offsets) * NR_CPUS, SMP_CACHE_BYTES);
	virt = phys_to_virt(offset);

	offset = (unsigned long)virt - (unsigned long)__per_cpu_load;
	dy_addr_start = (unsigned long)__per_cpu_load + static_offsets;
	current_dy_addr = dy_addr_start;
	current_dy_size = dy_offsets;

	for_each_possible_cpu(cpu)
		__per_cpu_offset[cpu] = offset + (static_offsets + dy_offsets) * cpu;

	pr_info("Embedded %zu pages/cpu s%zu r%zu d%zu u%zu\n",
		(static_offsets + dy_offsets) * NR_CPUS/PAGE_SIZE + 1, static_offsets, re,
		dy_offsets * NR_CPUS, static_offsets + dy_offsets);
}

#else

#endif
