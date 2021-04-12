/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_REBOOT_H
#define _LINUX_REBOOT_H

enum reboot_mode {
	REBOOT_COLD = 0,
	REBOOT_WARM,
	REBOOT_HARD,
	REBOOT_SOFT,
	REBOOT_GPIO,
};

#endif /* _LINUX_REBOOT_H */
