#include <linux/types.h>
#include <linux/cache.h>

/*
 * The recorded values of x0 .. x3 upon kernel entry.
 */
u64 __cacheline_aligned boot_args[4];
