/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __OF_DEVICE_H_
#define __OF_DEVICE_H_

/*
 * Struct used for matching a device
 */
struct of_device_id {
	char	name[32];
	char	type[32];
	char	compatible[128];
	const void *data;
};

#endif
