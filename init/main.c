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

asmlinkage __visible void __init start_kernel(void)
{
	u64 aa = 32;
	u32 bb = 64;

	aa += bb;
}
