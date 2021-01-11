#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/cache.h>
#include <linux/kernel.h>
#include <linux/jump_label.h>
#include <asm/io.h>
#include <linux/irqflags.h>
#include <linux/list.h>
#include <linux/llist.h>

/*
 * Used to generate warnings if static_key manipulation functions are used
 * before jump_label_init is called.
 */
bool static_key_initialized __read_mostly;

int start_kernel(void)
{
	u64 aa = 32;
	u32 bb = 64;

	static_key_initialized = true;

	if (static_key_initialized)
		bb -= 32;
	
	aa += bb;
	return aa;
}
