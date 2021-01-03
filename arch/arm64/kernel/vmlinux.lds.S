/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <asm-generic/vmlinux.lds.h>

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
		*(.discard)
		*(.discard.*)
		*(.interp .dynamic)
		*(.dynsym .dynstr .hash .gnu.hash)
		*(.eh_frame)
	}

	. = 0x40080000;
	_text = .;

	.text : {
		*(.text)
		*(.text.init)
	}

	. = ALIGN(8);
	.data : {
		*(.data*)
	}

	. = ALIGN(8);
	.bss : {
		*(.bss*)
	}
	_end = .;
}
