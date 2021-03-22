/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_MM_DEBUG_H
#define LINUX_MM_DEBUG_H 1

#include <linux/bug.h>
#include <linux/stringify.h>

#define VM_BUG_ON(cond) BUG_ON(cond)
#define VM_BUG_ON_PAGE(cond, page) VM_BUG_ON(cond)
#define VM_BUG_ON_VMA(cond, vma) VM_BUG_ON(cond)
#define VM_BUG_ON_MM(cond, mm) VM_BUG_ON(cond)
#define VM_WARN_ON(cond) (void)WARN_ON(cond)
#define VM_WARN_ON_ONCE(cond) (void)WARN_ON_ONCE(cond)
#define VM_WARN_ONCE(cond, format...) (void)WARN_ONCE(cond, format)
#define VM_WARN(cond, format...) (void)WARN(cond, format)

#define VIRTUAL_BUG_ON(cond) BUG_ON(cond)

#define VM_BUG_ON_PGFLAGS(cond, page) VM_BUG_ON_PAGE(cond, page)

#endif
