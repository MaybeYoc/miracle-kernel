/*
 *  linux/mm/page_alloc.c
 *
 *  Manages the free list, the system allocates free pages here.
 *  Note that kmalloc() lives in slab.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  Reshaped it to be a zoned allocator, Ingo Molnar, Red Hat, 1999
 *  Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 *  Zone balancing, Kanoj Sarcar, SGI, Jan 2000
 *  Per cpu hot/cold page lists, bulk allocation, Martin J. Bligh, Sept 2002
 *          (lots of bits borrowed from Ingo Molnar & Andrew Morton)
 */
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/pfn.h>
#include <linux/percpu.h>

#include <asm/sections.h>

#include "internal.h"

static unsigned long arch_zone_lowest_possible_pfn[MAX_NR_ZONES] __initdata;
static unsigned long arch_zone_highest_possible_pfn[MAX_NR_ZONES] __initdata;
static unsigned long zone_movable_pfn[MAX_NUMNODES] __initdata;

static DEFINE_PER_CPU(struct per_cpu_pageset, boot_pageset);
static DEFINE_PER_CPU(struct per_cpu_nodestat, boot_nodestats);

atomic_long_t _totalram_pages __read_mostly;

#if MAX_NUMNODES > 1
int nr_node_ids __read_mostly = MAX_NUMNODES;
int nr_online_nodes __read_mostly = 1;
#endif

/*
 * Array of node states.
 */
nodemask_t node_states[NR_NODE_STATES] __read_mostly = {
	[N_POSSIBLE] = NODE_MASK_ALL,
	[N_ONLINE] = { { [0] = 1UL } },
#ifndef CONFIG_NUMA
	[N_NORMAL_MEMORY] = { { [0] = 1UL } },
	[N_MEMORY] = { { [0] = 1UL } },
	[N_CPU] = { { [0] = 1UL } },
#endif	/* NUMA */
};

static char * const zone_names[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	 "DMA",
#endif
#ifdef CONFIG_ZONE_DMA32
	 "DMA32",
#endif
	 "Normal",
	 "Movable",
};

const char * const migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Movable",
	"Reclaimable",
	"Pcprate",
};

static __always_inline void set_pageblock_migratetype(struct page *page, int migratetype)
{
	set_page_migrate(page, migratetype);
}

static __always_inline int get_pfnblock_migratetype(struct page *page, unsigned long pfn)
{
	return page_migratenum(page);
}

static __always_inline int get_pageblock_migratetype(struct page *page)
{
	return get_pfnblock_migratetype(page, page_to_pfn(page));
}

static inline int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	return 0;
}

static void bad_page(struct page *page, const char *reason,
		unsigned long bad_flags)
{
	/*
	 * Allow a burst of 60 reports, then keep quiet for that minute;
	 * or allow a steady drip of one report per second.
	 */
	pr_alert("BUG: Bad page state  pfn:%05lx\n", page_to_pfn(page));
	pr_warn("page dumped because: %s\n", reason);
	pr_alert("bad because of flags: %#lx(%pGp)\n",
							bad_flags, &bad_flags);
	page_mapcount_reset(page); /* remove PageBuddy */
}

static inline bool set_page_guard(struct zone *zone, struct page *page,
			unsigned int order, int migratetype)
{
	return false;
}

static inline void clear_page_guard(struct zone *zone, struct page *page,
				unsigned int order, int migratetype)
{
	BUG();
}

static inline struct page *__rmqueue_cma_fallback(struct zone *zone,
					unsigned int order)
{
	return NULL;
}

static inline bool is_migrate_isolate(int migratetype)
{
	return false;
}

static inline bool has_isolate_pageblock(struct zone *zone)
{
	return false;
}

/*
 * A cached value of the page's pageblock's migratetype, used when the page is
 * put on a pcplist. Used to avoid the pageblock migratetype lookup when
 * freeing from pcplists in most cases, at the cost of possibly becoming stale.
 * Also the migratetype set in the page does not necessarily match the pcplist
 * index, e.g. page might have MIGRATE_CMA set but be on a pcplist with any
 * other index - this ensures that it will be put on the correct CMA freelist.
 */
static inline int get_pcppage_migratetype(struct page *page)
{
	return page->index;
}

static inline void set_pcppage_migratetype(struct page *page, int migratetype)
{
	page->index = migratetype;
}

static inline void set_page_order(struct page *page, unsigned int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

static inline void rmv_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

/*
 * This function checks whether a page is free && is the buddy
 * we can coalesce a page and its buddy if
 * (a) the buddy is not in a hole (check before calling!) &&
 * (b) the buddy is in the buddy system &&
 * (c) a page and its buddy have the same order &&
 * (d) a page and its buddy are in the same zone.
 *
 * For recording whether a page is in the buddy system, we set PageBuddy.
 * Setting, clearing, and testing PageBuddy is serialized by zone->lock.
 *
 * For recording page's order, we use page_private(page).
 */
static inline int page_is_buddy(struct page *page, struct page *buddy,
							unsigned int order)
{
	if (page_is_guard(buddy) && page_order(buddy) == order) {
		if (page_zone_id(page) != page_zone_id(buddy))
			return 0;

		VM_BUG_ON_PAGE(page_count(buddy) != 0, buddy);

		return 1;
	}

	if (PageBuddy(buddy) && page_order(buddy) == order) {
		/*
		 * zone check is done late to avoid uselessly
		 * calculating zone/node ids for pages that could
		 * never merge.
		 */
		if (page_zone_id(page) != page_zone_id(buddy))
			return 0;

		VM_BUG_ON_PAGE(page_count(buddy) != 0, buddy);

		return 1;
	}
	return 0;
}

/*
 * The order of subdivision here is critical for the IO subsystem.
 * Please do not alter this order without good reasons and regression
 * testing. Specifically, as large blocks of memory are subdivided,
 * the order in which smaller blocks are delivered depends on the order
 * they're subdivided in this function. This is the primary factor
 * influencing the order in which pages are delivered to the IO
 * subsystem according to empirical testing, and this is also justified
 * by considering the behavior of a buddy system containing a single
 * large block of memory acted on by a series of small allocations.
 * This behavior is a critical factor in sglist merging's success.
 *
 * -- nyc
 */
static inline void expand(struct zone *zone, struct page *page,
	int low, int high, struct free_area *area,
	int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		VM_BUG_ON_PAGE(bad_range(zone, &page[size]), &page[size]);

		/*
		 * Mark as guard pages (or page), that will allow to
		 * merge back to allocator when buddy will be freed.
		 * Corresponding page table entries will not be touched,
		 * pages will stay not present in virtual address space
		 */
		if (set_page_guard(zone, &page[size], high, migratetype))
			continue;
		
		list_add(&page[size].lru, &area->free_list[migratetype]);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
}

/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static __always_inline struct page *
__rmqueue_smallest(struct zone *zone, unsigned int order,
							int migratetype)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		page = list_first_entry_or_null(&area->free_list[migratetype],
							struct page, lru);
		if (!page)
			continue;
		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		expand(zone, page, order, current_order, area, migratetype);
		set_pcppage_migratetype(page, migratetype);
		return page;
	}

	return NULL;
}

/*
 * Try finding a free buddy page on the fallback list and put it on the free
 * list of requested migratetype, possibly along with other pages from the same
 * block, depending on fragmentation avoidance heuristics. Returns true if
 * fallback was found so that __rmqueue_smallest() can grab it.
 *
 * The use of signed ints for order and current_order is a deliberate
 * deviation from the rest of this file, to make the for loop
 * condition simpler.
 */
static __always_inline bool
__rmqueue_fallback(struct zone *zone, int order, int start_migratetype,
						unsigned int alloc_flags)
{
	return false;
}

/*
 * Do the hard work of removing an element from the buddy allocator.
 * Call me with the zone->lock already held.
 */
static __always_inline struct page *
__rmqueue(struct zone *zone, unsigned int order, int migratetype,
						unsigned int alloc_flags)
{
	struct page *page;

retry:
	page = __rmqueue_smallest(zone, order, migratetype);
	if (unlikely(!page)) {
		if (migratetype == MIGRATE_MOVABLE)
			page = __rmqueue_cma_fallback(zone, order);
		
		if (!page && __rmqueue_fallback(zone, order, migratetype,
								alloc_flags))
			goto retry;
	}

	return page;
}

/*
 * Freeing function for a buddy system allocator.
 *
 * The concept of a buddy system is to maintain direct-mapped table
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep a list of pages, which are heads of continuous
 * free pages of length of (1 << order) and marked with PageBuddy.
 * Page's order is recorded in page_private(page) field.
 * So when we are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were
 * free, the remainder of the region must be split into blocks.
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.
 *
 * -- nyc
 */
static inline void __free_one_page(struct page *page,
		unsigned long pfn,
		struct zone *zone, unsigned int order,
		int migratetype)
{
	unsigned long combined_pfn;
	unsigned long uninitialized_var(buddy_pfn);
	struct page *buddy;
	unsigned int max_order;

	max_order = min_t(unsigned int, MAX_ORDER, pageblock_order + 1);

	VM_BUG_ON(!zone_is_initialized(zone));
	VM_BUG_ON_PAGE(page->flags & PAGE_FLAGS_CHECK_AT_PREP, page);

	VM_BUG_ON(migratetype == -1);
	if (likely(!is_migrate_isolate(migratetype)))
		; /* TODO Nothing to do now */

	VM_BUG_ON_PAGE(pfn & ((1 << order) - 1), page);
	VM_BUG_ON_PAGE(bad_range(zone, page), page);

continue_merging:
	while (order < max_order - 1) {
		buddy_pfn = __find_buddy_pfn(pfn, order);
		buddy = page + (buddy_pfn - pfn);

		if (!pfn_valid_within(buddy_pfn))
			goto done_merging;
		if (!page_is_buddy(page, buddy, order))
			goto done_merging;
		/*
		 * Our buddy is free or it is CONFIG_DEBUG_PAGEALLOC guard page,
		 * merge with it and move up one order.
		 */
		if (page_is_guard(buddy)) {
			clear_page_guard(zone, buddy, order, migratetype);
		} else {
			list_del(&buddy->lru);
			zone->free_area[order].nr_free--;
			rmv_page_order(buddy);
		}
		combined_pfn = buddy_pfn & pfn;
		page = page + (combined_pfn - pfn);
		pfn = combined_pfn;
		order++;
	}
	if (max_order < MAX_ORDER) {
		/* If we are here, it means order is >= pageblock_order.
		 * We want to prevent merge between freepages on isolate
		 * pageblock and normal pageblock. Without this, pageblock
		 * isolation could cause incorrect freepage or CMA accounting.
		 *
		 * We don't want to hit this code for the more frequent
		 * low-order merging.
		 */
		if (unlikely(has_isolate_pageblock(zone))) {
			int buddy_mt;

			buddy_pfn = __find_buddy_pfn(pfn, order);
			buddy = page + (buddy_pfn - pfn);
			buddy_mt = get_pageblock_migratetype(buddy);

			if (migratetype != buddy_mt
					&& (is_migrate_isolate(migratetype) ||
						is_migrate_isolate(buddy_mt)))
				goto done_merging;
		}
		max_order++;
		goto continue_merging;
	}
done_merging:
	set_page_order(page, order);

	/*
	 * If this is not the largest possible page, check if the buddy
	 * of the next-highest order is free. If it is, it's possible
	 * that pages are being freed that will coalesce soon. In case,
	 * that is happening, add the free page to the tail of the list
	 * so it's less likely to be used soon and more likely to be merged
	 * as a higher order page
	 */
	if ((order < MAX_ORDER - 2) && pfn_valid_within(buddy_pfn)) {
		struct page *higher_page, *higher_buddy;
		combined_pfn = buddy_pfn & pfn;
		higher_page = page + (combined_pfn - pfn);
		buddy_pfn = __find_buddy_pfn(combined_pfn, order + 1);
		higher_buddy = higher_page + (buddy_pfn - combined_pfn);
		if (pfn_valid_within(buddy_pfn) &&
		    page_is_buddy(higher_page, higher_buddy, order + 1)) {
			list_add_tail(&page->lru,
				&zone->free_area[order].free_list[migratetype]);
			goto out;
		}
	}

	list_add(&page->lru, &zone->free_area[order].free_list[migratetype]);
out:
	zone->free_area[order].nr_free++;
}

static inline void prefetch_buddy(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	unsigned long buddy_pfn = __find_buddy_pfn(pfn, 0);
	struct page *buddy = page + (buddy_pfn - pfn);

	prefetch(buddy);
}

/*
 * A bad page could be due to a number of fields. Instead of multiple branches,
 * try and check multiple fields with one check. The caller must do a detailed
 * check if necessary.
 */
static inline bool page_expected_state(struct page *page,
					unsigned long check_flags)
{
	if (unlikely(atomic_read(&page->_mapcount) != -1))
		return false;

	if (unlikely((unsigned long)page->mapping |
			page_ref_count(page) |
			(page->flags & check_flags)))
		return false;

	return true;
}

static void free_pages_check_bad(struct page *page)
{
	const char *bad_reason;
	unsigned long bad_flags;

	bad_reason = NULL;
	bad_flags = 0;

	if (unlikely(atomic_read(&page->_mapcount) != -1))
		bad_reason = "nonzero mapcount";
	if (unlikely(page->mapping != NULL))
		bad_reason = "non-NULL mapping";
	if (unlikely(page_ref_count(page) != 0))
		bad_reason = "nonzero _refcount";
	if (unlikely(page->flags & PAGE_FLAGS_CHECK_AT_FREE)) {
		bad_reason = "PAGE_FLAGS_CHECK_AT_FREE flag(s) set";
		bad_flags = PAGE_FLAGS_CHECK_AT_FREE;
	}

	bad_page(page, bad_reason, bad_flags);
}

static inline int free_pages_check(struct page *page)
{
	if (likely(page_expected_state(page, PAGE_FLAGS_CHECK_AT_FREE)))
		return 0;

	/* Something has gone sideways, find it */
	free_pages_check_bad(page);
	return 1;
}

static inline bool bulkfree_pcp_prepare(struct page *page)
{
	return free_pages_check(page);
}

static int free_tail_pages_check(struct page *head_page, struct page *page)
{
	BUG_ON(1);

	return 1;
}

/*
 * Frees a number of pages from the PCP lists
 * Assumes all pages on list are in same zone, and of same order.
 * count is the number of pages to free.
 *
 * If the zone was previously in an "all pages pinned" state then look to
 * see if this freeing clears that state.
 *
 * And clear the zone's pages_scanned counter, to hold off the "all pages are
 * pinned" detection logic.
 */
static void free_pcppages_bulk(struct zone *zone, int count,
					struct per_cpu_pages *pcp)
{
	int migratetype = 0;
	int batch_free = 0;
	int prefetch_nr = 0;
	bool isolated_pageblocks;
	struct page *page, *tmp;
	LIST_HEAD(head);

	while (count) {
		struct list_head *list;

		/*
		 * Remove pages from lists in a round-robin fashion. A
		 * batch_free count is maintained that is incremented when an
		 * empty list is encountered.  This is so more pages are freed
		 * off fuller lists instead of spinning excessively around empty
		 * lists
		 */
		do {
			batch_free++;
			if (++migratetype == MIGRATE_PCPTYPES)
				migratetype = 0;
			list = &pcp->lists[migratetype];
		} while (list_empty(list));

		/* This is the only non-empty list. Free them all. */
		if (batch_free == MIGRATE_PCPTYPES)
			batch_free = count;

		do {
			page = list_last_entry(list, struct page, lru);
			/* must delete to avoid corrupting pcp list */
			list_del(&page->lru);
			pcp->count--;

			if (bulkfree_pcp_prepare(page))
				continue;

			list_add_tail(&page->lru, &head);

			/*
			 * We are going to put the page back to the global
			 * pool, prefetch its buddy to speed up later access
			 * under zone->lock. It is believed the overhead of
			 * an additional test and calculating buddy_pfn here
			 * can be offset by reduced memory latency later. To
			 * avoid excessive prefetching due to large count, only
			 * prefetch buddy for the first pcp->batch nr of pages.
			 */
			if (prefetch_nr++ < pcp->batch)
				prefetch_buddy(page);
		} while (--count && --batch_free && !list_empty(list));
	}

	spin_lock(&zone->lock);
	isolated_pageblocks = has_isolate_pageblock(zone);

	/*
	 * Use safe version since after __free_one_page(),
	 * page->lru.next will not point to original list.
	 */
	list_for_each_entry_safe(page, tmp, &head, lru) {
		int mt = get_pcppage_migratetype(page);
		/* MIGRATE_ISOLATE page should not go to pcplists */
		VM_BUG_ON_PAGE(is_migrate_isolate(mt), page);
		/* Pageblock could have been isolated meanwhile */
		if (unlikely(isolated_pageblocks))
			mt = get_pageblock_migratetype(page);

		__free_one_page(page, page_to_pfn(page), zone, 0, mt);
	}
	spin_unlock(&zone->lock);
}

static void free_one_page(struct zone *zone,
				struct page *page, unsigned long pfn,
				unsigned int order,
				int migratetype)
{
	spin_lock(&zone->lock);
	if (unlikely(has_isolate_pageblock(zone) ||
		is_migrate_isolate(migratetype))) {
		migratetype = get_pfnblock_migratetype(page, pfn);
	}
	__free_one_page(page, pfn, zone, order, migratetype);
	spin_unlock(&zone->lock);
}

static __always_inline bool free_pages_prepare(struct page *page,
					unsigned int order, bool check_free)
{
	int bad = 0;

	VM_BUG_ON_PAGE(PageTail(page), page);

	/*
	 * Check tail pages before head page information is cleared to
	 * avoid checking PageCompound for order-0 pages.
	 */
	if (unlikely(order)) {
		bool compound = PageCompound(page);
		int i;

		VM_BUG_ON_PAGE(compound && compound_order(page) != order, page);

		if (compound)
			BUG_ON(1); /* TODO */
		for (i = 1; i < (1 << order); i++) {
			if (compound)
				bad += free_tail_pages_check(page, page + i);
			if (unlikely(free_pages_check(page + i))) {
				bad++;
				continue;
			}
			(page + i)->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
		}
	}
	if (PageMappingFlags(page))
		page->mapping = NULL;
	if (check_free)
		bad += free_pages_check(page);
	if (bad)
		return false;

	page->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;

	return true;
}

static bool free_pcp_prepare(struct page *page)
{
	return free_pages_prepare(page, 0, false);
}

static void __free_pages_ok(struct page *page, unsigned int order)
{
	unsigned long flags;
	int migratetype;
	unsigned long pfn = page_to_pfn(page);

	if (!free_pages_prepare(page, order, true))
		return;

	migratetype = get_pfnblock_migratetype(page, pfn);
	local_irq_save(flags);
	free_one_page(page_zone(page), page, pfn, order, migratetype);
	local_irq_restore(flags);
}

static bool free_unref_page_prepare(struct page *page, unsigned long pfn)
{
	int migratetype;

	if (!free_pcp_prepare(page))
		return false;

	migratetype = get_pfnblock_migratetype(page, pfn);
	set_pcppage_migratetype(page, migratetype);
	return true;
}

static void free_unref_page_commit(struct page *page, unsigned long pfn)
{
	struct zone *zone = page_zone(page);
	struct per_cpu_pages *pcp;
	int migratetype;

	migratetype = get_pcppage_migratetype(page);

	/*
	 * We only track unmovable, reclaimable and movable on pcp lists.
	 * Free ISOLATE pages back to the allocator because they are being
	 * offlined but treat HIGHATOMIC as movable pages so we can get those
	 * areas back if necessary. Otherwise, we may have to free
	 * excessively into the page allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
		if (unlikely(is_migrate_isolate(migratetype))) {
			free_one_page(zone, page, pfn, 0, migratetype);
			WARN_ON(1);
			return;
		}
		migratetype = MIGRATE_MOVABLE;
	}

	pcp = &this_cpu_ptr(zone->pageset)->pcp;
	list_add(&page->lru, &pcp->lists[migratetype]);
	pcp->count++;
	if (pcp->count >= pcp->high) {
		unsigned long batch = READ_ONCE(pcp->batch);
		free_pcppages_bulk(zone, batch, pcp);
	}
}

/*
 * Free a 0-order page
 */
void free_unref_page(struct page *page)
{
	unsigned long flags;
	unsigned long pfn = page_to_pfn(page);

	if (!free_unref_page_prepare(page, pfn))
		return;

	local_irq_save(flags);
	free_unref_page_commit(page, pfn);
	local_irq_restore(flags);
}

static inline void free_the_page(struct page *page, unsigned int order)
{
	if (order == 0)		/* Via pcp? */
		free_unref_page(page);
	else
		__free_pages_ok(page, order);
}

void __free_pages(struct page *page, unsigned int order)
{
	if (put_page_testzero(page))
		free_the_page(page, order);
}

void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		VM_BUG_ON(!virt_addr_valid((void *)addr));
		__free_pages(virt_to_page((void *)addr), order);
	}
}

static inline bool prepare_alloc_pages(gfp_t gfp_mask, unsigned int order,
		int preferred_nid, nodemask_t *nodemask,
		struct alloc_context *ac, gfp_t *allock_flags)
{
	ac->zoneidx = gfp_zone(gfp_mask);
	ac->nodemask = nodemask;
	ac->migratetype = gfpflags_to_migratetype(gfp_mask);

	WARN_ON(ac->migratetype != MIGRATE_UNMOVABLE);

	return true;
}

static inline void finalise_ac(gfp_t gfp_mask, struct alloc_context *ac)
{
	/*
	 * The preferred zone is used for statistics but crucially it is
	 * also used as the starting point for the zonelist iterator. It
	 * may get reset for allocations that ignore memory policies.
	 */

}

/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, int preferred_nid,
							nodemask_t *nodemask)
{
	struct page *page;
	gfp_t alloc_flags;
	struct alloc_context ac = {};

	/*
	 * There are several places where we assume that the order value is sane
	 * so bail out early if the request is out of bound.
	 */
	if (unlikely(order >= MAX_ORDER)) {
		WARN_ON_ONCE(!(gfp_mask & __GFP_NOWARN));
		return NULL;
	}

	alloc_flags = gfp_mask;
	if (unlikely(!prepare_alloc_pages(gfp_mask, order, preferred_nid, nodemask,
								&ac, &alloc_flags)))
		return 	NULL;

	finalise_ac(gfp_mask, &ac);

	return page;
}

/*
 * Common helper functions. Never use with __GFP_HIGHMEM because the returned
 * address cannot represent highmem pages. Use alloc_pages and then kmap if
 * you need to access high mem.
 */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return 0;
	return (unsigned long) page_address(page);
}

static inline void init_reserved_page(unsigned long pfn)
{
}

/*
 * Initialised pages do not have PageReserved set. This function is
 * called for each range allocated by the bootmem allocator and
 * marks the pages PageReserved. The remaining valid pages are later
 * sent to the buddy page allocator.
 */
void __meminit reserve_bootmem_region(phys_addr_t start, phys_addr_t end)
{
	unsigned long start_pfn = PFN_DOWN(start);
	unsigned long end_pfn = PFN_UP(end);

	for (; start_pfn < end_pfn; start_pfn++) {
		if (pfn_valid(start_pfn)) {
			struct page *page = pfn_to_page(start_pfn);

			init_reserved_page(start_pfn);

			/* Avoid false-positive PageTail() */
			INIT_LIST_HEAD(&page->lru);

			/*
			 * no need for atomic set_bit because the struct
			 * page is not visible yet so nobody should
			 * access it yet.
			 */
			__SetPageReserved(page);
		}
	}
}

static void __init __free_pages_boot_core(struct page *page, unsigned int order)
{
	unsigned int nr_pages = 1 << order;
	struct page *p = page;
	unsigned int loop;

	prefetchw(p);
	for (loop = 0; loop < (nr_pages - 1); loop++, p++) {
		prefetchw(p + 1);
		__ClearPageReserved(p);
		set_page_count(p, 0);
	}
	__ClearPageReserved(p);
	set_page_count(p, 0);

	atomic_long_add(nr_pages, &page_zone(page)->managed_pages);
	set_page_refcounted(page);
	__free_pages(page, order);
}

void __init memblock_free_pages(struct page *page, unsigned long pfn,
							unsigned int order)
{
	return __free_pages_boot_core(page, order);
}

void __init mem_init_print_info(const char *str)
{
	unsigned long physpages, codesize, datasize, rosize, bss_size;
	unsigned long init_code_size, init_data_size;

	physpages = get_num_physpages();
	codesize = _etext - _stext;
	datasize = _edata - _sdata;
	rosize = __end_rodata - __start_rodata;
	bss_size = __bss_stop - __bss_start;
	init_data_size = __init_end - __init_begin;
	init_code_size = _einittext - _sinittext;

	/*
	 * Detect special cases and adjust section sizes accordingly:
	 * 1) .init.* may be embedded into .data sections
	 * 2) .init.text.* may be out of [__init_begin, __init_end],
	 *    please refer to arch/tile/kernel/vmlinux.lds.S.
	 * 3) .rodata.* may be embedded into .text or .data sections.
	 */
#define adj_init_size(start, end, size, pos, adj) \
	do { \
		if (start <= pos && pos < end && size > adj) \
			size -= adj; \
	} while (0)

	adj_init_size(__init_begin, __init_end, init_data_size,
		     _sinittext, init_code_size);
	adj_init_size(_stext, _etext, codesize, _sinittext, init_code_size);
	adj_init_size(_sdata, _edata, datasize, __init_begin, init_data_size);
	adj_init_size(_stext, _etext, codesize, __start_rodata, rosize);
	adj_init_size(_sdata, _edata, datasize, __start_rodata, rosize);

#undef	adj_init_size

/* TODO Temp */
#define nr_free_pages() (1UL)

	pr_info("Memory: %luK/%luK available (%luK kernel code, %luK rwdata, %luK rodata, %luK init, %luK bss, %luK reserved"
		"%s%s)\n",
		nr_free_pages() << (PAGE_SHIFT - 10),
		physpages << (PAGE_SHIFT - 10),
		codesize >> 10, datasize >> 10, rosize >> 10,
		(init_data_size + init_code_size) >> 10, bss_size >> 10,
		(physpages - totalram_pages()) << (PAGE_SHIFT - 10),
		str ? ", " : "", str ? str : "");
}

void adjust_managed_page_count(struct page *page, long count)
{
	atomic_long_add(count, &page_zone(page)->managed_pages);
	totalram_pages_add(count);
}

unsigned long free_reserved_area(void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

	return pages;
}

#if MAX_NUMNODES > 1
/*
 * Figure out the number of possible node ids.
 */
void __init setup_nr_node_ids(void)
{
	unsigned int highest;

	highest = find_last_bit(node_possible_map.bits, MAX_NUMNODES);
	nr_node_ids = highest + 1;
}
#endif

/**
 * get_pfn_range_for_nid - Return the start and end page frames for a node
 * @nid: The nid to return the range for. If MAX_NUMNODES, the min and max PFN are returned.
 * @start_pfn: Passed by reference. On return, it will have the node start_pfn.
 * @end_pfn: Passed by reference. On return, it will have the node end_pfn.
 *
 * It returns the start and end page frame of a node based on information
 * provided by memblock_set_node(). If called for a node
 * with no available memory, a warning is printed and the start and end
 * PFNs will be 0.
 */
void __init get_pfn_range_for_nid(unsigned int nid,
			unsigned long *start_pfn, unsigned long *end_pfn)
{
	unsigned long this_start_pfn, this_end_pfn;
	int i;

	*start_pfn = -1UL;
	*end_pfn = 0;

	for_each_mem_pfn_range(i, nid, &this_start_pfn, &this_end_pfn, NULL) {
		*start_pfn = min(*start_pfn, this_start_pfn);
		*end_pfn = max(*end_pfn, this_end_pfn);
	}

	if (*start_pfn == -1UL)
		*start_pfn = 0;
}

/* Find the lowest pfn for a node */
static unsigned long __init find_min_pfn_for_node(int nid)
{
	unsigned long min_pfn = ULONG_MAX;
	unsigned long start_pfn;
	int i;

	for_each_mem_pfn_range(i, nid, &start_pfn, NULL, NULL)
		min_pfn = min(min_pfn, start_pfn);

	if (min_pfn == ULONG_MAX) {
		pr_warn("Could not find start_pfn for node %d\n", nid);
		return 0;
	}

	return min_pfn;
}

/**
 * find_min_pfn_with_active_regions - Find the minimum PFN registered
 *
 * It returns the minimum PFN based on information provided via
 * memblock_set_node().
 */
unsigned long __init find_min_pfn_with_active_regions(void)
{
	return find_min_pfn_for_node(MAX_NUMNODES);
}

static void pageset_init(struct per_cpu_pageset *p)
{
	struct per_cpu_pages *pcp;
	int migratetype;

	memset(p, 0, sizeof(*p));

	pcp = &p->pcp;
	for (migratetype = 0; migratetype < MIGRATE_PCPTYPES; migratetype++)
		INIT_LIST_HEAD(&pcp->lists[migratetype]);
}

/*
 * pcp->high and pcp->batch values are related and dependent on one another:
 * ->batch must never be higher then ->high.
 * The following function updates them in a safe manner without read side
 * locking.
 *
 * Any new users of pcp->batch and pcp->high should ensure they can cope with
 * those fields changing asynchronously (acording the the above rule).
 *
 * mutex_is_locked(&pcp_batch_high_lock) required when calling this function
 * outside of boot time (or some other assurance that no concurrent updaters
 * exist).
 */
static void pageset_update(struct per_cpu_pages *pcp, unsigned long high,
		unsigned long batch)
{
       /* start with a fail safe value for batch */
	pcp->batch = 1;
	smp_wmb();

       /* Update high, then batch, in order */
	pcp->high = high;
	smp_wmb();

	pcp->batch = batch;
}

/* a companion to pageset_set_high() */
static void pageset_set_batch(struct per_cpu_pageset *p, unsigned long batch)
{
	pageset_update(&p->pcp, 6 * batch, max(1UL, 1 * batch));
}

static void setup_pageset(struct per_cpu_pageset *p, unsigned long batch)
{
	pageset_init(p);
	pageset_set_batch(p, batch);
}

static void __build_all_zonelists(void *data)
{

}

static noinline void __init
build_all_zonelists_init(void)
{
	int cpu;

	__build_all_zonelists(NULL);

	/*
	 * Initialize the boot_pagesets that are going to be used
	 * for bootstrapping processors. The real pagesets for
	 * each zone will be allocated later when the per cpu
	 * allocator is available.
	 *
	 * boot_pagesets are used also for bootstrapping offline
	 * cpus if the system is already booted because the pagesets
	 * are needed to initialize allocators on a specific cpu too.
	 * F.e. the percpu allocator needs the page allocator which
	 * needs the percpu allocator in order to allocate its pagesets
	 * (a chicken-egg dilemma).
	 */
	for_each_possible_cpu(cpu)
		setup_pageset(&per_cpu(boot_pageset, cpu), 0);
}

/*
 * unless system_state == SYSTEM_BOOTING.
 *
 * __ref due to call of __init annotated helper build_all_zonelists_init
 * [protected by SYSTEM_BOOTING].
 */
void __ref build_all_zonelists(pg_data_t *pgdat)
{
	if (system_state == SYSTEM_BOOTING) {
		build_all_zonelists_init();
	} else {
		__build_all_zonelists(pgdat);
		/* cpuset refresh routine should be here */
	}
}

static void __meminit zone_init_free_lists(struct zone *zone)
{
	unsigned int order, t;
	for_each_migratetype_order(order, t) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list[t]);
		zone->free_area[order].nr_free = 0;
	}
}

static void __meminit __init_single_page(struct page *page, unsigned long pfn,
				unsigned long zone, int nid)
{
	mm_zero_struct_page(page);
	set_page_links(page, zone, nid);
	init_page_count(page);
	page_mapcount_reset(page);
	INIT_LIST_HEAD(&page->lru);
}

static void __meminit memmap_init_zone(unsigned long size, int nid, unsigned long zone,
				  unsigned long start_pfn)
{
	unsigned long pfn, end_pfn = start_pfn + size;
	struct page *page;

	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		page = pfn_to_page(pfn);
		__init_single_page(page, pfn, zone, nid);

		set_pageblock_migratetype(page, MIGRATE_UNMOVABLE);
	}
}

static void __meminit memmap_init(unsigned long size, int nid,
				  unsigned long zone, unsigned long start_pfn)
{
	memmap_init_zone(size, nid, zone, start_pfn);
}

static void __meminit init_currently_empty_zone(struct zone *zone,
					unsigned long zone_start_pfn,
					unsigned long size)
{
	zone->zone_start_pfn = zone_start_pfn;

	zone_init_free_lists(zone);
	zone->initialized = 1;
}

static void __meminit pgdat_init_internals(struct pglist_data *pgdat)
{
	spin_lock_init(&pgdat->lru_lock);
}

static int zone_batchsize(struct zone *zone)
{
#ifdef CONFIG_MMU
	int batch;

	/*
	 * The per-cpu-pages pools are set to around 1000th of the
	 * size of the zone.
	 */
	batch = zone_managed_pages(zone) / 1024;
	/* But no more than a meg. */
	if (batch * PAGE_SIZE > 1024 * 1024)
		batch = (1024 * 1024) / PAGE_SIZE;
	batch /= 4;		/* We effectively *= 4 below */
	if (batch < 1)
		batch = 1;

	/*
	 * Clamp the batch to a 2^n - 1 value. Having a power
	 * of 2 value was found to be more likely to have
	 * suboptimal cache aliasing properties in some cases.
	 *
	 * For example if 2 tasks are alternately allocating
	 * batches of pages, one task can end up with a lot
	 * of pages of one half of the possible page colors
	 * and the other with pages of the other colors.
	 */
	batch = rounddown_pow_of_two(batch + batch/2) - 1;

	return batch;

#else
	/* The deferral and batching of frees should be suppressed under NOMMU
	 * conditions.
	 *
	 * The problem is that NOMMU needs to be able to allocate large chunks
	 * of contiguous memory as there's no hardware page translation to
	 * assemble apparent contiguous memory from discontiguous pages.
	 *
	 * Queueing large contiguous runs of pages for batching, however,
	 * causes the pages to actually be freed in smaller chunks.  As there
	 * can be a significant delay between the individual batches being
	 * recycled, this leads to the once large chunks of space being
	 * fragmented and becoming unavailable for high-order allocations.
	 */
	return 0;
#endif
}

static void pageset_set_high_and_batch(struct zone *zone,
				       struct per_cpu_pageset *pcp)
{
	pageset_set_batch(pcp, zone_batchsize(zone));
}

static void __meminit zone_pageset_init(struct zone *zone, int cpu)
{
	struct per_cpu_pageset *pcp = per_cpu_ptr(zone->pageset, cpu);

	pageset_init(pcp);
	pageset_set_high_and_batch(zone, pcp);
}

void __meminit setup_zone_pageset(struct zone *zone)
{
	int cpu;
	zone->pageset = alloc_percpu(struct per_cpu_pageset);
	for_each_possible_cpu(cpu)
		zone_pageset_init(zone, cpu);
}

/*
 * Allocate per cpu pagesets and initialize them.
 * Before this call only boot pagesets were available.
 */
void __init setup_per_cpu_pageset(void)
{
	struct pglist_data *pgdat;
	struct zone *zone;

	for_each_populated_zone(zone)
		setup_zone_pageset(zone);

	for_each_online_pgdat(pgdat)
		pgdat->per_cpu_nodestats =
			alloc_percpu(struct per_cpu_nodestat);
}

static __meminit void zone_pcp_init(struct zone *zone)
{
	/*
	 * per cpu subsystem is not up at this point. The following code
	 * relies on the ability of the linker to provide the
	 * offset of a (static) per cpu variable into the per cpu area.
	 */
	zone->pageset = &boot_pageset;

	if (populated_zone(zone))
		printk(KERN_DEBUG "  %s zone: %lu pages, LIFO batch:%u\n",
			zone->name, zone->present_pages,
					 zone_batchsize(zone));
}

static void __meminit zone_init_internals(struct zone *zone, enum zone_type idx, int nid,
							unsigned long remaining_pages)
{
	atomic_long_set(&zone->managed_pages, remaining_pages);
	zone_set_nid(zone, nid);
	zone->name = zone_names[idx];
	zone->zone_pgdat = NODE_DATA(nid);
	spin_lock_init(&zone->lock);
	zone_pcp_init(zone);
}

static void __init free_area_init_core(struct pglist_data *pgdat)
{
	enum zone_type j;
	int nid = pgdat->node_id;

	pgdat_init_internals(pgdat);
	pgdat->per_cpu_nodestats = &boot_nodestats;

	for (j = 0; j < MAX_NR_ZONES; j++) {
		struct zone *zone = pgdat->node_zones + j;
		unsigned long size, freesize;
		unsigned long zone_start_pfn = zone->zone_start_pfn;

		size = zone->spanned_pages;
		freesize = zone->present_pages;

		/*
		 * Set an approximate value for lowmem here, it will be adjusted
		 * when the bootmem allocator frees pages into the buddy system.
		 * And all highmem pages will be managed by the buddy system.
		 */
		zone_init_internals(zone, j, nid, freesize);

		if (!size)
			continue;

		init_currently_empty_zone(zone, zone_start_pfn, size);
		memmap_init(size, nid, j, zone_start_pfn);
	}
}

/*
 * The zone ranges provided by the architecture do not include ZONE_MOVABLE
 * because it is sized independent of architecture. Unlike the other zones,
 * the starting point for ZONE_MOVABLE is not fixed. It may be different
 * in each node depending on the size of each node and how evenly kernelcore
 * is distributed. This helper function adjusts the zone ranges
 * provided by the architecture for a given node by using the end of the
 * highest usable zone for ZONE_MOVABLE. This preserves the assumption that
 * zones within a node are in order of monotonic increases memory addresses
 */
static void __init adjust_zone_range_for_zone_movable(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn)
{
	/* Only adjust if ZONE_MOVABLE is on this node */
	if (zone_movable_pfn[nid]) {
		;/* TODO */
	}
}

/*
 * Return the number of pages a zone spans in a node, including holes
 * present_pages = zone_spanned_pages_in_node() - zone_absent_pages_in_node()
 */
static unsigned long __init zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn,
					unsigned long *ignored)
{
	/* When hotadd a new node from cpu_up(), the node should be empty */
	if (!node_start_pfn && !node_end_pfn)
		return 0;

	/* Get the start and end of the zone */
	*zone_start_pfn = arch_zone_lowest_possible_pfn[zone_type];
	*zone_end_pfn = arch_zone_highest_possible_pfn[zone_type];
	adjust_zone_range_for_zone_movable(nid, zone_type,
				node_start_pfn, node_end_pfn,
				zone_start_pfn, zone_end_pfn);

	/* Check that this node has pages within the zone's required range */
	if (*zone_end_pfn < node_start_pfn || *zone_start_pfn > node_end_pfn)
		return 0;

	/* Move the zone boundaries inside the node if necessary */
	*zone_end_pfn = min(*zone_end_pfn, node_end_pfn);
	*zone_start_pfn = max(*zone_start_pfn, node_start_pfn);

	/* Return the spanned pages */
	return *zone_end_pfn - *zone_start_pfn;
}

/*
 * Return the number of holes in a range on a node. If nid is MAX_NUMNODES,
 * then all holes in the requested range will be accounted for.
 */
unsigned long __init __absent_pages_in_range(int nid,
				unsigned long range_start_pfn,
				unsigned long range_end_pfn)
{
	unsigned long nr_absent = range_end_pfn - range_start_pfn;
	unsigned long start_pfn, end_pfn;
	int i;

	for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, NULL) {
		start_pfn = clamp(start_pfn, range_start_pfn, range_end_pfn);
		end_pfn = clamp(end_pfn, range_start_pfn, range_end_pfn);
		nr_absent -= end_pfn - start_pfn;
	}
	return nr_absent;
}

/* Return the number of page frames in holes in a zone on a node */
static unsigned long __init zone_absent_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *ignored)
{
	unsigned long zone_low = arch_zone_lowest_possible_pfn[zone_type];
	unsigned long zone_high = arch_zone_highest_possible_pfn[zone_type];
	unsigned long zone_start_pfn, zone_end_pfn;
	unsigned long nr_absent;

	/* When hotadd a new node from cpu_up(), the node should be empty */
	if (!node_start_pfn && !node_end_pfn)
		return 0;

	zone_start_pfn = clamp(node_start_pfn, zone_low, zone_high);
	zone_end_pfn = clamp(node_end_pfn, zone_low, zone_high);

	adjust_zone_range_for_zone_movable(nid, zone_type,
			node_start_pfn, node_end_pfn,
			&zone_start_pfn, &zone_end_pfn);
	nr_absent = __absent_pages_in_range(nid, zone_start_pfn, zone_end_pfn);

	return nr_absent;
}

static void __init calculate_node_totalpages(struct pglist_data *pgdat,
						unsigned long node_start_pfn,
						unsigned long node_end_pfn,
						unsigned long *zones_size,
						unsigned long *zholes_size)
{
	unsigned long realtotalpages = 0, totalpages = 0;
	enum zone_type i;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = pgdat->node_zones + i;
		unsigned long zone_start_pfn, zone_end_pfn;
		unsigned long size, real_size;

		size = zone_spanned_pages_in_node(pgdat->node_id, i,
						  node_start_pfn,
						  node_end_pfn,
						  &zone_start_pfn,
						  &zone_end_pfn,
						  zones_size);
		real_size = size - zone_absent_pages_in_node(pgdat->node_id, i,
						  node_start_pfn, node_end_pfn,
						  zholes_size);
		if (size)
			zone->zone_start_pfn = zone_start_pfn;
		else
			zone->zone_start_pfn = 0;
		zone->spanned_pages = size;
		zone->present_pages = real_size;

		totalpages += size;
		realtotalpages += real_size;
	}

	pgdat->node_spanned_pages = totalpages;
	pgdat->node_present_pages = realtotalpages;
	printk(KERN_DEBUG "On node %d totalpages: %lu\n", pgdat->node_id,
							realtotalpages);
}

void __init free_area_init_node(int nid, unsigned long *zones_size,
				   unsigned long node_start_pfn,
				   unsigned long *zholes_size)
{
	struct pglist_data *pgdat = NODE_DATA(nid);
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;

	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	pgdat->per_cpu_nodestats = NULL;

	get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
	pr_info("Initmem setup node %d [mem %#018Lx-%#018Lx]\n", nid,
		(u64)start_pfn << PAGE_SHIFT,
		end_pfn ? ((u64)end_pfn << PAGE_SHIFT) - 1 : 0);

	calculate_node_totalpages(pgdat, start_pfn, end_pfn,
				  zones_size, zholes_size);

	free_area_init_core(pgdat);
}

/*
 * Find the PFN the Movable zone begins in each node. Kernel memory
 * is spread evenly between nodes as long as the nodes have enough
 * memory. When they don't, some nodes will have more kernelcore than
 * others
 */
static void __init find_zone_movable_pfns_for_nodes(void)
{
	/* TODO */
}

/*
 * Zero all valid struct pages in range [spfn, epfn), return number of struct
 * pages zeroed
 */
static u64 zero_pfn_range(unsigned long spfn, unsigned long epfn)
{
	unsigned long pfn;
	u64 pgcnt = 0;

	for (pfn = spfn; pfn < epfn; pfn++) {
		if (!pfn_valid(ALIGN_DOWN(pfn, pageblock_nr_pages))) {
			pfn = ALIGN_DOWN(pfn, pageblock_nr_pages)
				+ pageblock_nr_pages - 1;
			continue;
		}
		mm_zero_struct_page(pfn_to_page(pfn));
		pgcnt++;
	}

	return pgcnt;
}

/*
 * Only struct pages that are backed by physical memory are zeroed and
 * initialized by going through __init_single_page(). But, there are some
 * struct pages which are reserved in memblock allocator and their fields
 * may be accessed (for example page_to_pfn() on some configuration accesses
 * flags). We must explicitly zero those struct pages.
 *
 * This function also addresses a similar issue where struct pages are left
 * uninitialized because the physical address range is not covered by
 * memblock.memory or memblock.reserved. That could happen when memblock
 * layout is manually configured via memmap=.
 */
void __init zero_resv_unavail(void)
{
	phys_addr_t start, end;
	u64 i, pgcnt;
	phys_addr_t next = 0;

	/*
	 * Loop through unavailable ranges not covered by memblock.memory.
	 */
	pgcnt = 0;
	for_each_mem_range(i, &memblock.memory, NULL,
			NUMA_NO_NODE, MEMBLOCK_NONE, &start, &end, NULL) {
		if (next < start)
			pgcnt += zero_pfn_range(PFN_DOWN(next), PFN_UP(start));
		next = end;
	}
	pgcnt += zero_pfn_range(PFN_DOWN(next), max_pfn);

	/*
	 * Struct pages that do not have backing memory. This could be because
	 * firmware is using some of this memory, or for some other reasons.
	 */
	if (pgcnt)
		pr_info("Zeroed struct page in unavailable ranges: %lld pages", pgcnt);
}

/* Any regular or high memory on that node ? */
static void check_for_memory(struct pglist_data *pgdat, int nid)
{

}

/**
 * free_area_init_nodes - Initialise all pg_data_t and zone data
 * @max_zone_pfn: an array of max PFNs for each zone
 *
 * This will call free_area_init_node() for each active node in the system.
 * Using the page ranges provided by memblock_set_node(), the size of each
 * zone in each node and their holes is calculated. If the maximum PFN
 * between two adjacent zones match, it is assumed that the zone is empty.
 * For example, if arch_max_dma_pfn == arch_max_dma32_pfn, it is assumed
 * that arch_max_dma32_pfn has no pages. It is also assumed that a zone
 * starts where the previous one ended. For example, ZONE_DMA32 starts
 * at arch_max_dma_pfn.
 */
void __init free_area_init_nodes(unsigned long *max_zone_pfn)
{
	unsigned long start_pfn, end_pfn;
	int i, nid;

	/* Record where the zone boundaries are */
	memset(arch_zone_lowest_possible_pfn, 0,
				sizeof(arch_zone_lowest_possible_pfn));
	memset(arch_zone_highest_possible_pfn, 0,
				sizeof(arch_zone_highest_possible_pfn));

	start_pfn = find_min_pfn_with_active_regions();

	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (i == ZONE_MOVABLE)
			continue;

		end_pfn = max(max_zone_pfn[i], start_pfn);
		arch_zone_lowest_possible_pfn[i] = start_pfn;
		arch_zone_highest_possible_pfn[i] = end_pfn;

		start_pfn = end_pfn;
	}

	/* Find the PFNs that ZONE_MOVABLE begins at in each node */
	memset(zone_movable_pfn, 0, sizeof(zone_movable_pfn));
	find_zone_movable_pfns_for_nodes();

	/* Print out the zone ranges */
	pr_info("Zone ranges:\n");
	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (i == ZONE_MOVABLE)
			continue;
		pr_info("  %-8s ", zone_names[i]);
		if (arch_zone_lowest_possible_pfn[i] ==
				arch_zone_highest_possible_pfn[i])
			pr_cont("empty\n");
		else
			pr_cont("[mem %#018Lx-%#018Lx]\n",
				(u64)arch_zone_lowest_possible_pfn[i]
					<< PAGE_SHIFT,
				((u64)arch_zone_highest_possible_pfn[i]
					<< PAGE_SHIFT) - 1);
	}

	/* Print out the PFNs ZONE_MOVABLE begins at in each node */
	pr_info("Movable zone start for each node\n");
	for (i = 0; i < MAX_NUMNODES; i++) {
		if (zone_movable_pfn[i])
			pr_info("  Node %d: %#018Lx\n", i,
			       (u64)zone_movable_pfn[i] << PAGE_SHIFT);
	}

	/* Print out the early node map */
	pr_info("Early memory node ranges\n");
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid)
		pr_info("  node %3d: [mem %#018Lx-%#018Lx]\n", nid,
			(u64)start_pfn << PAGE_SHIFT,
			((u64)end_pfn << PAGE_SHIFT) - 1);

	setup_nr_node_ids();
	zero_resv_unavail();
	for_each_online_node(nid) {
		struct pglist_data *pgdat = NODE_DATA(nid);
		free_area_init_node(nid, NULL,
				find_min_pfn_for_node(nid), NULL);

		/* Any memory on that node */
		if (pgdat->node_present_pages)
			node_set_state(nid, N_MEMORY);

		check_for_memory(pgdat, nid);
	}
}
