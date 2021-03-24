# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux memory manager.
#

obj-y := page_alloc.o memory.o mmzone.o percpu.o

obj-y += memblock.o
obj-y += init_mm.o
obj-y += early_ioremap.o

mmu-$(CONFIG_MMU) := vmalloc.o
obj-y += $(mmu-y)

obj-y += slub.o
obj-$(CONFIG_MEMTEST)		+= memtest.o
