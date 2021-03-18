// SPDX-License-Identifier: GPL-2.0+
/*
 * pglist
 */
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/mmzone.h>

#include <asm/memory.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;

static void __init setup_node_data(int nid, unsigned long start_pfn, unsigned long end_pfn)
{
	const size_t nd_size = roundup(sizeof(struct pglist_data), SMP_CACHE_BYTES);
	unsigned long nd_pa;
	void *nd;

	if (start_pfn >= end_pfn)
		pr_info("Initmem setup node %d [<memory-less node>]\n", nid);
	
	nd_pa = memblock_phys_alloc_try_nid(nd_size, SMP_CACHE_BYTES, nid);
	nd = __va(nd_pa);

	node_data[nid] = nd;
	memset(NODE_DATA(nid), 0, sizeof(struct pglist_data));
	NODE_DATA(nid)->node_id = nid;
	NODE_DATA(nid)->node_start_pfn = start_pfn;
	NODE_DATA(nid)->node_spanned_pages = end_pfn - start_pfn;
}

void __init zone_vmemmap_init(void)
{
	int i, nid;
	unsigned long start_pfn, end_pfn, total_pfn_size, virt;
	phys_addr_t phys;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		if (nid == NUMA_NO_NODE || nid >= MAX_NUMNODES)
			nid = 0;

		setup_node_data(nid, start_pfn, end_pfn);

		total_pfn_size = (end_pfn - start_pfn) * sizeof(struct page);
		virt = (unsigned long)pfn_to_page(start_pfn);
		phys = memblock_phys_alloc(total_pfn_size, PAGE_SIZE);
		map_vmemmap(init_mm.pgd, phys, virt, total_pfn_size);
		memset((void *)virt, 0, total_pfn_size);
	}
}
