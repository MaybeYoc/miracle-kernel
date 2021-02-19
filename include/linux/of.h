/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _LINUX_OF_H
#define _LINUX_OF_H
/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh and other computers.
 *
 * Copyright (C) 1996-2005 Paul Mackerras.
 *
 * Updates for PPC64 by Peter Bergner & David Engebretsen, IBM Corp.
 * Updates for SPARC64 by David S. Miller
 * Derived from PowerPC and Sparc prom.h files by Stephen Rothwell, IBM Corp.
 */
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>

#include <asm/byteorder.h>

/*
 * OF address retrieval & translation
 */

/* Helper to read a big number; size is in cells (not bytes) */
static inline u64 of_read_number(const __be32 *cell, int size)
{
	u64 r = 0;
	while (size--)
		r = (r << 32) | be32_to_cpu(*(cell++));
	return r;
}

/* Like of_read_number, but we want an unsigned long result */
static inline unsigned long of_read_ulong(const __be32 *cell, int size)
{
	/* toss away upper bits if unsigned long is smaller than u64 */
	return of_read_number(cell, size);
}

#endif /* _LINUX_OF_H */
