#ifndef __LINUX_MEMBLOCK_H_
#define __LINUX_MEMBLOCK_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/numa.h>

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;

/*
 * highest page
 */
extern unsigned long max_pfn;
/*
 * highest possible page
 */
extern unsigned long long max_possible_pfn;

/*
 * Logical memory blocks.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

enum memblock_flags {
	MEMBLOCK_NONE = 0x0, /* No special request */
	MEMBLOCK_HOTPLUG = 0x1, /* hotpluggable region */
	MEMBLOCK_MIRROR = 0x2, /* mirrored region */
	MEMBLOCK_NOMAP = 0x4, /* don't add to kernel direct mapping */
};

/*
 * struct memblock_region - represents a memory region
 * @base: physical address of the region
 * @size: size of the region
 * @flags: memory region attributes
 * @nid: NUMA node id
 */
struct memblock_region {
	phys_addr_t base;
	phys_addr_t size;
	enum memblock_flags flags;
	int nid;
};

struct memblock_type {
	char *name;
	unsigned long cnt;
	unsigned long max;
	phys_addr_t total_size;
	struct memblock_region *regions;
};

struct memblock {
	bool bottom_up; /* is bottom up direction? */
	phys_addr_t current_limit;
	struct memblock_type memory;
	struct memblock_type reserved;
};

extern struct memblock memblock;
extern int memblock_debug;

#define memblock_dbg(fmt, ...)                                                 \
	if (memblock_debug)                                                    \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#ifdef CONFIG_ARCH_DISCARD_MEMBLOCK
#define __init_memblock __meminit
#define __initdata_memblock __meminitdata
#error "Error: No Function Implemented memblock_discard"
void memblock_discard(void);
#else
#define __init_memblock
#define __initdata_memblock
#endif

#define MEMBLOCK_ALLOC_ANYWHERE PHYS_ADDR_MAX
#define MEMBLOCK_ALLOC_ACCESSIBLE 0

#ifndef ARCH_LOW_ADDRESS_LIMIT
#define ARCH_LOW_ADDRESS_LIMIT 0xffffffffUL
#endif

/* We are using top down, so it is safe to use 0 here */
#define MEMBLOCK_LOW_LIMIT 0

/*
 * pfn conversion functions
 *
 * While the memory MEMBLOCKs should always be page aligned, the reserved
 * MEMBLOCKs may not be. This accessor attempt to provide a very clear
 * idea of what they return for such non aligned MEMBLOCKs.
 */

/**
 * memblock_region_memory_base_pfn - get the lowest pfn of the memory region
 * @reg: memblock_region structure
 *
 * Return: the lowest pfn intersecting with the memory region
 */
static inline unsigned long
memblock_region_memory_base_pfn(const struct memblock_region *reg)
{
	return PFN_UP(reg->base);
}

/**
 * memblock_region_memory_end_pfn - get the end pfn of the memory region
 * @reg: memblock_region structure
 *
 * Return: the end_pfn of the reserved region
 */
static inline unsigned long
memblock_region_memory_end_pfn(const struct memblock_region *reg)
{
	return PFN_DOWN(reg->base + reg->size);
}

/**
 * memblock_region_reserved_base_pfn - get the lowest pfn of the reserved region
 * @reg: memblock_region structure
 *
 * Return: the lowest pfn intersecting with the reserved region
 */
static inline unsigned long
memblock_region_reserved_base_pfn(const struct memblock_region *reg)
{
	return PFN_DOWN(reg->base);
}

/**
 * memblock_region_reserved_end_pfn - get the end pfn of the reserved region
 * @reg: memblock_region structure
 *
 * Return: the end_pfn of the reserved region
 */
static inline unsigned long
memblock_region_reserved_end_pfn(const struct memblock_region *reg)
{
	return PFN_UP(reg->base + reg->size);
}

#define for_each_memblock_type(i, memblock_type, rgn)                          \
	for (i = 0, rgn = &memblock_type->regions[0]; i < memblock_type->cnt;  \
	     i++, rgn = &memblock_type->regions[i])

#define for_each_memblock(memblock_type, region)                               \
	for (region = memblock.memblock_type.regions;                          \
	     region <                                                          \
	     (memblock.memblock_type.regions + memblock.memblock_type.cnt);    \
	     region++)

/**
 * for_each_mem_range - iterate through memblock areas from type_a and not
 * included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define for_each_mem_range(i, type_a, type_b, nid, flags, p_start, p_end,      \
			   p_nid)                                              \
	for (i = 0, __next_mem_range(&i, nid, flags, type_a, type_b, p_start,  \
				     p_end, p_nid);                            \
	     i != (u64)ULLONG_MAX; __next_mem_range(                           \
		     &i, nid, flags, type_a, type_b, p_start, p_end, p_nid))

/**
 * for_each_free_mem_range - iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock.  Available as
 * soon as memblock is initialized.
 */
#define for_each_free_mem_range(i, nid, flags, p_start, p_end, p_nid)          \
	for_each_mem_range (i, &memblock.memory, &memblock.reserved, nid,      \
			    flags, p_start, p_end, p_nid)

/**
 * for_each_mem_range_rev - reverse iterate through memblock areas from
 * type_a and not included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define for_each_mem_range_rev(i, type_a, type_b, nid, flags, p_start, p_end,  \
			       p_nid)                                          \
	for (i = (u64)ULLONG_MAX,                                              \
	    __next_mem_range_rev(&i, nid, flags, type_a, type_b, p_start,      \
				 p_end, p_nid);                                \
	     i != (u64)ULLONG_MAX; __next_mem_range_rev(                       \
		     &i, nid, flags, type_a, type_b, p_start, p_end, p_nid))

/**
 * for_each_free_mem_range_reverse - rev-iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock in reverse
 * order.  Available as soon as memblock is initialized.
 */
#define for_each_free_mem_range_reverse(i, nid, flags, p_start, p_end, p_nid)  \
	for_each_mem_range_rev (i, &memblock.memory, &memblock.reserved, nid,  \
				flags, p_start, p_end, p_nid)

/**
 * for_each_reserved_mem_region - iterate over all reserved memblock areas
 * @i: u64 used as loop variable
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 *
 * Walks over reserved areas of memblock. Available as soon as memblock
 * is initialized.
 */
#define for_each_reserved_mem_region(i, p_start, p_end)                        \
	for (i = 0UL, __next_reserved_mem_region(&i, p_start, p_end);          \
	     i != (u64)ULLONG_MAX;                                             \
	     __next_reserved_mem_region(&i, p_start, p_end))

int memblock_search_pfn_nid(unsigned long pfn, unsigned long *start_pfn,
			    unsigned long *end_pfn);
void __next_mem_pfn_range(int *idx, int nid, unsigned long *out_start_pfn,
			  unsigned long *out_end_pfn, int *out_nid);

/**
 * for_each_mem_pfn_range - early memory pfn range iterator
 * @i: an integer used as loop variable
 * @nid: node selector, %MAX_NUMNODES for all nodes
 * @p_start: ptr to ulong for start pfn of the range, can be %NULL
 * @p_end: ptr to ulong for end pfn of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over configured memory ranges.
 */
#define for_each_mem_pfn_range(i, nid, p_start, p_end, p_nid)                  \
	for (i = -1, __next_mem_pfn_range(&i, nid, p_start, p_end, p_nid);     \
	     i >= 0; __next_mem_pfn_range(&i, nid, p_start, p_end, p_nid))

void __next_mem_range(u64 *idx, int nid, enum memblock_flags flags,
		      struct memblock_type *type_a,
		      struct memblock_type *type_b, phys_addr_t *out_start,
		      phys_addr_t *out_end, int *out_nid);

void __next_mem_range_rev(u64 *idx, int nid, enum memblock_flags flags,
			  struct memblock_type *type_a,
			  struct memblock_type *type_b, phys_addr_t *out_start,
			  phys_addr_t *out_end, int *out_nid);

bool memblock_overlaps_region(struct memblock_type *type, phys_addr_t base,
			      phys_addr_t size);

phys_addr_t memblock_find_in_range_node(phys_addr_t size, phys_addr_t align,
					phys_addr_t start, phys_addr_t end,
					int nid, enum memblock_flags flags);
phys_addr_t memblock_find_in_range(phys_addr_t start, phys_addr_t end,
				   phys_addr_t size, phys_addr_t align);

/* Low level functions */
int memblock_add_range(struct memblock_type *type, phys_addr_t base,
		       phys_addr_t size, int nid, enum memblock_flags flags);
int memblock_add_node(phys_addr_t base, phys_addr_t size, int nid);
int memblock_add(phys_addr_t base, phys_addr_t size);

int memblock_reserve(phys_addr_t base, phys_addr_t size);

int memblock_remove(phys_addr_t base, phys_addr_t size);

int memblock_free(phys_addr_t base, phys_addr_t size);

int memblock_mark_hotplug(phys_addr_t base, phys_addr_t size);
int memblock_clear_hotplug(phys_addr_t base, phys_addr_t size);

int memblock_mark_mirror(phys_addr_t base, phys_addr_t size);

int memblock_mark_nomap(phys_addr_t base, phys_addr_t size);
int memblock_clear_nomap(phys_addr_t base, phys_addr_t size);

phys_addr_t __init memblock_alloc_range(phys_addr_t size, phys_addr_t align,
					phys_addr_t start, phys_addr_t end,
					enum memblock_flags flags);
phys_addr_t memblock_alloc_base_nid(phys_addr_t size, phys_addr_t align,
				    phys_addr_t max_addr, int nid,
				    enum memblock_flags flags);
phys_addr_t memblock_phys_alloc_nid(phys_addr_t size, phys_addr_t align,
				    int nid);
phys_addr_t __memblock_alloc_base(phys_addr_t size, phys_addr_t align,
				  phys_addr_t max_addr);
phys_addr_t memblock_alloc_base(phys_addr_t size, phys_addr_t align,
				phys_addr_t max_addr);

phys_addr_t memblock_phys_alloc(phys_addr_t size, phys_addr_t align);
phys_addr_t memblock_phys_alloc_try_nid(phys_addr_t size, phys_addr_t align,
					int nid);

void *memblock_alloc_try_nid_nopanic(phys_addr_t size, phys_addr_t align,
				     phys_addr_t min_addr, phys_addr_t max_addr,
				     int nid);
void *memblock_alloc_try_nid(phys_addr_t size, phys_addr_t align,
			     phys_addr_t min_addr, phys_addr_t max_addr,
			     int nid);

static inline void *__init memblock_alloc(phys_addr_t size, phys_addr_t align)
{
	return memblock_alloc_try_nid(size, align, MEMBLOCK_LOW_LIMIT,
				      MEMBLOCK_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
}

static inline void *__init memblock_alloc_nopanic(phys_addr_t size,
						  phys_addr_t align)
{
	return memblock_alloc_try_nid_nopanic(size, align, MEMBLOCK_LOW_LIMIT,
					      MEMBLOCK_ALLOC_ACCESSIBLE,
					      NUMA_NO_NODE);
}

static inline void *__init memblock_alloc_low_nopanic(phys_addr_t size,
						      phys_addr_t align)
{
	return memblock_alloc_try_nid_nopanic(size, align, MEMBLOCK_LOW_LIMIT,
					      ARCH_LOW_ADDRESS_LIMIT,
					      NUMA_NO_NODE);
}

static inline void *__init memblock_alloc_from_nopanic(phys_addr_t size,
						       phys_addr_t align,
						       phys_addr_t min_addr)
{
	return memblock_alloc_try_nid_nopanic(
		size, align, min_addr, MEMBLOCK_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
}

phys_addr_t memblock_phys_mem_size(void);
phys_addr_t memblock_reserved_size(void);
phys_addr_t memblock_start_of_DRAM(void);
phys_addr_t memblock_end_of_DRAM(void);
void memblock_enforce_memory_limit(phys_addr_t memory_limit);
void memblock_cap_memory_range(phys_addr_t base, phys_addr_t size);
void memblock_mem_limit_remove_map(phys_addr_t limit);

bool memblock_is_reserved(phys_addr_t addr);
bool memblock_is_memory(phys_addr_t addr);
bool memblock_is_map_memory(phys_addr_t addr);
bool memblock_is_region_memory(phys_addr_t base, phys_addr_t size);
bool memblock_is_region_reserved(phys_addr_t base, phys_addr_t size);

/**
 * memblock_set_current_limit - Set the current allocation limit to allow
 *                         limiting allocations to what is currently
 *                         accessible during boot
 * @limit: New limit value (physical address)
 */
void memblock_set_current_limit(phys_addr_t limit);
phys_addr_t memblock_get_current_limit(void);

extern void __memblock_dump_all(void);

static inline void memblock_dump_all(void)
{
	if (memblock_debug)
		__memblock_dump_all();
}

void memblock_trim_memory(phys_addr_t align);

void memblock_allow_resize(void);

int memblock_set_node(phys_addr_t base, phys_addr_t size,
		      struct memblock_type *type, int nid);

static inline void memblock_set_region_node(struct memblock_region *r, int nid)
{
	r->nid = nid;
}

static inline int memblock_get_region_node(const struct memblock_region *r)
{
	return r->nid;
}

/*
 * Set the allocation direction to bottom-up or top-down.
 */
static inline void __init memblock_set_bottom_up(bool enable)
{
	memblock.bottom_up = enable;
}

/*
 * Check if the allocation direction is bottom-up or not.
 * if this is true, that said, memblock will allocate memory
 * in bottom-up direction.
 */
static inline bool memblock_bottom_up(void)
{
	return memblock.bottom_up;
}

static inline bool memblock_is_hotpluggable(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_HOTPLUG;
}

static inline bool memblock_is_mirror(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_MIRROR;
}

static inline bool memblock_is_nomap(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_NOMAP;
}

static inline void memblock_set_region_flags(struct memblock_region *r,
					     enum memblock_flags flags)
{
	r->flags |= flags;
}

static inline void memblock_clear_region_flags(struct memblock_region *r,
					       enum memblock_flags flags)
{
	r->flags &= ~flags;
}

unsigned long memblock_free_all(void);

void __memblock_free_late(phys_addr_t base, phys_addr_t size);

static inline void __init memblock_free_late(phys_addr_t base, phys_addr_t size)
{
	__memblock_free_late(base, size);
}

#ifdef CONFIG_MEMTEST
extern void early_memtest(phys_addr_t start, phys_addr_t end);
#else
static inline void early_memtest(phys_addr_t start, phys_addr_t end)
{
}
#endif

#endif /* __KERNEL__ */

#endif /* __LINUX_MEMBLOCK_H_ */
