// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple xorshift PRNG
 *   see http://www.jstatsoft.org/v08/i14/paper
 *
 * Copyright (c) 2012 Michael Walle
 * Michael Walle <michael@walle.cc>
 */
#include <linux/compiler.h>

static unsigned long y = 1;

static unsigned long random_r(unsigned long *seedp)
{
	*seedp ^= (*seedp << 13);
	*seedp ^= (*seedp >> 17);
	*seedp ^= (*seedp << 5);

	return *seedp;
}

unsigned long random(void)
{
	return random_r(&y);
}

unsigned long random_range(unsigned long min, unsigned long max)
{
	unsigned long temp;

	if (unlikely(min > max))
		return 0;

	if (unlikely(min == max))
		return min;

	temp = max - min + 1;

	return min + (random() % temp);
}

void srand(unsigned long seed)
{
	y = seed;
}
