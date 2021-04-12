#include <linux/numa.h>
#include <linux/memblock.h>

void vmemmap_init(void)
{
	int nid;
	size_t size;
	unsigned long virt;
	phys_addr_t phys;

	/* Finally register nodes. */
	for_each_node_mask(nid, &numa_nodes_parsed) {
		unsigned long start_pfn, end_pfn;

		get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);

		size = (end_pfn - start_pfn) * sizeof(struct page);
		virt = (unsigned long)pfn_to_page(start_pfn);
		phys = memblock_phys_alloc(size, PAGE_SIZE);
		map_vmemmap(init_mm.pgd, phys, virt, size);
	}
}