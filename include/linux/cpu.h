/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/cpu.h - generic cpu definition
 *
 * This is mainly for topological representation. We define the 
 * basic 'struct cpu' here, which can be embedded in per-arch 
 * definitions of processors.
 *
 * Basic handling of the devices is done in drivers/base/cpu.c
 *
 * CPUs are exported via sysfs in the devices/system/cpu
 * directory. 
 */
#ifndef _LINUX_CPU_H_
#define _LINUX_CPU_H_

#include <linux/device.h>

struct cpu {
	int node_id;		/* The node which contains the CPU */
	int hotpluggable;	/* creates sysfs control file if hotpluggable */
	struct device dev;
};

extern void boot_cpu_init(void);

#include <asm/cpu.h>

#endif /* _LINUX_CPU_H_ */
