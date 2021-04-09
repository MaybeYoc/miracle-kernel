/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MM_SLAB_H
#define MM_SLAB_H
/*
 * Internal slab definitions
 */
#include <linux/slub_def.h>

/*
 * State of the slab allocator.
 *
 * This is used to describe the states of the allocator during bootup.
 * Allocators use this to gradually bootstrap themselves. Most allocators
 * have the problem that the structures used for managing slab caches are
 * allocated from slab caches themselves.
 */
enum slab_state {
	DOWN,			/* No slab functionality yet */
	PARTIAL,		/* SLUB: kmem_cache_node available */
	PARTIAL_NODE,		/* SLAB: kmalloc size for node struct available */
	UP,			/* Slab caches usable but not all extras yet */
	FULL			/* Everything is working */
};

extern enum slab_state slab_state;

/* The slab cache that manages slab cache information */
extern struct kmem_cache *kmem_cache;

extern const struct kmalloc_info_struct {
	const char *name;
	unsigned int size;
} kmalloc_info[];

struct kmem_cache_node {
	spinlock_t list_lock;
	unsigned long nr_partial;
	struct list_head partial;
};

/* If !memcg, all caches are root. */
#define slab_root_caches	slab_caches
#define root_caches_node	list

#define for_each_memcg_cache(iter, root) \
	for ((void)(iter), (void)(root); 0; )

static inline bool is_root_cache(struct kmem_cache *s)
{
	return true;
}

static inline bool slab_equal_or_root(struct kmem_cache *s,
				      struct kmem_cache *p)
{
	return true;
}

static inline const char *cache_name(struct kmem_cache *s)
{
	return s->name;
}

static inline struct kmem_cache *
cache_from_memcg_idx(struct kmem_cache *s, int idx)
{
	return NULL;
}

static inline struct kmem_cache *memcg_root_cache(struct kmem_cache *s)
{
	return s;
}

static inline int memcg_charge_slab(struct page *page, gfp_t gfp, int order,
				    struct kmem_cache *s)
{
	return 0;
}

static inline void memcg_uncharge_slab(struct page *page, int order,
				       struct kmem_cache *s)
{
}

static inline void slab_init_memcg_params(struct kmem_cache *s)
{
}

static inline void memcg_link_cache(struct kmem_cache *s)
{
}

static inline struct kmem_cache_node *get_node(struct kmem_cache *s, int node)
{
	return s->node[node];
}

/* Kmalloc array related functions */
void setup_kmalloc_cache_index_table(void);
void create_kmalloc_caches(slab_flags_t);

/*
 * Iterator over all nodes. The body will be executed for each node that has
 * a kmem_cache_node structure allocated (which is true for all online nodes)
 */
#define for_each_kmem_cache_node(__s, __node, __n) \
	for (__node = 0; __node < nr_possible_nodes; __node++) \
		 if ((__n = get_node(__s, __node)))

static inline void slab_post_alloc_hook(struct kmem_cache *s, gfp_t flags,
					size_t size, void **p)
{
}

static inline struct kmem_cache *slab_pre_alloc_hook(struct kmem_cache *s,
						     gfp_t flags)
{
	return s;
}

static inline struct kmem_cache *cache_from_obj(struct kmem_cache *s, void *x)
{
	struct kmem_cache *cachep;
	struct page *page;

	/*
	 * When kmemcg is not being used, both assignments should return the
	 * same value. but we don't want to pay the assignment price in that
	 * case. If it is not compiled in, the compiler should be smart enough
	 * to not do even the assignment. In that case, slab_equal_or_root
	 * will also be a constant.
	 */
	if (!unlikely(s->flags & SLAB_CONSISTENCY_CHECKS))
		return s;

	page = virt_to_head_page(x);
	cachep = page->slab_cache;
	if (slab_equal_or_root(cachep, s))
		return cachep;

	pr_err("%s: Wrong slab cache. %s but object is from %s\n",
	       __func__, s->name, cachep->name);
	WARN_ON_ONCE(1);
	return s;
}

extern void create_boot_cache(struct kmem_cache *, const char *name,
			unsigned int size, slab_flags_t flags,
			unsigned int useroffset, unsigned int usersize);

int slab_unmergeable(struct kmem_cache *s);
struct kmem_cache *find_mergeable(unsigned size, unsigned align,
		slab_flags_t flags, const char *name, void (*ctor)(void *));
struct kmem_cache *
__kmem_cache_alias(const char *name, unsigned int size, unsigned int align,
		   slab_flags_t flags, void (*ctor)(void *));

slab_flags_t kmem_cache_flags(unsigned int object_size,
	slab_flags_t flags, const char *name,
	void (*ctor)(void *));

/* Functions provided by the slab allocators */
int __kmem_cache_create(struct kmem_cache *, slab_flags_t flags);

/* The list of all slab caches on the system */
extern struct list_head slab_caches;

static inline void cache_random_seq_destroy(struct kmem_cache *cachep) { }

static inline size_t slab_ksize(const struct kmem_cache *s)
{
	if (s->flags & SLAB_KASAN)
		return s->object_size;
	/*
	 * If we have the need to store the freelist pointer
	 * back there or track user information then we can
	 * only use the space before that information.
	 */
	if (s->flags & (SLAB_TYPESAFE_BY_RCU | SLAB_STORE_USER))
		return s->inuse;
	/*
	 * Else we can use all the padding etc for the allocation
	 */
	return s->size;
}

/* Find the kmalloc slab corresponding for a certain size */
struct kmem_cache *kmalloc_slab(size_t, gfp_t);

/*
 * Generic implementation of bulk operations
 * These are useful for situations in which the allocator cannot
 * perform optimizations. In that case segments of the object listed
 * may be allocated or freed using these operations.
 */
void __kmem_cache_free_bulk(struct kmem_cache *, size_t, void **);

struct kmem_cache *create_kmalloc_cache(const char *name, unsigned int size,
			slab_flags_t flags, unsigned int useroffset,
			unsigned int usersize);
extern void create_boot_cache(struct kmem_cache *, const char *name,
			unsigned int size, slab_flags_t flags,
			unsigned int useroffset, unsigned int usersize);

int __kmem_cache_shutdown(struct kmem_cache *);
void __kmem_cache_release(struct kmem_cache *);

#endif /* MM_SLAB_H */
