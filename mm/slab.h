/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MM_SLAB_H
#define MM_SLAB_H
/*
 * Internal slab definitions
 */
#include <linux/slub_def.h>

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
	//if (!memcg_kmem_enabled() &&
	 //   !unlikely(s->flags & SLAB_CONSISTENCY_CHECKS))
	//	return s;

	page = virt_to_head_page(x);
	cachep = page->slab_cache;
	if (slab_equal_or_root(cachep, s))
		return cachep;

	pr_err("%s: Wrong slab cache. %s but object is from %s\n",
	       __func__, s->name, cachep->name);
	WARN_ON_ONCE(1);
	return s;
}

#endif /* MM_SLAB_H */
