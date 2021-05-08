// SPDX-License-Identifier: GPL-2.0+
/*
 * Simple xorshift PRNG
 *   see http://www.jstatsoft.org/v08/i14/paper
 *
 * Copyright (c) 2012 Michael Walle
 * Michael Walle <michael@walle.cc>
 */

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

	if (min >= max)
		return 0;

	temp = max - min;

	return (random() % (min + 1)) + (random() % (temp + 1));
}

void srand(unsigned long seed)
{
	y = seed;
}
