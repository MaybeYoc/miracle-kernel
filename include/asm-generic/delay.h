/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_DELAY_H
#define __ASM_GENERIC_DELAY_H

extern void __delay(unsigned long cycles);
extern void __ndelay(unsigned long nsecs);

#endif /* __ASM_GENERIC_DELAY_H */
