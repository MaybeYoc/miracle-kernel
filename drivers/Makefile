# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel device drivers.
#
# 15 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

obj-y := test_driver.o
obj-y				+= irqchip/
obj-y				+= clocksource/
obj-y				+= firmware/
obj-$(CONFIG_OF)		+= of/
obj-y += tty/
