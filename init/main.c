/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  GK 2/5/95  -  Changed to support mounting root fs via NFS
 *  Added initrd & change_root: Werner Almesberger & Hans Lermen, Feb '96
 *  Moan early if gcc is old, avoiding bogus kernels - Paul Gortmaker, May '96
 *  Simplified starting of init:  Michael A. Griffith <grif@acm.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/memblock.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched/init.h>
#include <linux/radix-tree.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jump_label.h>
#include <linux/sched/task_stack.h>
#include <linux/init_task.h>
#include <linux/cpu.h>
#include <linux/clockchips.h>
#include <clocksource/arm_arch_timer.h>
#include <linux/timekeeping.h>

#include <asm/memory.h>
#include <asm/sections.h>
#include <asm/setup.h>

enum system_states system_state __read_mostly;

/* Untouched command line saved by arch-specific code. */
char __initdata boot_command_line[COMMAND_LINE_SIZE];

extern void time_init(void);

extern const struct obs_kernel_param __setup_start[], __setup_end[];

/*
 * Used to generate warnings if static_key manipulation functions are used
 * before jump_label_init is called.
 */
bool static_key_initialized __read_mostly;

/* Check for early params. */
static int __init do_early_param(char *param, char *val,
				 const char *unused, void *arg)
{
	const struct obs_kernel_param *p;

	for (p = __setup_start; p < __setup_end; p++) {
		if ((p->early && parameq(param, p->str)) ||
		    (strcmp(param, "console") == 0 &&
		     strcmp(p->str, "earlycon") == 0)
		) {
			if (p->setup_func(val) != 0)
				pr_warn("Malformed early option '%s'\n", param);
		}
	}
	/* We accept everything at this stage. */
	return 0;
}

void __init parse_early_options(char *cmdline)
{
	parse_args("early options", cmdline, NULL, 0, 0, 0, NULL,
		   do_early_param);
}

void __init __weak smp_setup_processor_id(void)
{
}

/* Arch code calls this early on, or if not, just before other parsing. */
void __init parse_early_param(void)
{
	static int done __initdata;
	static char tmp_cmdline[COMMAND_LINE_SIZE] __initdata;

	if (done)
		return;

	/* All fall through to do_early_param. */
	strlcpy(tmp_cmdline, boot_command_line, COMMAND_LINE_SIZE);
	parse_early_options(tmp_cmdline);
	done = 1;
}

/*
 * Set up kernel memory allocators
 */
static void __init mm_init(void)
{
	mem_init();
	kmem_cache_init();
	vmalloc_init();
}

asmlinkage __visible void __init start_kernel(void)
{
	char *command_line;

	system_state = SYSTEM_BOOTING;

	set_task_stack_end_magic(&init_task);

	smp_setup_processor_id();

	local_irq_disable();

	boot_cpu_init();

	pr_notice("%s", linux_banner);
	setup_arch(&command_line);

	setup_nr_cpu_ids();
	setup_per_cpu_areas();
	smp_prepare_boot_cpu();	/* arch-specific boot-cpu hooks */

	build_all_zonelists();

	pr_notice("Kernel command line: %s\n", boot_command_line);
	parse_early_param();

	jump_label_init();

	mm_init();

	setup_per_cpu_pageset();

	/*
	 * Set up the scheduler prior starting any interrupts (such as the
	 * timer interrupt). Full topology setup happens at smp_init()
	 * time - but meanwhile we still have a functioning scheduler.
	 */
	sched_init();

	radix_tree_init();

	/* init some links before init_ISA_irqs() */
	early_irq_init();
	init_IRQ();

	timekeeping_init();
	time_init();
	WARN(!irqs_disabled(), "Interrupts were enabled early\n");

	local_irq_enable();

	while(1);
}
