#ifndef __LINUX_RANDOM_H_
#define __LINUX_RANDOM_H_

#define RAND_MAX -1U
void srand(unsigned long seed);
unsigned long random(void);
unsigned long
random_range(unsigned long min, unsigned long max);

static inline unsigned long random_max(unsigned long max)
{
	return random_range(0, max);
}

#endif /* __LINUX_RANDOM_H_ */