#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/preempt.h>
#include <linux/percpu.h>
#include <linux/bug.h>
#include <linux/cache.h>
#include <linux/cpu.h>

#include <asm/arch_timer.h>
#include <asm/cputype.h>

/*
 * In case the boot CPU is hotpluggable, we record its initial state and
 * current state separately. Certain system registers may contain different
 * values depending on configuration at or after reset.
 */
DEFINE_PER_CPU(struct cpuinfo_arm64, cpu_data);
static struct cpuinfo_arm64 boot_cpu_data;

static char *icache_policy_str[] = {
	[0 ... ICACHE_POLICY_PIPT]	= "RESERVED/UNKNOWN",
	[ICACHE_POLICY_VIPT]		= "VIPT",
	[ICACHE_POLICY_PIPT]		= "PIPT",
	[ICACHE_POLICY_VPIPT]		= "VPIPT",
};

unsigned long __icache_flags;

static const char *const hwcap_str[] = {
	"fp",
	"asimd",
	"evtstrm",
	"aes",
	"pmull",
	"sha1",
	"sha2",
	"crc32",
	"atomics",
	"fphp",
	"asimdhp",
	"cpuid",
	"asimdrdm",
	"jscvt",
	"fcma",
	"lrcpc",
	"dcpop",
	"sha3",
	"sm3",
	"sm4",
	"asimddp",
	"sha512",
	"sve",
	"asimdfhm",
	"dit",
	"uscat",
	"ilrcpc",
	"flagm",
	"ssbs",
	"sb",
	"paca",
	"pacg",
	NULL
};

u64 __attribute_const__ icache_get_ccsidr(void)
{
	u64 ccsidr;

	WARN_ON(preemptible());

	/* Select L1 I-cache and read its size ID register */
	asm("msr csselr_el1, %1; isb; mrs %0, ccsidr_el1"
	    : "=r"(ccsidr) : "r"(1L));
	return ccsidr;
}

static void cpuinfo_detect_icache_policy(struct cpuinfo_arm64 *info)
{
	unsigned int cpu = smp_processor_id();
	u32 l1ip = CTR_L1IP(info->reg_ctr);

	switch (l1ip) {
	case ICACHE_POLICY_PIPT:
		break;
	case ICACHE_POLICY_VPIPT:
		set_bit(ICACHEF_VPIPT, &__icache_flags);
		break;
	default:
		/* Fallthrough */
	case ICACHE_POLICY_VIPT:
		/* Assume aliasing */
		set_bit(ICACHEF_ALIASING, &__icache_flags);
	}

	pr_info("Detected %s I-cache on CPU%d\n", icache_policy_str[l1ip], cpu);
}

static void __cpuinfo_store_cpu(struct cpuinfo_arm64 *info)
{
	info->reg_cntfrq = arch_timer_get_cntfrq();
	/*
	 * Use the effective value of the CTR_EL0 than the raw value
	 * exposed by the CPU. CTR_E0.IDC field value must be interpreted
	 * with the CLIDR_EL1 fields to avoid triggering false warnings
	 * when there is a mismatch across the CPUs. Keep track of the
	 * effective value of the CTR_EL0 in our internal records for
	 * acurate sanity check and feature enablement.
	 */
	info->reg_ctr = read_cpuid_effective_cachetype();
	info->reg_dczid = read_cpuid(DCZID_EL0);
	info->reg_midr = read_cpuid_id();
	info->reg_revidr = read_cpuid(REVIDR_EL1);

	info->reg_id_aa64dfr0 = read_cpuid(ID_AA64DFR0_EL1);
	info->reg_id_aa64dfr1 = read_cpuid(ID_AA64DFR1_EL1);
	info->reg_id_aa64isar0 = read_cpuid(ID_AA64ISAR0_EL1);
	info->reg_id_aa64isar1 = read_cpuid(ID_AA64ISAR1_EL1);
	info->reg_id_aa64mmfr0 = read_cpuid(ID_AA64MMFR0_EL1);
	info->reg_id_aa64mmfr1 = read_cpuid(ID_AA64MMFR1_EL1);
	info->reg_id_aa64mmfr2 = read_cpuid(ID_AA64MMFR2_EL1);
	info->reg_id_aa64pfr0 = read_cpuid(ID_AA64PFR0_EL1);
	info->reg_id_aa64pfr1 = read_cpuid(ID_AA64PFR1_EL1);
	info->reg_id_aa64zfr0 = read_cpuid(ID_AA64ZFR0_EL1);

	cpuinfo_detect_icache_policy(info);
}

void cpuinfo_store_cpu(void)
{
	struct cpuinfo_arm64 *info = this_cpu_ptr(&cpu_data);
	__cpuinfo_store_cpu(info);
}

void __init cpuinfo_store_boot_cpu(void)
{
	struct cpuinfo_arm64 *info = &per_cpu(cpu_data, 0);

	__cpuinfo_store_cpu(info);

	boot_cpu_data = *info;
}
