#include <linux/types.h>
#include <linux/cache.h>
#include <linux/init.h>

unsigned long elf_hwcap __read_mostly;

/*
 * The recorded values of x0 .. x3 upon kernel entry.
 */
u64 __cacheline_aligned boot_args[4];
phys_addr_t __fdt_pointer __initdata;
