# SPDX-License-Identifier: GPL-2.0
###
# scripts contains sources for various helper programs used throughout
# the kernel for the build process.
# ---------------------------------------------------------------------------
# kallsyms:      Find all symbols in vmlinux
# pnmttologo:    Convert pnm files to logo files
# conmakehash:   Create chartable
# conmakehash:	 Create arrays for initializing the kernel console tables

HOST_EXTRACFLAGS += -I$(srctree)/tools/include

HOSTCFLAGS_sortextable.o = -I$(srctree)/tools/include

always		:= $(hostprogs-y) $(hostprogs-m)

# The following hostprogs-y programs are only build on demand
hostprogs-y += unifdef

# These targets are used internally to avoid "is up to date" messages
PHONY += build_unifdef
build_unifdef: $(obj)/unifdef
	@:

subdir-$(CONFIG_GCC_PLUGINS) += gcc-plugins
subdir-$(CONFIG_GDB_SCRIPTS) += gdb

# Let clean descend into subdirs
subdir-	+= basic dtc kconfig
