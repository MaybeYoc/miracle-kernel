/*
 * We need to be able to catch inadvertent references to memstart_addr
 * that occur (potentially in generic code) before arm64_memblock_init()
 * executes, which assigns it its actual value. So use a default value
 * that cannot be mistaken for a real physical address.
 */
#include <linux/types.h>
#include <linux/cache.h>

s64 memstart_addr __ro_after_init = -1;
