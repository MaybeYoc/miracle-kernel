#include <linux/linkage.h>

/* Errata workaround post TTBRx_EL1 update. */
asmlinkage void post_ttbr_update_workaround(void)
{
	asm("nop; nop; nop");
}