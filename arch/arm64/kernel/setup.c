#include <linux/cache.h>
#include <linux/types.h>
#include <linux/init.h>

phys_addr_t __fdt_pointer __initdata;

/*
 * The recorded values of x0 .. x3 upon kernel entry.
 */
u64 __cacheline_aligned boot_args[4];
