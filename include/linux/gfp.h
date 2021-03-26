#ifndef __LINUX_GFP_H
#define __LINUX_GFP_H

#include <linux/numa.h>
#include <linux/nodemask.h>
#include <linux/mmdebug.h>
#include <linux/page-flags-layout.h>
#include <linux/bitops.h>
#include <linux/stddef.h>
#include <linux/linkage.h>
#include <linux/mmzone.h>

/* Plain integer GFP bitmasks. Do not use this directly. */
#define ___GFP_DMA				BIT(0)
#define ___GFP_DMA32			BIT(1)
#define ___GFP_NORMAL			BIT(2)
#define ___GFP_MOVABLE			BIT(3)
#define GFP_ZONE_SHIFT			(0)

#define ___GFP_UNMOVABLE		BIT(4)
#define ___GFP_MOVABLE2			BIT(5)
#define ___GFP_RECLAIMABLE		BIT(6)
#define GFP_MIGRATE_SHIFT		(4)

#define ___GFP_IO				BIT(7)
#define ___GFP_FS				BIT(8)
#define ___GFP_COMP				BIT(9)
#define ___GFP_ZERO				BIT(10)
#define ___GFP_THISNODE			BIT(11)
#define ___GFP_NOWARN			BIT(12)

#define ___GFP_BITS_SHIFT		BIT(13)

/* If the above are modified, __GFP_BITS_SHIFT may need updating */

/*
 * GFP bitmasks..
 *
 * Zone modifiers (see linux/mmzone.h - low three bits)
 *
 * Do not put any conditional on these. If necessary modify the definitions
 * without the underscores and use them consistently. The definitions here may
 * be used in bit comparisons.
 */
#define __GFP_DMA	((__force gfp_t)___GFP_DMA)
#define __GFP_DMA32	((__force gfp_t)___GFP_DMA32)
#define __GFP_NORMAL	((__force gfp_t)___GFP_NORMAL)
#define __GFP_MOVABLE	((__force gfp_t)___GFP_MOVABLE)  /* Page is movable */
#define GFP_ZONEMASK	(__GFP_DMA|__GFP_DMA32|___GFP_NORMAL|__GFP_MOVABLE)

#define __GFP_UNMOVABLE		((__force gfp_t)___GFP_UNMOVABLE)
#define __GFP_MOVABLE2		((__force gfp_t)___GFP_MOVABLE2)
#define __GFP_RECLAIMABLE	((__force gfp_t)___GFP_RECLAIMABLE)
#define GFP_MIGRATE_MASK	(__GFP_UNMOVABLE | __GFP_MOVABLE2 | __GFP_RECLAIMABLE)

#define __GFP_IO	((__force gfp_t)___GFP_IO)	/* Can start physical IO? */
#define __GFP_FS	((__force gfp_t)___GFP_FS)	/* Can call down to low-level FS? */
#define __GFP_COMP	((__force gfp_t)___GFP_COMP)	/* Add compound page metadata */
#define __GFP_ZERO	((__force gfp_t)___GFP_ZERO)	/* Return zeroed page on success */
#define __GFP_THISNODE	((__force gfp_t)___GFP_THISNODE)
#define __GFP_NOWARN	((__force gfp_t)___GFP_NOWARN)

#define __GFP_BITS_SHIFT ___GFP_BITS_SHIFT
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

#define GFP_KERNEL	(__GFP_NORMAL | __GFP_UNMOVABLE | __GFP_FS)
#define GFP_USER	(__GFP_NORMAL | __GFP_MOVABLE2 | __GFP_FS)
#define GFP_TRANSHUGE	(__GFP_MOVABLE | __GFP_MOVABLE2 | __GFP_COMP)

/* Flag - indicates that the buffer will be suitable for DMA.  Ignored on some
   platforms, used as appropriate on others */

#define GFP_DMA		__GFP_DMA

/* 4GB DMA on some platforms */
#define GFP_DMA32	__GFP_DMA32

static inline int gfpflags_to_migratetype(const gfp_t gfp_flags)
{
	unsigned long flags;

	VM_WARN_ON((gfp_flags & GFP_MIGRATE_MASK) == GFP_MIGRATE_MASK);

	flags = (gfp_flags & GFP_MIGRATE_MASK) >> GFP_MIGRATE_SHIFT;

	return find_first_bit(&flags, BITS_PER_LONG);
}

#define GFP_ZONES_SHIFT ZONES_SHIFT

#if 16 * GFP_ZONES_SHIFT > BITS_PER_LONG
#error GFP_ZONES_SHIFT too large to create GFP_ZONE_TABLE integer
#endif

#ifdef CONFIG_ZONE_DMA
#define OPT_ZONE_DMA ZONE_DMA
#else
#define OPT_ZONE_DMA ZONE_NORMAL
#endif

#ifdef CONFIG_ZONE_DMA32
#define OPT_ZONE_DMA32 ZONE_DMA32
#else
#define OPT_ZONE_DMA32 ZONE_NORMAL
#endif

#define GFP_ZONE_TABLE (	\
		(OPT_ZONE_DMA << __GFP_DMA * GFP_ZONES_SHIFT)	\
		| (OPT_ZONE_DMA32 << __GFP_DMA32 * GFP_ZONES_SHIFT)	\
		| (ZONE_NORMAL << __GFP_NORMAL * GFP_ZONES_SHIFT)	\
		| (ZONE_MOVABLE << __GFP_MOVABLE * GFP_ZONES_SHIFT)	\
)

static inline enum zone_type gfp_zone(gfp_t flags)
{
	enum zone_type z;
	int bit = (__force int) (flags & GFP_ZONEMASK);

	z = (GFP_ZONE_TABLE >> (bit * GFP_ZONES_SHIFT)) &
					((1 << GFP_ZONES_SHIFT) - 1);
	
	return z;
}

struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, int preferred_nid,
							nodemask_t *nodemask);
static inline struct page *
__alloc_pages(gfp_t gfp_mask, unsigned int order, int preferred_nid)
{
	return __alloc_pages_nodemask(gfp_mask, order, preferred_nid, NULL);
}

/*
 * Allocate pages, preferring the node given as nid. The node must be valid and
 * online. For more general interface, see alloc_pages_node().
 */
static inline struct page *
__alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order)
{
	VM_BUG_ON(nid < 0 || nid >= MAX_NUMNODES);
	VM_WARN_ON((gfp_mask & __GFP_THISNODE) && !node_online(nid));

	return __alloc_pages(gfp_mask, order, nid);
}

/*
 * Allocate pages, preferring the node given as nid. When nid == NUMA_NO_NODE,
 * prefer the current CPU's closest node. Otherwise node must be valid and
 * online.
 */
static inline struct page *alloc_pages_node(int nid, gfp_t gfp_mask,
						unsigned int order)
{
	if (nid == NUMA_NO_NODE)
		/* TODO */;

	return __alloc_pages_node(nid, gfp_mask, order);
}

#define alloc_pages(gfp_mask, order) \
		alloc_pages_node(numa_node_id(), gfp_mask, order)
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

extern unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

#define __get_free_page(gfp_mask) \
		__get_free_pages((gfp_mask), 0)

#define __get_zeroed_page(gfp_mask)	\
		__get_free_pages(gfp_mask | __GFP_ZERO, 0)

#define __get_dma_pages(gfp_mask, order) \
		__get_free_pages((gfp_mask) | GFP_DMA, (order))

extern void __free_pages(struct page *page, unsigned int order);
extern void free_pages(unsigned long addr, unsigned int order);

#define __free_page(page) __free_pages((page), 0)
#define free_page(addr) free_pages((addr), 0)

#endif /* __LINUX_GFP_H */
