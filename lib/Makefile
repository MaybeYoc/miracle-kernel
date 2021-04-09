# SPDX-License-Identifier: GPL-2.0
#
# Makefile for some libs needed in the kernel.
#
obj-y += bitmap.o llist.o list_sort.o kstrtox.o 	\
		hexdump.o div64.o string_helpers.o	\
		memweight.o sort.o find_bit.o kasprintf.o

obj-y += logic_pio.o

obj-$(CONFIG_GENERIC_ATOMIC64) += atomic64.o
obj-$(CONFIG_GENERIC_HWEIGHT) += hweight.o
obj-$(CONFIG_CRC32)	+= crc32.o

lib-y := ctype.o string.o vsprintf.o int_sqrt.o cmdline.o	\
		rbtree.o radix-tree.o xarray.o idr.o irq_regs.o plist.o

ifneq ($(CONFIG_HAVE_DEC_LOCK),y)
lib-y += dec_and_lock.o
endif

lib-$(CONFIG_PRINTK) += dump_stack.o

libfdt_files = fdt.o fdt_ro.o fdt_wip.o fdt_rw.o fdt_sw.o fdt_strerror.o \
	       fdt_empty_tree.o
$(foreach file, $(libfdt_files), \
	$(eval CFLAGS_$(file) = -I$(src)/../scripts/dtc/libfdt))
lib-$(CONFIG_LIBFDT) += $(libfdt_files)

hostprogs-y	:= gen_crc32table
clean-files	:= crc32table.h

$(obj)/crc32.o: $(obj)/crc32table.h

quiet_cmd_crc32 = GEN     $@
      cmd_crc32 = $< > $@

$(obj)/crc32table.h: $(obj)/gen_crc32table
	$(call cmd,crc32)
