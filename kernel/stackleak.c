// SPDX-License-Identifier: GPL-2.0
/*
 * This code fills the used part of the kernel stack with a poison value
 * before returning to userspace. It's part of the STACKLEAK feature
 * ported from grsecurity/PaX.
 *
 * Author: Alexander Popov <alex.popov@linux.com>
 *
 * STACKLEAK reduces the information which kernel stack leak bugs can
 * reveal and blocks some uninitialized stack variable attacks.
 */
#include <linux/linkage.h>

asmlinkage void notrace stackleak_erase(void)
{

}
