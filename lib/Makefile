# SPDX-License-Identifier: GPL-2.0
#
# Makefile for some libs needed in the kernel.
#
obj-y += bitmap.o find_bit.o llist.o list_sort.o kstrtox.o hexdump.o

# TODO
obj-$(CONFIG_GENERIC_ATOMIC64) += atomic64.o
obj-$(CONFIG_GENERIC_HWEIGHT) += hweight.o

lib-y := ctype.o string.o vsprintf.o int_sqrt.o

libfdt_files = fdt.o fdt_ro.o fdt_wip.o fdt_rw.o fdt_sw.o fdt_strerror.o \
	       fdt_empty_tree.o
$(foreach file, $(libfdt_files), \
	$(eval CFLAGS_$(file) = -I$(src)/../scripts/dtc/libfdt))
lib-$(CONFIG_LIBFDT) += $(libfdt_files)
