#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/cache.h>
#include <linux/kernel.h>
/*
 * Used to generate warnings if static_key manipulation functions are used
 * before jump_label_init is called.
 */
bool static_key_initialized __read_mostly;

int start_kernel(void)
{
	return 0;
}
