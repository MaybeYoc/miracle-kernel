/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/memblock.h>

#include <asm/memory.h>
#include <asm/sections.h>

u64 test_addr;
u64 aa = 0;
u32 bb = 64;
phys_addr_t new_addr;
phys_addr_t new_addr2;
phys_addr_t new_addr3;
void *virt_addr;
asmlinkage __visible void __init start_kernel(void)
{
	
	memblock_add(0x40000000, 0x50000);
	memblock_add(0x80000000, 0x50000);
	memblock_add(0x40005000, 0x50000);
	memblock_add(0x60000000, 0x40000);
	memblock_remove(0x60000000, 0x3000);
	memblock_reserve(0x40006000, 0x100);
	memblock_free(0x40006050, 0x20);
	if (memblock_is_region_reserved(0x50000000, 0x200) == true)
		aa = 1;

	new_addr = memblock_phys_alloc(0x300, 4096);
	new_addr2 = memblock_phys_alloc(0x300, 4096);
	test_addr = 1;
	memblock_free(new_addr, 4096);
	new_addr3 = memblock_phys_alloc(0x300, 4096);
	test_addr = 3;
}
