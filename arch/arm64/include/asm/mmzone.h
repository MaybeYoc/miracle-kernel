/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_MMZONE_H
#define __ASM_MMZONE_H

#include <linux/init.h>

#include <asm/numa.h>

extern struct pglist_data *node_data[];
#define NODE_DATA(nid)		(node_data[(nid)])

extern void __init map_vmemmap(pgd_t *pgdp, phys_addr_t phys, 
								unsigned long virt, size_t size);
#endif /* __ASM_MMZONE_H */
