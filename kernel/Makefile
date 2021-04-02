#
# Makefile for the linux kernel.
#

obj-y := panic.o exit.o params.o cpu.o resource.o
obj-y += sched/
obj-y += locking/
obj-y += printk/
obj-y += irq/
obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_GCC_PLUGIN_STACKLEAK) += stackleak.o
obj-$(CONFIG_JUMP_LABEL) += jump_label.o
