#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>

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
