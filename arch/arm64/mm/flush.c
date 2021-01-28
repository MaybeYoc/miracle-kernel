#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/cache.h>

void sync_icache_aliases(void *kaddr, unsigned long len)
{
	unsigned long addr = (unsigned long)kaddr;

	if (icache_is_aliasing()) {
		__clean_dcache_area_pou(kaddr, len);
		__flush_icache_all();
	} else {
		/*
		 * Don't issue kick_all_cpus_sync() after I-cache invalidation
		 * for user mappings.
		 */
		__flush_icache_range(addr, addr + len);
	}
}
