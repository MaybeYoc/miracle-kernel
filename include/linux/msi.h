/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_MSI_H
#define LINUX_MSI_H

#include <linux/list.h>

struct msi_msg {
	u32	address_lo;	/* low 32 bits of msi message address */
	u32	address_hi;	/* high 32 bits of msi message address */
	u32	data;		/* 16 bits of msi message data */
};

/**
 * struct msi_desc - Descriptor structure for MSI based interrupts
 * @list:	List head for management
 * @irq:	The base interrupt number
 * @nvec_used:	The number of vectors used
 * @dev:	Pointer to the device which uses this descriptor
 * @msg:	The last set MSI message cached for reuse
 * @affinity:	Optional pointer to a cpu affinity mask for this descriptor
 *
 * @masked:	[PCI MSI/X] Mask bits
 * @is_msix:	[PCI MSI/X] True if MSI-X
 * @multiple:	[PCI MSI/X] log2 num of messages allocated
 * @multi_cap:	[PCI MSI/X] log2 num of messages supported
 * @maskbit:	[PCI MSI/X] Mask-Pending bit supported?
 * @is_64:	[PCI MSI/X] Address size: 0=32bit 1=64bit
 * @entry_nr:	[PCI MSI/X] Entry which is described by this descriptor
 * @default_irq:[PCI MSI/X] The default pre-assigned non-MSI irq
 * @mask_pos:	[PCI MSI]   Mask register position
 * @mask_base:	[PCI MSI-X] Mask register base address
 * @platform:	[platform]  Platform device specific msi descriptor data
 * @fsl_mc:	[fsl-mc]    FSL MC device specific msi descriptor data
 */
struct msi_desc {
	/* Shared device/bus type independent data */
	struct list_head		list;
	unsigned int			irq;
	unsigned int			nvec_used;
};

#endif /* LINUX_MSI_H */
