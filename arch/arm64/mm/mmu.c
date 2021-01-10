/*
 * We need to be able to catch inadvertent references to memstart_addr
 * that occur (potentially in generic code) before arm64_memblock_init()
 * executes, which assigns it its actual value. So use a default value
 * that cannot be mistaken for a real physical address.
 */
#include <linux/types.h>
#include <linux/cache.h>
#include <asm/memory.h>
#include <asm/pgtable-hwdef.h>

u64 idmap_t0sz = TCR_T0SZ(VA_BITS);
u64 idmap_ptrs_per_pgd = PTRS_PER_PGD;
u64 vabits_user __ro_after_init;
u64 kimage_voffset __ro_after_init;
