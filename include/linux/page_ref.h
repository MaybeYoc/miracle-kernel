/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PAGE_REF_H
#define _LINUX_PAGE_REF_H

#include <linux/atomic.h>
#include <linux/mm_types.h>
#include <linux/page-flags.h>

static inline int page_count(struct page *page)
{
	return atomic_read(&compound_head(page)->_refcount);
}

static inline void set_page_count(struct page *page, int v)
{
	atomic_set(&page->_refcount, v);
}

static inline int page_ref_count(struct page *page)
{
	return atomic_read(&page->_refcount);
}

static inline int page_ref_dec_and_test(struct page *page)
{
	int ret = atomic_dec_and_test(&page->_refcount);

	return ret;
}

static inline void init_page_count(struct page *page)
{
	set_page_count(page, 1);
}

#endif
