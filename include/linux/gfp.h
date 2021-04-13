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
#define ___GFP_NORMAL			BIT(1)
#define ___GFP_MOVABLE			BIT(2)
#define GFP_ZONE_SHIFT			(0)

#define ___GFP_ZERO				BIT(3)
#define ___GFP_THISNODE			BIT(4)
#define ___GFP_NOWARN			BIT(5)

#define ___GFP_BITS_SHIFT		6

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
#define __GFP_NORMAL	((__force gfp_t)___GFP_NORMAL)
#define __GFP_MOVABLE	((__force gfp_t)___GFP_MOVABLE)
#define GFP_ZONEMASK	(__GFP_DMA|__GFP_NORMAL|__GFP_MOVABLE)

#define __GFP_ZERO	((__force gfp_t)___GFP_ZERO)	/* Return zeroed page on success */
#define __GFP_THISNODE	((__force gfp_t)___GFP_THISNODE)
#define __GFP_NOWARN	((__force gfp_t)___GFP_NOWARN)

#define __GFP_BITS_SHIFT ___GFP_BITS_SHIFT
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

#define GFP_DMA		(__GFP_DMA)
#define GFP_KERNEL	(__GFP_NORMAL)
#define GFP_MOVABLE		(__GFP_MOVABLE)

#define GFP_USER	(GFP_KERNEL)

static inline enum zone_type gfp_zone(gfp_t flags)
{
	if (unlikely(__GFP_DMA & flags))
		return ZONE_DMA;
	if (unlikely(__GFP_MOVABLE & flags))
		return ZONE_MOVABLE;
	
	return ZONE_NORMAL;
}

static inline int gfp_zonelist(gfp_t flags)
{
#ifdef CONFIG_NUMA
	if (unlikely(flags & __GFP_THISNODE))
		return ZONELIST_NOFALLBACK;
#endif
	return ZONELIST_FALLBACK;
}

/*
 * We get the zone list from the current node and the gfp_mask.
 * This zone list contains a maximum of MAXNODES*MAX_NR_ZONES zones.
 * There are two zonelists per node, one for all zones with memory and
 * one containing just zones from the node the zonelist belongs to.
 *
 * For the normal case of non-DISCONTIGMEM systems the NODE_DATA() gets
 * optimized to &contig_page_data at compile-time.
 */
static inline struct zonelist *node_zonelist(int nid, gfp_t flags)
{
	return NODE_DATA(nid)->node_zonelists + gfp_zonelist(flags);
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
		nid = 0;

	return __alloc_pages_node(nid, gfp_mask, order);
}

#define alloc_pages(gfp_mask, order) \
		alloc_pages_node(this_cpu_numa_node_id(), gfp_mask, order)
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

static inline bool gfpflags_allow_blocking(const gfp_t gfp_flags)
{
	return false;
}

#endif /* __LINUX_GFP_H */
