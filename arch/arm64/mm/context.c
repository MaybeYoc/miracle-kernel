#include <asm/pgtable-types.h>
#include <asm/cpufeature.h>
#include <asm/smp.h>
/* Errata workaround post TTBRx_EL1 update. */
asmlinkage void post_ttbr_update_workaround(void)
{
	asm(ALTERNATIVE("nop; nop; nop",
			"ic iallu; dsb nsh; isb",
			ARM64_WORKAROUND_CAVIUM_27456,
			CONFIG_CAVIUM_ERRATUM_27456));
}
