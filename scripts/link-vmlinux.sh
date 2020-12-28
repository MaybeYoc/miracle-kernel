#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
#
# link vmlinux
#
# vmlinux is linked from the objects selected by $(KBUILD_VMLINUX_INIT) and
# $(KBUILD_VMLINUX_MAIN) and $(KBUILD_VMLINUX_LIBS). Most are built-in.a files
# from top-level directories in the kernel tree, others are specified in
# arch/$(ARCH)/Makefile. Ordering when linking is important, and
# $(KBUILD_VMLINUX_INIT) must be first. $(KBUILD_VMLINUX_LIBS) are archives
# which are linked conditionally (not within --whole-archive), and do not
# require symbol indexes added.
#
# vmlinux
#   ^
#   |
#   +-< $(KBUILD_VMLINUX_INIT)
#   |   +--< init/version.o + more
#   |
#   +--< $(KBUILD_VMLINUX_MAIN)
#   |    +--< drivers/built-in.a mm/built-in.a + more
#   |
#   +--< $(KBUILD_VMLINUX_LIBS)
#   |    +--< lib/lib.a + more
#   |
#   +-< ${kallsymso} (see description in KALLSYMS section)
#
# vmlinux version (uname -v) cannot be updated during normal
# descending-into-subdirs phase since we do not yet know if we need to
# update vmlinux.
# Therefore this step is delayed until just before final link of vmlinux.
#
# System.map is generated to document addresses of all kernel symbols

# Error out on error
set -e

# Nice output in kbuild format
# Will be supressed by "make -s"
info()
{
	if [ "${quiet}" != "silent_" ]; then
		printf "  %-7s %s\n" ${1} ${2}
	fi
}

# Thin archive build here makes a final archive with symbol table and indexes
# from vmlinux objects INIT and MAIN, which can be used as input to linker.
# KBUILD_VMLINUX_LIBS archives should already have symbol table and indexes
# added.
#
# Traditional incremental style of link does not require this step
#
# built-in.a output file
#
archive_builtin()
{
	info AR built-in.a
	rm -f built-in.a;
	${AR} rcsTP${KBUILD_ARFLAGS} built-in.a			\
				${KBUILD_VMLINUX_INIT}		\
				${KBUILD_VMLINUX_MAIN}
}

# Link of vmlinux
# ${1} - optional extra .o files
# ${2} - output file
vmlinux_link()
{
	local lds="${objtree}/${KBUILD_LDS}"
	local objects

	objects="--whole-archive			\
		built-in.a				\
		--no-whole-archive			\
		--start-group				\
		${KBUILD_VMLINUX_LIBS}			\
		--end-group				\
		${1}"

	${LD} ${KBUILD_LDFLAGS} ${LDFLAGS_vmlinux} -o ${2}	\
		-T ${lds} ${objects}
}

# Create map file with all symbols from ${1}
# See mksymap for additional details
mksysmap()
{
	${CONFIG_SHELL} "${srctree}/scripts/mksysmap" ${1} ${2}
}

# Delete output files in case of error
cleanup()
{
	rm -f .tmp_System.map
	rm -f .tmp_kallsyms*
	rm -f .tmp_vmlinux*
	rm -f built-in.a
	rm -f System.map
	rm -f vmlinux
}

on_exit()
{
	if [ $? -ne 0 ]; then
		cleanup
	fi
}
trap on_exit EXIT

on_signals()
{
	exit 1
}
trap on_signals HUP INT QUIT TERM

#
#
# Use "make V=1" to debug this script
case "${KBUILD_VERBOSE}" in
*1*)
	set -x
	;;
esac

if [ "$1" = "clean" ]; then
	cleanup
	exit 0
fi

# We need access to CONFIG_ symbols
case "${KCONFIG_CONFIG}" in
*/*)
	. "${KCONFIG_CONFIG}"
	;;
*)
	# Force using a file from the current directory
	. "./${KCONFIG_CONFIG}"
esac

# Update version
info GEN .version
if [ -r .version ]; then
	VERSION=$(expr 0$(cat .version) + 1)
	echo $VERSION > .version
else
	rm -f .version
	echo 1 > .version
fi;

# final build of init/
${MAKE} -f "${srctree}/scripts/Makefile.build" obj=init

archive_builtin

kallsymso=""
kallsyms_vmlinux=""

info LD vmlinux
vmlinux_link "${kallsymso}" vmlinux

info SYSMAP System.map
mksysmap vmlinux System.map
