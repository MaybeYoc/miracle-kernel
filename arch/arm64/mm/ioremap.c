#include <linux/io.h>

#include <asm/early_ioremap.h>
#include <asm/page.h>

void __iomem *__ioremap(phys_addr_t phys_addr, size_t size, pgprot_t prot)
{
	/* TODO */
	return early_ioremap(phys_addr, size);
}

void __iounmap(volatile void __iomem *io_addr)
{
	/* TODO */
}

void __iomem *ioremap_cache(phys_addr_t phys_addr, size_t size)
{
	return NULL;
}

/*
 * Must be called after early_fixmap_init
 */
void __init early_ioremap_init(void)
{
	early_ioremap_setup();
}
