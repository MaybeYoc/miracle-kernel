// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/linkage.h>

#include <asm/ptrace.h>

asmlinkage void el0_svc_handler(struct pt_regs *regs)
{
	printk("doing el0 syscall\n");
}
