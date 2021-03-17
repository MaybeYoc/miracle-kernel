/*
 * Contains CPU feature definitions
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "CPU features: " fmt
#include <linux/compiler.h>
#include <linux/cache.h>
#include <linux/types.h>
#include <linux/init.h>

#include <asm/hwcap.h>

unsigned long elf_hwcap __read_mostly;

#ifdef CONFIG_COMPAT
#define COMPAT_ELF_HWCAP_DEFAULT	\
				(COMPAT_HWCAP_HALF|COMPAT_HWCAP_THUMB|\
				 COMPAT_HWCAP_FAST_MULT|COMPAT_HWCAP_EDSP|\
				 COMPAT_HWCAP_TLS|COMPAT_HWCAP_VFP|\
				 COMPAT_HWCAP_VFPv3|COMPAT_HWCAP_VFPv4|\
				 COMPAT_HWCAP_NEON|COMPAT_HWCAP_IDIV)
unsigned int compat_elf_hwcap __read_mostly = COMPAT_ELF_HWCAP_DEFAULT;
unsigned int compat_elf_hwcap2 __read_mostly;
#endif

static void __init setup_cpufeature(void)
{
	u64 features, block;

	elf_hwcap = 0;

	/*
	 * ID_AA64ISAR0_EL1 contains 4-bit wide signed feature blocks.
	 * The blocks we test below represent incremental functionality
	 * for non-negative values. Negative values are reserved.
	 */
	features = read_cpuid(ID_AA64ISAR0_EL1);
	block = (features >> 4) & 0xf;

	if (!(block & 0x8)) {
		switch (block) {
		default:
		case 2:
			elf_hwcap |= HWCAP_PMULL;
		case 1:
			elf_hwcap |= HWCAP_AES;
		case 0:
			break;
		}
	}

	block = (features >> 8) & 0xf;
	if (block && !(block & 0x8))
		elf_hwcap |= HWCAP_SHA1;

	block = (features >> 12) & 0xf;
	if (block && !(block & 0x8))
		elf_hwcap |= HWCAP_SHA2;

	block = (features >> 16) & 0xf;
	if (block && !(block & 0x8))
		elf_hwcap |= HWCAP_CRC32;

#ifdef CONFIG_COMPAT
	/*
	 * ID_ISAR5_EL1 carries similar information as above, but pertaining to
	 * the Aarch32 32-bit execution state.
	 */
	features = read_cpuid(ID_ISAR5_EL1);
	block = (features >> 4) & 0xf;
	if (!(block & 0x8)) {
		switch (block) {
		default:
		case 2:
			compat_elf_hwcap2 |= COMPAT_HWCAP2_PMULL;
		case 1:
			compat_elf_hwcap2 |= COMPAT_HWCAP2_AES;
		case 0:
			break;
		}
	}

	block = (features >> 8) & 0xf;
	if (block && !(block & 0x8))
		compat_elf_hwcap2 |= COMPAT_HWCAP2_SHA1;

	block = (features >> 12) & 0xf;
	if (block && !(block & 0x8))
		compat_elf_hwcap2 |= COMPAT_HWCAP2_SHA2;

	block = (features >> 16) & 0xf;
	if (block && !(block & 0x8))
		compat_elf_hwcap2 |= COMPAT_HWCAP2_CRC32;
#endif
}
arch_initcall(setup_cpufeature);
