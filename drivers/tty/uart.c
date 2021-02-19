/*
 *(C) Copyright Black Sesame Technologies (Shanghai)Ltd. Co., 2020. All rights reserved.
 *
 * This file contains proprietary information that is the sole intellectual property of Black
 * Sesame Technologies (Shanghai)Ltd. Co. No part of this material or its documentation 
 * may be reproduced, distributed, transmitted, displayed or published in any manner 
 * without the written permission of Black Sesame Technologies (Shanghai)Ltd. Co.
 */

#include <linux/kernel.h>
#include <linux/init.h>

#include "uart.h"

static int uart_init_done = 0;
extern struct tty_uart_platdata plat;

void uart_init(int port, int baudrate)
{
    plat.init(&plat, port, baudrate);
}

static int early_uart(char *p)
{
	uart_init(0, 115200);

	uart_init_done = 1;

	return 0;
}
early_param("earlycon", early_uart);

int uart_setbrg(int port, int baudrate)
{
	return plat.setbrg(&plat, port, baudrate);
}

int uart_pending(bool input)
{
    return plat.pending(&plat, input);
}

int getc(void)
{
    return plat.getc(&plat);
}

int tstc(void)
{
    return plat.pending(&plat, true);
}

void putc_q(const char c)
{
    plat.putc(&plat, c);
}

void puts_q(const char *str)
{
	if (!uart_init_done)
		return;

	while (*str) {
		putc_q(*str++);
	}
}

int uart_get_baudrate(void)
{
    return plat.baudrate;
}

int uart_get_port(void)
{
    return plat.port;
}
