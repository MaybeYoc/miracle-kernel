/*
 * Copyright (C) 2014 Linaro Ltd. <ard.biesheuvel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_CPUFEATURE_H
#define __ASM_CPUFEATURE_H

#include <asm/cpucaps.h>
#include <asm/cputype.h>
#include <asm/hwcap.h>
#include <asm/sysreg.h>

/*
 * In the arm64 world (as in the ARM world), elf_hwcap is used both internally
 * in the kernel and for user space to keep track of which optional features
 * are supported by the current system. So let's map feature 'x' to HWCAP_x.
 * Note that HWCAP_x constants are bit fields so we need to take the log.
 */

#define MAX_CPU_FEATURES	(8 * sizeof(elf_hwcap))
#define cpu_feature(x)		ilog2(HWCAP_ ## x)

#ifndef __ASSEMBLY__

//#include <linux/kernel.h>

/*
 * CPU feature register tracking
 *
 * The safe value of a CPUID feature field is dependent on the implications
 * of the values assigned to it by the architecture. Based on the relationship
 * between the values, the features are classified into 3 types - LOWER_SAFE,
 * HIGHER_SAFE and EXACT.
 *
 * The lowest value of all the CPUs is chosen for LOWER_SAFE and highest
 * for HIGHER_SAFE. It is expected that all CPUs have the same value for
 * a field when EXACT is specified, failing which, the safe value specified
 * in the table is chosen.
 */

enum ftr_type {
	FTR_EXACT,	/* Use a predefined safe value */
	FTR_LOWER_SAFE,	/* Smaller value is safe */
	FTR_HIGHER_SAFE,/* Bigger value is safe */
};

#define FTR_STRICT	true	/* SANITY check strict matching required */
#define FTR_NONSTRICT	false	/* SANITY check ignored */

#define FTR_SIGNED	true	/* Value should be treated as signed */
#define FTR_UNSIGNED	false	/* Value should be treated as unsigned */

#define FTR_VISIBLE	true	/* Feature visible to the user space */
#define FTR_HIDDEN	false	/* Feature is hidden from the user */

#define FTR_VISIBLE_IF_IS_ENABLED(config)		\
	(IS_ENABLED(config) ? FTR_VISIBLE : FTR_HIDDEN)

struct arm64_ftr_bits {
	bool		sign;	/* Value is signed ? */
	bool		visible;
	bool		strict;	/* CPU Sanity check: strict matching required ? */
	enum ftr_type	type;
	u8		shift;
	u8		width;
	s64		safe_val; /* safe value for FTR_EXACT features */
};

/*
 * @arm64_ftr_reg - Feature register
 * @strict_mask		Bits which should match across all CPUs for sanity.
 * @sys_val		Safe value across the CPUs (system view)
 */
struct arm64_ftr_reg {
	const char			*name;
	u64				strict_mask;
	u64				user_mask;
	u64				sys_val;
	u64				user_val;
	const struct arm64_ftr_bits	*ftr_bits;
};

extern struct arm64_ftr_reg arm64_ftr_reg_ctrel0;

/*
 * CPU capabilities:
 *
 * We use arm64_cpu_capabilities to represent system features, errata work
 * arounds (both used internally by kernel and tracked in cpu_hwcaps) and
 * ELF HWCAPs (which are exposed to user).
 *
 * To support systems with heterogeneous CPUs, we need to make sure that we
 * detect the capabilities correctly on the system and take appropriate
 * measures to ensure there are no incompatibilities.
 *
 * This comment tries to explain how we treat the capabilities.
 * Each capability has the following list of attributes :
 *
 * 1) Scope of Detection : The system detects a given capability by
 *    performing some checks at runtime. This could be, e.g, checking the
 *    value of a field in CPU ID feature register or checking the cpu
 *    model. The capability provides a call back ( @matches() ) to
 *    perform the check. Scope defines how the checks should be performed.
 *    There are three cases:
 *
 *     a) SCOPE_LOCAL_CPU: check all the CPUs and "detect" if at least one
 *        matches. This implies, we have to run the check on all the
 *        booting CPUs, until the system decides that state of the
 *        capability is finalised. (See section 2 below)
 *		Or
 *     b) SCOPE_SYSTEM: check all the CPUs and "detect" if all the CPUs
 *        matches. This implies, we run the check only once, when the
 *        system decides to finalise the state of the capability. If the
 *        capability relies on a field in one of the CPU ID feature
 *        registers, we use the sanitised value of the register from the
 *        CPU feature infrastructure to make the decision.
 *		Or
 *     c) SCOPE_BOOT_CPU: Check only on the primary boot CPU to detect the
 *        feature. This category is for features that are "finalised"
 *        (or used) by the kernel very early even before the SMP cpus
 *        are brought up.
 *
 *    The process of detection is usually denoted by "update" capability
 *    state in the code.
 *
 * 2) Finalise the state : The kernel should finalise the state of a
 *    capability at some point during its execution and take necessary
 *    actions if any. Usually, this is done, after all the boot-time
 *    enabled CPUs are brought up by the kernel, so that it can make
 *    better decision based on the available set of CPUs. However, there
 *    are some special cases, where the action is taken during the early
 *    boot by the primary boot CPU. (e.g, running the kernel at EL2 with
 *    Virtualisation Host Extensions). The kernel usually disallows any
 *    changes to the state of a capability once it finalises the capability
 *    and takes any action, as it may be impossible to execute the actions
 *    safely. A CPU brought up after a capability is "finalised" is
 *    referred to as "Late CPU" w.r.t the capability. e.g, all secondary
 *    CPUs are treated "late CPUs" for capabilities determined by the boot
 *    CPU.
 *
 *    At the moment there are two passes of finalising the capabilities.
 *      a) Boot CPU scope capabilities - Finalised by primary boot CPU via
 *         setup_boot_cpu_capabilities().
 *      b) Everything except (a) - Run via setup_system_capabilities().
 *
 * 3) Verification: When a CPU is brought online (e.g, by user or by the
 *    kernel), the kernel should make sure that it is safe to use the CPU,
 *    by verifying that the CPU is compliant with the state of the
 *    capabilities finalised already. This happens via :
 *
 *	secondary_start_kernel()-> check_local_cpu_capabilities()
 *
 *    As explained in (2) above, capabilities could be finalised at
 *    different points in the execution. Each newly booted CPU is verified
 *    against the capabilities that have been finalised by the time it
 *    boots.
 *
 *	a) SCOPE_BOOT_CPU : All CPUs are verified against the capability
 *	except for the primary boot CPU.
 *
 *	b) SCOPE_LOCAL_CPU, SCOPE_SYSTEM: All CPUs hotplugged on by the
 *	user after the kernel boot are verified against the capability.
 *
 *    If there is a conflict, the kernel takes an action, based on the
 *    severity (e.g, a CPU could be prevented from booting or cause a
 *    kernel panic). The CPU is allowed to "affect" the state of the
 *    capability, if it has not been finalised already. See section 5
 *    for more details on conflicts.
 *
 * 4) Action: As mentioned in (2), the kernel can take an action for each
 *    detected capability, on all CPUs on the system. Appropriate actions
 *    include, turning on an architectural feature, modifying the control
 *    registers (e.g, SCTLR, TCR etc.) or patching the kernel via
 *    alternatives. The kernel patching is batched and performed at later
 *    point. The actions are always initiated only after the capability
 *    is finalised. This is usally denoted by "enabling" the capability.
 *    The actions are initiated as follows :
 *	a) Action is triggered on all online CPUs, after the capability is
 *	finalised, invoked within the stop_machine() context from
 *	enable_cpu_capabilitie().
 *
 *	b) Any late CPU, brought up after (1), the action is triggered via:
 *
 *	  check_local_cpu_capabilities() -> verify_local_cpu_capabilities()
 *
 * 5) Conflicts: Based on the state of the capability on a late CPU vs.
 *    the system state, we could have the following combinations :
 *
 *		x-----------------------------x
 *		| Type  | System   | Late CPU |
 *		|-----------------------------|
 *		|  a    |   y      |    n     |
 *		|-----------------------------|
 *		|  b    |   n      |    y     |
 *		x-----------------------------x
 *
 *     Two separate flag bits are defined to indicate whether each kind of
 *     conflict can be allowed:
 *		ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU - Case(a) is allowed
 *		ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU - Case(b) is allowed
 *
 *     Case (a) is not permitted for a capability that the system requires
 *     all CPUs to have in order for the capability to be enabled. This is
 *     typical for capabilities that represent enhanced functionality.
 *
 *     Case (b) is not permitted for a capability that must be enabled
 *     during boot if any CPU in the system requires it in order to run
 *     safely. This is typical for erratum work arounds that cannot be
 *     enabled after the corresponding capability is finalised.
 *
 *     In some non-typical cases either both (a) and (b), or neither,
 *     should be permitted. This can be described by including neither
 *     or both flags in the capability's type field.
 */


/*
 * Decide how the capability is detected.
 * On any local CPU vs System wide vs the primary boot CPU
 */
#define ARM64_CPUCAP_SCOPE_LOCAL_CPU		((u16)BIT(0))
#define ARM64_CPUCAP_SCOPE_SYSTEM		((u16)BIT(1))
/*
 * The capabilitiy is detected on the Boot CPU and is used by kernel
 * during early boot. i.e, the capability should be "detected" and
 * "enabled" as early as possibly on all booting CPUs.
 */
#define ARM64_CPUCAP_SCOPE_BOOT_CPU		((u16)BIT(2))
#define ARM64_CPUCAP_SCOPE_MASK			\
	(ARM64_CPUCAP_SCOPE_SYSTEM	|	\
	 ARM64_CPUCAP_SCOPE_LOCAL_CPU	|	\
	 ARM64_CPUCAP_SCOPE_BOOT_CPU)

#define SCOPE_SYSTEM				ARM64_CPUCAP_SCOPE_SYSTEM
#define SCOPE_LOCAL_CPU				ARM64_CPUCAP_SCOPE_LOCAL_CPU
#define SCOPE_BOOT_CPU				ARM64_CPUCAP_SCOPE_BOOT_CPU
#define SCOPE_ALL				ARM64_CPUCAP_SCOPE_MASK

/*
 * Is it permitted for a late CPU to have this capability when system
 * hasn't already enabled it ?
 */
#define ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU	((u16)BIT(4))
/* Is it safe for a late CPU to miss this capability when system has it */
#define ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU	((u16)BIT(5))

/*
 * CPU errata workarounds that need to be enabled at boot time if one or
 * more CPUs in the system requires it. When one of these capabilities
 * has been enabled, it is safe to allow any CPU to boot that doesn't
 * require the workaround. However, it is not safe if a "late" CPU
 * requires a workaround and the system hasn't enabled it already.
 */
#define ARM64_CPUCAP_LOCAL_CPU_ERRATUM		\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU | ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU)
/*
 * CPU feature detected at boot time based on system-wide value of a
 * feature. It is safe for a late CPU to have this feature even though
 * the system hasn't enabled it, although the feature will not be used
 * by Linux in this case. If the system has enabled this feature already,
 * then every late CPU must have it.
 */
#define ARM64_CPUCAP_SYSTEM_FEATURE	\
	(ARM64_CPUCAP_SCOPE_SYSTEM | ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU)
/*
 * CPU feature detected at boot time based on feature of one or more CPUs.
 * All possible conflicts for a late CPU are ignored.
 */
#define ARM64_CPUCAP_WEAK_LOCAL_CPU_FEATURE		\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU		|	\
	 ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU	|	\
	 ARM64_CPUCAP_PERMITTED_FOR_LATE_CPU)

/*
 * CPU feature detected at boot time, on one or more CPUs. A late CPU
 * is not allowed to have the capability when the system doesn't have it.
 * It is Ok for a late CPU to miss the feature.
 */
#define ARM64_CPUCAP_BOOT_RESTRICTED_CPU_LOCAL_FEATURE	\
	(ARM64_CPUCAP_SCOPE_LOCAL_CPU		|	\
	 ARM64_CPUCAP_OPTIONAL_FOR_LATE_CPU)

/*
 * CPU feature used early in the boot based on the boot CPU. All secondary
 * CPUs must match the state of the capability as detected by the boot CPU.
 */
#define ARM64_CPUCAP_STRICT_BOOT_CPU_FEATURE ARM64_CPUCAP_SCOPE_BOOT_CPU

extern struct static_key_false arm64_const_caps_ready;

#endif /* __ASSEMBLY__ */

#endif
