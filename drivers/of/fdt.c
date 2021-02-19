// SPDX-License-Identifier: GPL-2.0
/*
 * Functions for working with the Flattened Device Tree data format
 *
 * Copyright 2009 Benjamin Herrenschmidt, IBM Corp
 * benh@kernel.crashing.org
 */

#define pr_fmt(fmt)	"OF: fdt: " fmt

#include <linux/crc32.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/libfdt.h>
#include <linux/of.h>

#include <asm/setup.h>  /* for COMMAND_LINE_SIZE */
#include <asm/page.h>

#include "of_private.h"

/* Everything below here references initial_boot_params directly. */
int __initdata dt_root_addr_cells;
int __initdata dt_root_size_cells;

void *initial_boot_params;

#ifdef CONFIG_OF_EARLY_FLATTREE

static u32 of_fdt_crc32;

bool __init early_init_dt_verify(void *params)
{
	if (!params)
		return false;
	
	/* check device tree validity */
	if (fdt_check_header(params))
		return false;
	
	/* Setup flat device-tree pointer */
	initial_boot_params = params;
	of_fdt_crc32 = crc32_be(~0, initial_boot_params,
				fdt_totalsize(initial_boot_params));
	return true;
}

/**
 * of_scan_flat_dt - scan flattened tree blob and call callback on each.
 * @it: callback function
 * @data: context data pointer
 *
 * This function is used to scan the flattened device-tree, it is
 * used to extract the memory information at boot before we can
 * unflatten the tree
 */
int __init of_scan_flat_dt(int (*it)(unsigned long node,
				     const char *uname, int depth,
				     void *data),
			   void *data)
{
	const void *blob = initial_boot_params;
	const char *pathp;
	int offset, rc = 0, depth = -1;

	if (!blob)
		return 0;

	for (offset = fdt_next_node(blob, -1, &depth);
	     offset >= 0 && depth >= 0 && !rc;
	     offset = fdt_next_node(blob, offset, &depth)) {

		pathp = fdt_get_name(blob, offset, NULL);
		if (*pathp == '/')
			pathp = kbasename(pathp);
		rc = it(offset, pathp, depth, data);
	}
	return rc;
}

/**
 * of_get_flat_dt_root - find the root node in the flat blob
 */
unsigned long __init of_get_flat_dt_root(void)
{
	return 0;
}

/**
 * of_get_flat_dt_prop - Given a node in the flat blob, return the property ptr
 *
 * This function can be used within scan_flattened_dt callback to get
 * access to properties
 */
const void *__init of_get_flat_dt_prop(unsigned long node, const char *name,
				       int *size)
{
	return fdt_getprop(initial_boot_params, node, name, size);
}

const char * __init of_flat_dt_get_machine_name(void)
{
	const char *name;
	unsigned long dt_root = of_get_flat_dt_root();

	name = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!name)
		name = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	return name;
}

#ifdef CONFIG_BLK_DEV_INITRD
unsigned long initrd_start, initrd_end;
int initrd_below_start_ok;

phys_addr_t phys_initrd_start __initdata;
unsigned long phys_initrd_size __initdata;

static void __early_init_dt_declare_initrd(unsigned long start,
					   unsigned long end)
{
	/* ARM64 would cause a BUG to occur here when CONFIG_DEBUG_VM is
	 * enabled since __va() is called too early. ARM64 does make use
	 * of phys_initrd_start/phys_initrd_size so we can skip this
	 * conversion.
	 */
	if (!IS_ENABLED(CONFIG_ARM64)) {
		initrd_start = (unsigned long)__va(start);
		initrd_end = (unsigned long)__va(end);
		initrd_below_start_ok = 1;
	}
}

/**
 * early_init_dt_check_for_initrd - Decode initrd location from flat tree
 * @node: reference to node containing initrd location ('chosen')
 */
static void __init early_init_dt_check_for_initrd(unsigned long node)
{
	u64 start, end;
	int len;
	const __be32 *prop;

	pr_debug("Looking for initrd properties... ");

	prop = of_get_flat_dt_prop(node, "linux,initrd-start", &len);
	if (!prop)
		return;
	start = of_read_number(prop, len/4);

	prop = of_get_flat_dt_prop(node, "linux,initrd-end", &len);
	if (!prop)
		return;
	end = of_read_number(prop, len/4);

	__early_init_dt_declare_initrd(start, end);
	phys_initrd_start = start;
	phys_initrd_size = end - start;

	pr_debug("initrd_start=0x%llx  initrd_end=0x%llx\n",
		 (unsigned long long)start, (unsigned long long)end);
}
#else
static inline void early_init_dt_check_for_initrd(unsigned long node)
{
}
#endif /* CONFIG_BLK_DEV_INITRD */

#ifndef MIN_MEMBLOCK_ADDR
#define MIN_MEMBLOCK_ADDR	__pa(PAGE_OFFSET)
#endif
#ifndef MAX_MEMBLOCK_ADDR
#define MAX_MEMBLOCK_ADDR	((phys_addr_t)~0)
#endif

void __init __weak early_init_dt_add_memory_arch(u64 base, u64 size)
{
	const u64 phys_offset = MIN_MEMBLOCK_ADDR;

	if (size < PAGE_SIZE - (base & ~PAGE_MASK)) {
		pr_warn("Ignoring memory block 0x%llx - 0x%llx\n",
			base, base + size);
		return;
	}

	if (!PAGE_ALIGNED(base)) {
		size -= PAGE_SIZE - (base & ~PAGE_MASK);
		base = PAGE_ALIGN(base);
	}
	size &= PAGE_MASK;

	if (base > MAX_MEMBLOCK_ADDR) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n",
				base, base + size);
		return;
	}

	if (base + size - 1 > MAX_MEMBLOCK_ADDR) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n",
				((u64)MAX_MEMBLOCK_ADDR) + 1, base + size);
		size = MAX_MEMBLOCK_ADDR - base + 1;
	}

	if (base + size < phys_offset) {
		pr_warning("Ignoring memory block 0x%llx - 0x%llx\n",
			   base, base + size);
		return;
	}
	if (base < phys_offset) {
		pr_warning("Ignoring memory range 0x%llx - 0x%llx\n",
			   base, phys_offset);
		size -= phys_offset - base;
		base = phys_offset;
	}
	memblock_add(base, size);
}

int __init __weak early_init_dt_mark_hotplug_memory_arch(u64 base, u64 size)
{
	return memblock_mark_hotplug(base, size);
}

u64 __init dt_mem_next_cell(int s, const __be32 **cellp)
{
	const __be32 *p = *cellp;

	*cellp = p + s;
	return of_read_number(p, s);
}

/**
 * early_init_dt_scan_memory - Look for and parse memory nodes
 */
int __init early_init_dt_scan_memory(unsigned long node, const char *uname,
				     int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	const __be32 *reg, *endp;
	int l;
	bool hotpluggable;

	/* We are scanning "memory" nodes only */
	if (type == NULL || strcmp(type, "memory") != 0)
		return 0;

	reg = of_get_flat_dt_prop(node, "linux,usable-memory", &l);
	if (reg == NULL)
		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	hotpluggable = of_get_flat_dt_prop(node, "hotpluggable", NULL);

	pr_debug("memory scan node %s, reg size %d,\n", uname, l);

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
		pr_debug(" - %llx ,  %llx\n", (unsigned long long)base,
		    (unsigned long long)size);

		early_init_dt_add_memory_arch(base, size);

		if (!hotpluggable)
			continue;

		if (early_init_dt_mark_hotplug_memory_arch(base, size))
			pr_warn("failed to mark hotplug range 0x%llx - 0x%llx\n",
				base, base + size);
	}

	return 0;
}

int __init early_init_dt_scan_chosen(unsigned long node, const char *uname,
				     int depth, void *data)
{
	int l;
	const char *p;

	pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

	if (depth != 1 || !data ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	early_init_dt_check_for_initrd(node);

	/* Retrieve command line */
	p = of_get_flat_dt_prop(node, "bootargs", &l);
	if (p != NULL && l > 0)
		strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));

	/*
	 * CONFIG_CMDLINE is meant to be a default in case nothing else
	 * managed to set the command line, unless CONFIG_CMDLINE_FORCE
	 * is set in which case we override whatever was found earlier.
	 */
#ifdef CONFIG_CMDLINE
#if defined(CONFIG_CMDLINE_EXTEND)
	strlcat(data, " ", COMMAND_LINE_SIZE);
	strlcat(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#elif defined(CONFIG_CMDLINE_FORCE)
	strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#else
	/* No arguments from boot loader, use kernel's  cmdl*/
	if (!((char *)data)[0])
		strlcpy(data, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
#endif
#endif /* CONFIG_CMDLINE */

	pr_debug("Command line is: %s\n", (char*)data);

	/* break now */
	return 1;
}

/**
 * early_init_dt_scan_root - fetch the top level address and size cells
 */
int __init early_init_dt_scan_root(unsigned long node, const char *uname,
				   int depth, void *data)
{
	const __be32 *prop;

	if (depth != 0)
		return 0;

	dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
	dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

	prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
	if (prop)
		dt_root_size_cells = be32_to_cpup(prop);
	pr_debug("dt_root_size_cells = %x\n", dt_root_size_cells);

	prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
	if (prop)
		dt_root_addr_cells = be32_to_cpup(prop);
	pr_debug("dt_root_addr_cells = %x\n", dt_root_addr_cells);

	/* break now */
	return 1;
}

void __init early_init_dt_scan_nodes(void)
{
	int rc = 0;

	/* Retrieve various information from the /chosen node */
	rc = of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
	if (!rc)
		pr_warn("No chosen node found, continuing without\n");

	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);

	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);
}

bool __init early_init_dt_scan(void *params)
{
	bool status;

	status = early_init_dt_verify(params);
	if (!status)
		return false;

	early_init_dt_scan_nodes();
	return true;
}

#endif /* CONFIG_OF_EARLY_FLATTREE */
