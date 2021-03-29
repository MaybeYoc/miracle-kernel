// SPDX-License-Identifier: GPL-2.0
/*
 * Slab allocator functions that are independent of the allocator strategy
 *
 * (C) 2012 Christoph Lameter <cl@linux.com>
 */
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/poison.h>
#include <linux/compiler.h>
#include <linux/cache.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/page.h>

#include "slab.h"

enum slab_state slab_state;
LIST_HEAD(slab_caches);
struct kmem_cache *kmem_cache;

struct kmem_cache *
kmalloc_caches[NR_KMALLOC_TYPES][KMALLOC_SHIFT_HIGH + 1] __ro_after_init;

unsigned int kmem_cache_size(struct kmem_cache *s)
{
	return s->object_size;
}

static inline int kmem_cache_sanity_check(const char *name, unsigned int size)
{
	return 0;
}

/*
 * Conversion table for small slabs sizes / 8 to the index in the
 * kmalloc array. This is necessary for slabs < 192 since we have non power
 * of two cache sizes there. The size of larger slabs can be determined using
 * fls.
 */
static unsigned char size_index[24] __ro_after_init = {
	3,	/* 8 */
	4,	/* 16 */
	5,	/* 24 */
	5,	/* 32 */
	6,	/* 40 */
	6,	/* 48 */
	6,	/* 56 */
	6,	/* 64 */
	1,	/* 72 */
	1,	/* 80 */
	1,	/* 88 */
	1,	/* 96 */
	7,	/* 104 */
	7,	/* 112 */
	7,	/* 120 */
	7,	/* 128 */
	2,	/* 136 */
	2,	/* 144 */
	2,	/* 152 */
	2,	/* 160 */
	2,	/* 168 */
	2,	/* 176 */
	2,	/* 184 */
	2	/* 192 */
};

static inline unsigned int size_index_elem(unsigned int bytes)
{
	return (bytes - 1) / 8;
}

/*
 * kmalloc_info[] is to make slub_debug=,kmalloc-xx option work at boot time.
 * kmalloc_index() supports up to 2^26=64MB, so the final entry of the table is
 * kmalloc-67108864.
 */
const struct kmalloc_info_struct kmalloc_info[] __initconst = {
	{NULL,                      0},		{"kmalloc-96",             96},
	{"kmalloc-192",           192},		{"kmalloc-8",               8},
	{"kmalloc-16",             16},		{"kmalloc-32",             32},
	{"kmalloc-64",             64},		{"kmalloc-128",           128},
	{"kmalloc-256",           256},		{"kmalloc-512",           512},
	{"kmalloc-1k",           1024},		{"kmalloc-2k",           2048},
	{"kmalloc-4k",           4096},		{"kmalloc-8k",           8192},
	{"kmalloc-16k",         16384},		{"kmalloc-32k",         32768},
	{"kmalloc-64k",         65536},		{"kmalloc-128k",       131072},
	{"kmalloc-256k",       262144},		{"kmalloc-512k",       524288},
	{"kmalloc-1M",        1048576},		{"kmalloc-2M",        2097152},
	{"kmalloc-4M",        4194304},		{"kmalloc-8M",        8388608},
	{"kmalloc-16M",      16777216},		{"kmalloc-32M",      33554432},
	{"kmalloc-64M",      67108864}
};

/*
 * Patch up the size_index table if we have strange large alignment
 * requirements for the kmalloc array. This is only the case for
 * MIPS it seems. The standard arches will not generate any code here.
 *
 * Largest permitted alignment is 256 bytes due to the way we
 * handle the index determination for the smaller caches.
 *
 * Make sure that nothing crazy happens if someone starts tinkering
 * around with ARCH_KMALLOC_MINALIGN
 */
void __init setup_kmalloc_cache_index_table(void)
{
	unsigned int i;

	BUILD_BUG_ON(KMALLOC_MIN_SIZE > 256 ||
		(KMALLOC_MIN_SIZE & (KMALLOC_MIN_SIZE - 1)));

	for (i = 8; i < KMALLOC_MIN_SIZE; i += 8) {
		unsigned int elem = size_index_elem(i);

		if (elem >= ARRAY_SIZE(size_index))
			break;
		size_index[elem] = KMALLOC_SHIFT_LOW;
	}

	if (KMALLOC_MIN_SIZE >= 64) {
		/*
		 * The 96 byte size cache is not used if the alignment
		 * is 64 byte.
		 */
		for (i = 64 + 8; i <= 96; i += 8)
			size_index[size_index_elem(i)] = 7;

	}

	if (KMALLOC_MIN_SIZE >= 128) {
		/*
		 * The 192 byte sized cache is not used if the alignment
		 * is 128 byte. Redirect kmalloc to use the 256 byte cache
		 * instead.
		 */
		for (i = 128 + 8; i <= 192; i += 8)
			size_index[size_index_elem(i)] = 8;
	}
}

/*
 * Figure out what the alignment of the objects will be given a set of
 * flags, a user specified alignment and the size of the objects.
 */
static unsigned int calculate_alignment(slab_flags_t flags,
		unsigned int align, unsigned int size)
{
	/*
	 * If the user wants hardware cache aligned objects then follow that
	 * suggestion if the object is sufficiently large.
	 *
	 * The hardware cache alignment cannot override the specified
	 * alignment though. If that is greater then use it.
	 */
	if (flags & SLAB_HWCACHE_ALIGN) {
		unsigned int ralign;

		ralign = cache_line_size();
		while (size <= ralign / 2)
			ralign /= 2;
		align = max(align, ralign);
	}

	if (align < ARCH_SLAB_MINALIGN)
		align = ARCH_SLAB_MINALIGN;

	return ALIGN(align, sizeof(void *));
}

/* Create a cache during boot when no slab services are available yet */
void __init create_boot_cache(struct kmem_cache *s, const char *name,
		unsigned int size, slab_flags_t flags,
		unsigned int useroffset, unsigned int usersize)
{
	int err;

	s->name = name;
	s->size = s->object_size = size;
	s->align = calculate_alignment(flags, ARCH_KMALLOC_MINALIGN, size);
	s->useroffset = useroffset;
	s->usersize = usersize;

	slab_init_memcg_params(s);

	err = __kmem_cache_create(s, flags);

	if (err)
		panic("Creation of kmalloc slab %s size=%u failed. Reason %d\n",
					name, size, err);

	s->refcount = -1;	/* Exempt from merging for now */
}

void __kmem_cache_free_bulk(struct kmem_cache *s, size_t nr, void **p)
{
	size_t i;

	for (i = 0; i < nr; i++) {
		if (s)
			kmem_cache_free(s, p[i]);
		else
			kfree(p[i]);
	}
}

static const char *
kmalloc_cache_name(const char *prefix, unsigned int size)
{

	static const char units[3] = "\0kM";
	int idx = 0;

	while (size >= 1024 && (size % 1024 == 0)) {
		size /= 1024;
		idx++;
	}

	return kasprintf(GFP_KERNEL, "%s-%u%c", prefix, size, units[idx]);
}

static void __init
new_kmalloc_cache(int idx, int type, slab_flags_t flags)
{
	const char *name;

	if (type == KMALLOC_RECLAIM) {
		flags |= SLAB_RECLAIM_ACCOUNT;
		name = kmalloc_cache_name("kmalloc-rcl",
						kmalloc_info[idx].size);
		BUG_ON(!name);
	} else {
		name = kmalloc_info[idx].name;
	}

	kmalloc_caches[type][idx] = create_kmalloc_cache(name,
					kmalloc_info[idx].size, flags, 0,
					kmalloc_info[idx].size);
}

/*
 * Create the kmalloc array. Some of the regular kmalloc arrays
 * may already have been created because they were needed to
 * enable allocations for slab creation.
 */
void __init create_kmalloc_caches(slab_flags_t flags)
{
	int i, type;

	for (type = KMALLOC_NORMAL; type <= KMALLOC_RECLAIM; type++) {
		for (i = KMALLOC_SHIFT_LOW; i <= KMALLOC_SHIFT_HIGH; i++) {
			if (!kmalloc_caches[type][i])
				new_kmalloc_cache(i, type, flags);

			/*
			 * Caches that are not of the two-to-the-power-of size.
			 * These have to be created immediately after the
			 * earlier power of two caches
			 */
			if (KMALLOC_MIN_SIZE <= 32 && i == 6 &&
					!kmalloc_caches[type][1])
				new_kmalloc_cache(1, type, flags);
			if (KMALLOC_MIN_SIZE <= 64 && i == 7 &&
					!kmalloc_caches[type][2])
				new_kmalloc_cache(2, type, flags);
		}
	}

	/* Kmalloc array is now usable */
	slab_state = UP;

#ifdef CONFIG_ZONE_DMA
	for (i = 0; i <= KMALLOC_SHIFT_HIGH; i++) {
		struct kmem_cache *s = kmalloc_caches[KMALLOC_NORMAL][i];

		if (s) {
			unsigned int size = kmalloc_size(i);
			const char *n = kmalloc_cache_name("dma-kmalloc", size);

			BUG_ON(!n);
			kmalloc_caches[KMALLOC_DMA][i] = create_kmalloc_cache(
				n, size, SLAB_CACHE_DMA | flags, 0, 0);
		}
	}
#endif
	/* TODO */
	slab_state = FULL;
}

/*
 * To avoid unnecessary overhead, we pass through large allocation requests
 * directly to the page allocator. We use __GFP_COMP, because we will need to
 * know the allocation order to free the pages properly in kfree.
 */
void *kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	void *ret;
	struct page *page;

	flags |= __GFP_COMP;
	page = alloc_pages(flags, order);
	ret = page ? page_address(page) : NULL;

	return ret;
}

/*
 * Find the kmem_cache structure that serves a given size of
 * allocation
 */
struct kmem_cache *kmalloc_slab(size_t size, gfp_t flags)
{
	unsigned int index;

	if (size <= 192) {
		if (!size)
			return ZERO_SIZE_PTR;

		index = size_index[size_index_elem(size)];
	} else {
		if (WARN_ON_ONCE(size > KMALLOC_MAX_CACHE_SIZE))
			return NULL;
		index = fls(size - 1);
	}

	return kmalloc_caches[kmalloc_type(flags)][index];
}

struct kmem_cache *__init create_kmalloc_cache(const char *name,
		unsigned int size, slab_flags_t flags,
		unsigned int useroffset, unsigned int usersize)
{
	struct kmem_cache *s = kmem_cache_zalloc(kmem_cache, GFP_KERNEL);

	if (!s)
		panic("Out of memory when creating slab %s\n", name);

	create_boot_cache(s, name, size, flags, useroffset, usersize);
	list_add(&s->list, &slab_caches);
	memcg_link_cache(s);
	s->refcount = 1;
	return s;
}

bool slab_is_available(void)
{
	return slab_state >= UP;
}
