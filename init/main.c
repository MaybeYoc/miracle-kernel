
#include <linux/kernel.h>
#include <linux/kconfig.h>
#include <linux/types.h>
#include <linux/compiler.h>

int start_kernel(void)
{
	u64 aa, bb;

	aa = 2;

	bb = 3;

	return aa + bb;
}
