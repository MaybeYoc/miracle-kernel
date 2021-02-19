/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __INITRD_H_
#define __INITRD_H_

/* 1 if it is not an error if initrd_start < memory_start */
extern int initrd_below_start_ok;

/* free_initrd_mem always gets called with the next two as arguments.. */
extern unsigned long initrd_start, initrd_end;

extern phys_addr_t phys_initrd_start;
extern unsigned long phys_initrd_size;

#endif
