#include <linux/kconfig.h>
#include <linux/cache.h>

unsigned long __cacheline_aligned boot_args[4];

int start_kernel(void)
{
	return 0;
}
