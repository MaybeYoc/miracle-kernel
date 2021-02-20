/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <linux/errno.h>

#ifdef __KERNEL__

#include <linux/gfp.h>
#include <linux/page-flags.h>
#include <linux/mmzone.h>

#include <asm/page.h>
#include <asm/pgtable.h>

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

/* test whether an address (unsigned long or pointer) is aligned to PAGE_SIZE */
#define PAGE_ALIGNED(addr)	IS_ALIGNED((unsigned long)(addr), PAGE_SIZE)

#define offset_in_page(p)	((unsigned long)(p) & ~PAGE_MASK)

#ifndef __pa_symbol
#define __pa_symbol(x)  __pa(RELOC_HIDE((unsigned long)(x), 0))
#endif

#ifndef page_to_virt
#define page_to_virt(x)	__va(PFN_PHYS(page_to_pfn(x)))
#endif

#ifndef lm_alias
#define lm_alias(x)	__va(__pa_symbol(x))
#endif

#define page_private(page)		((page)->private)
#define set_page_private(page, v)	((page)->private = (v))

static inline bool page_is_guard(struct page *page) { return false; }

/*
 * The identification function is mainly used by the buddy allocator for
 * determining if two pages could be buddies. We are not really identifying
 * the zone since we could be using the section number id if we do not have
 * node id available in page flags.
 * We only guarantee that it will return the same value for two combinable
 * pages in a zone.
 */
static inline int page_zone_id(struct page *page)
{
	return 0; // TODO
	//return (page->flags >> ZONEID_PGSHIFT) & ZONEID_MASK;
}

#endif /* __KERNEL__ */

#endif /* _LINUX_MM_H */
