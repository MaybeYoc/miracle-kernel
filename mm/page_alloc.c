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
#include <linux/mmzone.h>
#include <linux/mm_types.h>
#include <linux/kernel.h>
#include <linux/pfn.h>

#include "internal.h"

gfp_t gfp_allowed_mask __read_mostly = GFP_BOOT_MASK;

static char * const zone_names[MAX_NR_ZONES] = {
	"DMA",
	"Normal",
	"Movable",
};

const char * const migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Movable",
	"Reclaimable",
	"HighAtomic",
};

static inline int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	return 0;
}

static inline bool set_page_guard(struct zone *zone, struct page *page,
			unsigned int order, int migratetype)
{
	return false;
}

static inline void clear_page_guard(struct zone *zone, struct page *page,
				unsigned int order, int migratetype)
{

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
		;
		//__mod_zone_freepage_state(zone, 1 << order, migratetype); TODO

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
			//buddy_mt = get_pageblock_migratetype(buddy);
			buddy_mt = 0; // TODO
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

static void __meminit init_currently_empty_zone(struct zone *zone,
					unsigned long zone_start_pfn,
					unsigned long size)
{
	/* TODO */
}

static void __init free_area_init_core(struct pglist_data *pgdat)
{
	enum zone_type j;

	for (j = 0; j < MAX_NR_ZONES; j++) {
		struct zone *zone = pgdat->node_zones + j;
		unsigned long size;
		unsigned long zone_start_pfn = zone->zone_start_pfn;

		size = zone->zone_total_pages;
		if (!size)
			continue;

		init_currently_empty_zone(zone, zone_start_pfn, size);
	}
}

void __init free_area_init_node(int nid, unsigned long *max_zone_pfn)
{
	unsigned long pfn, start_pfn, min_pfn, total_pages;
	struct pglist_data *pgdat = NODE_DATA(nid);
	int i;

	min_pfn = find_min_pfn_for_node(nid);
	start_pfn = pgdat->node_start_pfn;
	BUG_ON(min_pfn != start_pfn);
	total_pages = pgdat->node_total_pages;
	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = &pgdat->node_zones[i];
		zone->name = zone_names[i];
		zone->node = nid;
		zone->zone_pgdat = pgdat;
		zone->type = i;

		if (max_zone_pfn[i]) {
			zone->zone_start_pfn = start_pfn;
			zone->zone_total_pages = max_zone_pfn[i];
			for (pfn = start_pfn; pfn < start_pfn + max_zone_pfn[i]; pfn++) {
				set_page_zone(pfn_to_page(pfn), i);
				set_page_node(pfn_to_page(pfn), nid);
			}
			start_pfn += max_zone_pfn[i];
			total_pages -= max_zone_pfn[i];
		}
	}
	BUG_ON(total_pages);

	free_area_init_core(pgdat);
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
	struct pglist_data *pgdat;

	/* TODO
	 * Don't need removable and dma pages NOW!
	 */
	for_each_online_pgdat(pgdat) {
		max_zone_pfn[ZONE_DMA] = 0;
		max_zone_pfn[ZONE_NORMAL] = pgdat->node_total_pages;
		max_zone_pfn[ZONE_MOVABLE] = 0;
		free_area_init_node(pgdat->node_id, max_zone_pfn);
	}
		
}
