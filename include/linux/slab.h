/*
 * Written by Mark Hemment, 1996 (markhe@nextd.demon.co.uk).
 *
 * (C) SGI 2006, Christoph Lameter
 * 	Cleaned up and restructured to ease the addition of alternative
 * 	implementations of SLAB allocators.
 * (C) Linux Foundation 2008-2013
 *      Unified interface for all slab allocators
 */

#ifndef _LINUX_SLAB_H
#define	_LINUX_SLAB_H

#include <linux/mmzone.h>
#include <linux/gfp.h>
#include <linux/types.h>
/* TODO */

/*
 * Flags to pass to kmem_cache_create().
 * The ones marked DEBUG are only valid if CONFIG_DEBUG_SLAB is set.
 */
/* DEBUG: Perform (expensive) checks on alloc/free */
#define SLAB_CONSISTENCY_CHECKS	((slab_flags_t __force)0x00000100U)
/* DEBUG: Red zone objs in a cache */
#define SLAB_RED_ZONE		((slab_flags_t __force)0x00000400U)
/* DEBUG: Poison objects */
#define SLAB_POISON		((slab_flags_t __force)0x00000800U)
/* Align objs on cache lines */
#define SLAB_HWCACHE_ALIGN	((slab_flags_t __force)0x00002000U)
/* Use GFP_DMA memory */
#define SLAB_CACHE_DMA		((slab_flags_t __force)0x00004000U)
/* DEBUG: Store the last owner for bug hunting */
#define SLAB_STORE_USER		((slab_flags_t __force)0x00010000U)
/* Panic if kmem_cache_create() fails */
#define SLAB_PANIC		((slab_flags_t __force)0x00040000U)
/*
 * SLAB_TYPESAFE_BY_RCU - **WARNING** READ THIS!
 *
 * This delays freeing the SLAB page by a grace period, it does _NOT_
 * delay object freeing. This means that if you do kmem_cache_free()
 * that memory location is free to be reused at any time. Thus it may
 * be possible to see another object there in the same RCU grace period.
 *
 * This feature only ensures the memory location backing the object
 * stays valid, the trick to using this is relying on an independent
 * object validation pass. Something like:
 *
 *  rcu_read_lock()
 * again:
 *  obj = lockless_lookup(key);
 *  if (obj) {
 *    if (!try_get_ref(obj)) // might fail for free objects
 *      goto again;
 *
 *    if (obj->key != key) { // not the object we expected
 *      put_ref(obj);
 *      goto again;
 *    }
 *  }
 *  rcu_read_unlock();
 *
 * This is useful if we need to approach a kernel structure obliquely,
 * from its address obtained without the usual locking. We can lock
 * the structure to stabilize it and check it's still at the given address,
 * only if we can be sure that the memory has not been meanwhile reused
 * for some other kind of object (which our subsystem's lock might corrupt).
 *
 * rcu_read_lock before reading the address, then rcu_read_unlock after
 * taking the spinlock within the structure expected at that address.
 *
 * Note that SLAB_TYPESAFE_BY_RCU was originally named SLAB_DESTROY_BY_RCU.
 */
/* Defer freeing slabs to RCU */
#define SLAB_TYPESAFE_BY_RCU	((slab_flags_t __force)0x00080000U)
/* Spread some memory over cpuset */
#define SLAB_MEM_SPREAD		((slab_flags_t __force)0x00100000U)
/* Trace allocations and frees */
#define SLAB_TRACE		((slab_flags_t __force)0x00200000U)

/*
 * ZERO_SIZE_PTR will be returned for zero sized kmalloc requests.
 *
 * Dereferencing ZERO_SIZE_PTR will lead to a distinct access fault.
 *
 * ZERO_SIZE_PTR can be passed to kfree though in the same way that NULL can.
 * Both make kfree a no-op.
 */
#define ZERO_SIZE_PTR ((void *)16)

#define ZERO_OR_NULL_PTR(x) ((unsigned long)(x) <= \
				(unsigned long)ZERO_SIZE_PTR)

#endif	/* _LINUX_SLAB_H */
