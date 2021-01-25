#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/cachetype.h>
#include <asm/cputype.h>

unsigned long __icache_flags;

u64 __attribute_const__ icache_get_ccsidr(void)
{
	u64 ccsidr;

	WARN_ON(preemptible());

	/* Select L1 I-cache and read its size ID register */
	asm("msr csselr_el1, %1; isb; mrs %0, ccsidr_el1"
	    : "=r"(ccsidr) : "r"(1L));
	return ccsidr;
}
