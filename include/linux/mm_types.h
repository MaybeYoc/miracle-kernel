/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/rbtree.h>
#include <linux/log2.h>
#include <linux/spinlock.h>

#include <asm/mmu.h>
#include <asm/page.h>

/*
 * Each physical page in the system has a struct page associated with
 * it to keep track of whatever it is we are using the page for at the
 * moment. Note that we have no way to track which tasks are using
 * a page, though if it is a pagecache page, rmap structures can tell us
 * who is mapping it.
 *
 * If you allocate the page using alloc_pages(), you can use some of the
 * space in struct page for your own purposes.  The five words in the main
 * union are available, except for bit 0 of the first word which must be
 * kept clear.  Many users use this word to store a pointer to an object
 * which is guaranteed to be aligned.  If you use the same storage as
 * page->mapping, you must restore it to NULL before freeing the page.
 *
 * If your page will not be mapped to userspace, you can also use the four
 * bytes in the mapcount union, but you must call page_mapcount_reset()
 * before freeing it.
 *
 * If you want to use the refcount field, it must be used in such a way
 * that other CPUs temporarily incrementing and then decrementing the
 * refcount does not cause problems.  On receiving the page from
 * alloc_pages(), the refcount will be positive.
 *
 * If you allocate pages of order > 0, you can use some of the fields
 * in each subpage, but you may need to restore some of their values
 * afterwards.
 *
 * SLUB uses cmpxchg_double() to atomically update its freelist and
 * counters.  That requires that freelist & counters be adjacent and
 * double-word aligned.  We align all struct pages to double-word
 * boundaries, and ensure that 'freelist' is aligned within the
 * struct.
 */
#ifdef CONFIG_HAVE_ALIGNED_STRUCT_PAGE
#define _struct_page_alignment	__aligned(2 * sizeof(unsigned long))
#else
#define _struct_page_alignment
#endif

struct page {
	unsigned long flags;		/* Atomic flags, some possibly
					 * updated asynchronously */
	struct list_head lru;
	/* TODO See page-flags.h for PAGE_MAPPING_FLAGS */
	//struct address_space *mapping;
	void *mapping;
	
	pgoff_t index;		/* Our offset within mapping. */
	unsigned long private;

	struct kmem_cache *slab_cache; /* not slob */

	struct {	/* Tail pages of compound page */
		unsigned long compound_head;	/* Bit zero is set */

		/* First tail page only */
		unsigned char compound_dtor;
		unsigned char compound_order;
		atomic_t compound_mapcount;
	};
	
	struct {	/* Second tail page of compound page */
		unsigned long _compound_pad_1;	/* compound_head */
		unsigned long _compound_pad_2;
		struct list_head deferred_list;
	};
	struct {	/* Page table pages */
		unsigned long _pt_pad_1;	/* compound_head */
		pgtable_t pmd_huge_pte; /* protected by page->ptl */
		unsigned long _pt_pad_2;	/* mapping */
		union {
			struct mm_struct *pt_mm; /* x86 pgds only */
			atomic_t pt_frag_refcount; /* powerpc */
		};
		spinlock_t ptl;
	};

	void *freelist;		/* first free object */

	union {
		unsigned long counters;		/* SLUB */
		struct {			/* SLUB */
			unsigned inuse:16;
			unsigned objects:15;
			unsigned frozen:1;
		};
	};

	void *virtual;			/* Kernel virtual address (NULL if
					   not kmapped, ie. highmem) */
	/*
		* If the page is neither PageSlab nor mappable to userspace,
		* the value stored here may help determine what this page
		* is used for.  See page-flags.h for a list of page types
		* which are currently stored here.
		*/
	unsigned int page_type;

	/* Usage count. *DO NOT USE DIRECTLY*. See page_ref.h */
	atomic_t _refcount;

}  _struct_page_alignment;

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */

	unsigned long vm_start;		/* Our start address within vm_mm. */
	unsigned long vm_end;		/* The first byte after our end address
					   within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

	struct rb_node vm_rb;

	struct mm_struct *vm_mm;	/* The address space we belong to. */
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */
	unsigned long vm_flags;		/* Flags, see mm.h. */
} __randomize_layout;

struct mm_struct {
	struct {
		struct vm_area_struct *mmap;	/* list of VMAs */
		struct rb_root mm_rb;
		unsigned long mmap_base;	/* base of mmap area */
		unsigned long mmap_legacy_base;	/* base of mmap area in bottom-up allocations */

		unsigned long task_size;	/* size of task vm space */
		unsigned long highest_vm_end;	/* highest vma end address */
		pgd_t * pgd;

		/**
		 * @mm_users: The number of users including userspace.
		 *
		 * Use mmget()/mmget_not_zero()/mmput() to modify. When this
		 * drops to 0 (i.e. when the task exits and there are no other
		 * temporary reference holders), we also release a reference on
		 * @mm_count (which may then free the &struct mm_struct if
		 * @mm_count also drops to 0).
		 */
		atomic_t mm_users;

		/**
		 * @mm_count: The number of references to &struct mm_struct
		 * (@mm_users count as 1).
		 *
		 * Use mmgrab()/mmdrop() to modify. When this drops to 0, the
		 * &struct mm_struct is freed.
		 */
		atomic_t mm_count;

		int map_count;			/* number of VMAs */

		spinlock_t page_table_lock; /* Protects page tables and some
							* counters
							*/

		struct list_head mmlist; /* List of maybe swapped mm's.	These
						* are globally strung together off
						* init_mm.mmlist, and are protected
						* by mmlist_lock
						*/


		unsigned long hiwater_rss; /* High-watermark of RSS usage */
		unsigned long hiwater_vm;  /* High-water virtual memory usage */

		unsigned long total_vm;	   /* Total pages mapped */

		spinlock_t arg_lock; /* protect the below fields */
		unsigned long start_code, end_code, start_data, end_data;
		unsigned long start_brk, brk, start_stack;
		unsigned long arg_start, arg_end, env_start, env_end;

		/* Architecture-specific MM context */
		mm_context_t context;

		unsigned long flags; /* Must use atomic bitops to access */
	} __randomize_layout;

	/*
	 * The mm_cpumask needs to be at the end of mm_struct, because it
	 * is dynamically sized based on nr_cpu_ids.
	 */
	unsigned long cpu_bitmap[];
};

extern struct mm_struct init_mm;

/*
 * Used for sizing the vmemmap region on some architectures
 */
#define STRUCT_PAGE_MAX_SHIFT	(order_base_2(sizeof(struct page)))

#endif /* _LINUX_MM_TYPES_H */
