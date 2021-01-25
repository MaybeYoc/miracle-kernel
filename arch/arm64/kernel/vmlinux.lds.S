/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

OUTPUT_ARCH(aarch64)
ENTRY(_text)
SECTIONS
{
	.text : {
		*(.text)
		*(.text.*)
	}

	.data : {
		*(.data)
		*(.rodata)
	}

	.bss : {
		*(.bss)
	}
}

#if 0
#include <asm-generic/vmlinux.lds.h>
#include <linux/sizes.h>
#include <asm/kernel-pgtable.h>
#include <asm/memory.h>
#include <asm/cache.h>

#include "image.h"

/* .exit.text needed in case of alternative patching */
#define ARM_EXIT_KEEP(x)	x
#define ARM_EXIT_DISCARD(x)

OUTPUT_ARCH(aarch64)
ENTRY(_text)

#define IDMAP_TEXT					\
	. = ALIGN(SZ_4K);				\
	__idmap_text_start = .;				\
	*(.idmap.text)					\
	__idmap_text_end = .;

SECTIONS
{
	/*
	 * XXX: The linker does not define how output sections are
	 * assigned to input sections when there are multiple statements
	 * matching the same input section name.  There is no documented
	 * order of matching.
	 */
	/DISCARD/ : {
		ARM_EXIT_DISCARD(EXIT_TEXT)
		ARM_EXIT_DISCARD(EXIT_DATA)
		*(.discard)
		*(.discard.*)
		*(.interp .dynamic)
		*(.dynsym .dynstr .hash .gnu.hash)
		*(.eh_frame)
	}

	. = KIMAGE_VADDR + TEXT_OFFSET;

	__init_begin = .;
	.head.text : {
		_text = .;
		HEAD_TEXT
	}

	.text : {		/* Real text segment */
		_stext = .; /* Text and read-only data	*/
		__exception_text_start = .;
		*(.exception.text)
		__exception_text_end = .;
		IRQENTRY_TEXT
		ENTRY_TEXT
		TEXT_TEXT
		IDMAP_TEXT
		*(.fixup)
		*(.gnu.warning)
		. = ALIGN(16);
		*(.got)		/* Global offset table */
	}

	. = ALIGN(SEGMENT_ALIGN);
	_etext = .;			/* End of text section */

	RO_DATA(PAGE_SIZE) /* everything from this point to */
	EXCEPTION_TABLE(8)		/* __init_begin will be marked RO NX */
	NOTES

	. = ALIGN(PAGE_SIZE);
	idmap_pg_dir = .;
	. += IDMAP_DIR_SIZE;

	swapper_pg_dir = .;
	. += PAGE_SIZE;
	swapper_pg_end = .;

	. = ALIGN(SEGMENT_ALIGN);
	__init_begin = .;
	__inittext_begin = .;

	INIT_TEXT_SECTION(8)
	.exit.text : {
		ARM_EXIT_KEEP(EXIT_TEXT)
	}

	. = ALIGN(4);
	.altinstructions : {
		__alt_instructions = .;
		*(.altinstructions)
		__alt_instructions_end = .;
	}
	.altinstr_replacement : {
		*(.altinstr_replacement)
	}

	. = ALIGN(PAGE_SIZE);
	__inittext_end = .;
	__initdata_begin = .;

	.init.data : {
		INIT_DATA
		INIT_SETUP(16)
		INIT_CALLS
		CON_INITCALL
	}
	.exit.data : {
		ARM_EXIT_KEEP(EXIT_DATA)
	}

	PERCPU_SECTION(L1_CACHE_BYTES)

	.rela.dyn : ALIGN(8) {
		*(.rela .rela*)
	}

	__rela_offset	= ABSOLUTE(ADDR(.rela.dyn) - KIMAGE_VADDR);
	__rela_size	= SIZEOF(.rela.dyn);

	. = ALIGN(SEGMENT_ALIGN);
	__initdata_end = .;
	__init_end = .;

	_data = .;
	_sdata = .;
	RW_DATA_SECTION(L1_CACHE_BYTES, PAGE_SIZE, THREAD_ALIGN)

	/*
	 * Data written with the MMU off but read with the MMU on requires
	 * cache lines to be invalidated, discarding up to a Cache Writeback
	 * Granule (CWG) of data from the cache. Keep the section that
	 * requires this type of maintenance to be in its own Cache Writeback
	 * Granule (CWG) area so the cache maintenance operations don't
	 * interfere with adjacent data.
	 */
	.mmuoff.data.write : ALIGN(SZ_2K) {
		__mmuoff_data_start = .;
		*(.mmuoff.data.write)
	}
	. = ALIGN(SZ_2K);
	.mmuoff.data.read : {
		*(.mmuoff.data.read)
		__mmuoff_data_end = .;
	}
	_edata = .;

	BSS_SECTION(0, 0, 0)

	. = ALIGN(PAGE_SIZE);
	init_pg_dir = .;
	. += INIT_DIR_SIZE;
	init_pg_end = .;
	_end = .;

	STABS_DEBUG
	DWARF_DEBUG
	HEAD_SYMBOLS
}

ASSERT(__idmap_text_end - (__idmap_text_start & ~(SZ_4K - 1)) <= SZ_4K,
	"ID map text too big or misaligned")
/*
 * If padding is applied before .head.text, virt<->phys conversions will fail.
 */
ASSERT(_text == (KIMAGE_VADDR + TEXT_OFFSET), "HEAD is misaligned")
#endif
