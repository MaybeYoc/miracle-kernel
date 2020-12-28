# SPDX-License-Identifier: GPL-2.0
#
# Kbuild for top-level directory of the kernel
# This file takes care of the following:
# 1) Generate asm-offsets.h

#####
# 1) Generate asm-offsets.h
#

offsets-file := include/generated/asm-offsets.h

always  += $(offsets-file)
targets += arch/$(SRCARCH)/kernel/asm-offsets.s

arch/$(SRCARCH)/kernel/asm-offsets.s:

$(offsets-file): arch/$(SRCARCH)/kernel/asm-offsets.s FORCE
	$(call filechk,offsets,__ASM_OFFSETS_H__)

# Keep these three files during make clean
no-clean-files := $(offsets-file)
