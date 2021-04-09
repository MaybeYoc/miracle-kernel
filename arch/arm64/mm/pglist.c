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

cpumask_t * node_to_cpumask_map[MAX_NUMNODES];

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
nodemask_t numa_nodes_parsed __initdata;

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

static int __init numa_register_nodes(void)
{
	int nid;
	size_t size;
	struct memblock_region *mblk;
	unsigned long virt;
	phys_addr_t phys;

	/* Check that valid nid is set to memblks */
	for_each_memblock(memory, mblk)
		if (mblk->nid == NUMA_NO_NODE || mblk->nid >= MAX_NUMNODES) {
			pr_warn("Warning: invalid memblk node %d [mem %#010Lx-%#010Lx]\n",
				mblk->nid, mblk->base,
				mblk->base + mblk->size - 1);
			return -EINVAL;
		}

	/* Finally register nodes. */
	for_each_node_mask(nid, &numa_nodes_parsed) {
		unsigned long start_pfn, end_pfn;

		get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
		setup_node_data(nid, start_pfn, end_pfn);
		node_set_online(nid);

		size = (end_pfn - start_pfn) * sizeof(struct page);
		virt = (unsigned long)pfn_to_page(start_pfn);
		phys = memblock_phys_alloc(size, PAGE_SIZE);
		map_vmemmap(init_mm.pgd, phys, virt, size);
	}

	/* Setup online nodes to actual nodes*/
	__node_possible_mask = numa_nodes_parsed;

	return 0;
}

static int __init numa_init(int (*init_func)(void))
{
	int ret;

	//nodes_clear(numa_nodes_parsed);
	//nodes_clear(node_possible_map);
	//nodes_clear(node_online_map);

	ret = init_func();
	if (ret < 0)
		return ret;

	ret = numa_register_nodes();
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * numa_add_memblk - Set node id to memblk
 * @nid: NUMA node ID of the new memblk
 * @start: Start address of the new memblk
 * @end:  End address of the new memblk
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init numa_add_memblk(int nid, u64 start, u64 end)
{
	int ret;

	ret = memblock_set_node(start, (end - start), &memblock.memory, nid);
	if (ret < 0) {
		pr_err("memblock [0x%llx - 0x%llx] failed to add on node %d\n",
			start, (end - 1), nid);
		return ret;
	}

	nodemask_set_node(nid, &numa_nodes_parsed);
	return ret;
}

/**
 * dummy_numa_init - Fallback dummy NUMA init
 *
 * Used if there's no underlying NUMA architecture, NUMA initialization
 * fails, or NUMA is disabled on the command line.
 *
 * Must online at least one node (node 0) and add memory blocks that cover all
 * allowed memory. It is unlikely that this function fails.
 */
static int __init dummy_numa_init(void)
{
	int ret;
	struct memblock_region *mblk;

	pr_info("NUMA disabled\n"); /* Forced off on command line. */
	pr_info("Faking a node at [mem %#018Lx-%#018Lx]\n",
		memblock_start_of_DRAM(), memblock_end_of_DRAM() - 1);

	for_each_memblock(memory, mblk) {
		ret = numa_add_memblk(0, mblk->base, mblk->base + mblk->size);
		if (!ret)
			continue;

		pr_err("NUMA init failed\n");
		return ret;
	}

	return 0;
}

/**
 * arm64_numa_init - Initialize NUMA
 *
 * Try each configured NUMA initialization method until one succeeds.  The
 * last fallback is dummy single node config encomapssing whole memory.
 */
void __init arm64_numa_init(void)
{
	numa_init(dummy_numa_init);
}
