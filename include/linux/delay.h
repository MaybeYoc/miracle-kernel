
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <linux/kernel.h>
#include <linux/time.h>

#include <asm/delay.h>


#ifndef delay
static inline void delay(unsigned long cycles)
{
	__delay(cycles);
}
#define delay(x) delay(x)
#endif

#ifndef ndelay
static inline void ndelay(unsigned long nsecs)
{
	__ndelay(nsecs);
}
#define ndelay(x) ndelay(x)
#endif

#ifndef udelay
static inline void udelay(unsigned long usecs)
{
	__ndelay(usecs * NSEC_PER_USEC);
}
#define udelay(x) udelay(x)
#endif

#ifndef mdelay
static inline void mdelay(unsigned long msecs)
{
	__ndelay(msecs * NSEC_PER_MSEC);
}
#define mdelay(x) mdelay(x)
#endif

#endif /* defined(_LINUX_DELAY_H) */
