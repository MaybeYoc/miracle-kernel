#include <linux/io.h>
#include <asm/page.h>

void __iomem *__ioremap(phys_addr_t phys_addr, size_t size, pgprot_t prot)
{
	return NULL;
}

void __iounmap(volatile void __iomem *io_addr)
{

}

void __iomem *ioremap_cache(phys_addr_t phys_addr, size_t size)
{
	return NULL;
}
