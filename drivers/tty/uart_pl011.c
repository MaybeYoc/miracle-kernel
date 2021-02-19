// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 *
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * Copyright (C) 1999 2000 2001 Erik Mouw (J.A.K.Mouw@its.tudelft.nl)
 *
 * Modified to add driver model (DM) support
 * (C) Copyright 2016 Marcel Ziswiler <marcel.ziswiler@toradex.com>
 */
#include <linux/kernel.h>
#include <linux/io.h>

#include <asm/early_ioremap.h>

#include "uart.h"

struct pl01x_regs {
	u32	dr;		/* 0x00 Data register */
	u32	ecr;		/* 0x04 Error clear register (Write) */
	u32	pl010_lcrh;	/* 0x08 Line control register, high byte */
	u32	pl010_lcrm;	/* 0x0C Line control register, middle byte */
	u32	pl010_lcrl;	/* 0x10 Line control register, low byte */
	u32	pl010_cr;	/* 0x14 Control register */
	u32	fr;		/* 0x18 Flag register (Read only) */
#ifdef CONFIG_PL011_SERIAL_RLCR
	u32	pl011_rlcr;	/* 0x1c Receive line control register */
#else
	u32	reserved;
#endif
	u32	ilpr;		/* 0x20 IrDA low-power counter register */
	u32	pl011_ibrd;	/* 0x24 Integer baud rate register */
	u32	pl011_fbrd;	/* 0x28 Fractional baud rate register */
	u32	pl011_lcrh;	/* 0x2C Line control register */
	u32	pl011_cr;	/* 0x30 Control register */
};


#define UART_PL01x_RSR_OE               0x08
#define UART_PL01x_RSR_BE               0x04
#define UART_PL01x_RSR_PE               0x02
#define UART_PL01x_RSR_FE               0x01

#define UART_PL01x_FR_TXFE              0x80
#define UART_PL01x_FR_RXFF              0x40
#define UART_PL01x_FR_TXFF              0x20
#define UART_PL01x_FR_RXFE              0x10
#define UART_PL01x_FR_BUSY              0x08
#define UART_PL01x_FR_TMSK              (UART_PL01x_FR_TXFF + UART_PL01x_FR_BUSY)

/*
 *  PL010 definitions
 *
 */
#define UART_PL010_CR_LPE               (1 << 7)
#define UART_PL010_CR_RTIE              (1 << 6)
#define UART_PL010_CR_TIE               (1 << 5)
#define UART_PL010_CR_RIE               (1 << 4)
#define UART_PL010_CR_MSIE              (1 << 3)
#define UART_PL010_CR_IIRLP             (1 << 2)
#define UART_PL010_CR_SIREN             (1 << 1)
#define UART_PL010_CR_UARTEN            (1 << 0)

#define UART_PL010_LCRH_WLEN_8          (3 << 5)
#define UART_PL010_LCRH_WLEN_7          (2 << 5)
#define UART_PL010_LCRH_WLEN_6          (1 << 5)
#define UART_PL010_LCRH_WLEN_5          (0 << 5)
#define UART_PL010_LCRH_FEN             (1 << 4)
#define UART_PL010_LCRH_STP2            (1 << 3)
#define UART_PL010_LCRH_EPS             (1 << 2)
#define UART_PL010_LCRH_PEN             (1 << 1)
#define UART_PL010_LCRH_BRK             (1 << 0)


#define UART_PL010_BAUD_460800            1
#define UART_PL010_BAUD_230400            3
#define UART_PL010_BAUD_115200            7
#define UART_PL010_BAUD_57600             15
#define UART_PL010_BAUD_38400             23
#define UART_PL010_BAUD_19200             47
#define UART_PL010_BAUD_14400             63
#define UART_PL010_BAUD_9600              95
#define UART_PL010_BAUD_4800              191
#define UART_PL010_BAUD_2400              383
#define UART_PL010_BAUD_1200              767
/*
 *  PL011 definitions
 *
 */
#define UART_PL011_LCRH_SPS             (1 << 7)
#define UART_PL011_LCRH_WLEN_8          (3 << 5)
#define UART_PL011_LCRH_WLEN_7          (2 << 5)
#define UART_PL011_LCRH_WLEN_6          (1 << 5)
#define UART_PL011_LCRH_WLEN_5          (0 << 5)
#define UART_PL011_LCRH_FEN             (1 << 4)
#define UART_PL011_LCRH_STP2            (1 << 3)
#define UART_PL011_LCRH_EPS             (1 << 2)
#define UART_PL011_LCRH_PEN             (1 << 1)
#define UART_PL011_LCRH_BRK             (1 << 0)

#define UART_PL011_CR_CTSEN             (1 << 15)
#define UART_PL011_CR_RTSEN             (1 << 14)
#define UART_PL011_CR_OUT2              (1 << 13)
#define UART_PL011_CR_OUT1              (1 << 12)
#define UART_PL011_CR_RTS               (1 << 11)
#define UART_PL011_CR_DTR               (1 << 10)
#define UART_PL011_CR_RXE               (1 << 9)
#define UART_PL011_CR_TXE               (1 << 8)
#define UART_PL011_CR_LPE               (1 << 7)
#define UART_PL011_CR_IIRLP             (1 << 2)
#define UART_PL011_CR_SIREN             (1 << 1)
#define UART_PL011_CR_UARTEN            (1 << 0)

#define UART_PL011_IMSC_OEIM            (1 << 10)
#define UART_PL011_IMSC_BEIM            (1 << 9)
#define UART_PL011_IMSC_PEIM            (1 << 8)
#define UART_PL011_IMSC_FEIM            (1 << 7)
#define UART_PL011_IMSC_RTIM            (1 << 6)
#define UART_PL011_IMSC_TXIM            (1 << 5)
#define UART_PL011_IMSC_RXIM            (1 << 4)
#define UART_PL011_IMSC_DSRMIM          (1 << 3)
#define UART_PL011_IMSC_DCDMIM          (1 << 2)
#define UART_PL011_IMSC_CTSMIM          (1 << 1)
#define UART_PL011_IMSC_RIMIM           (1 << 0)

enum pl01x_type {
	TYPE_PL010,
	TYPE_PL011,
};

static enum pl01x_type pl01x_type __attribute__ ((section(".data")));
static struct pl01x_regs *base_regs __attribute__ ((section(".data")));


static int pl01x_putc(struct pl01x_regs *regs, char c)
{
	/* Wait until there is space in the FIFO */
	if (readl(&regs->fr) & UART_PL01x_FR_TXFF)
		return -EAGAIN;

	/* Send the character */
	writel(c, &regs->dr);

	return 0;
}

static int pl01x_getc(struct pl01x_regs *regs)
{
	unsigned int data;

	/* Wait until there is data in the FIFO */
	if (readl(&regs->fr) & UART_PL01x_FR_RXFE)
		return -EAGAIN;

	data = readl(&regs->dr);

	/* Check for an error flag */
	if (data & 0xFFFFFF00) {
		/* Clear the error */
		writel(0xFFFFFFFF, &regs->ecr);
		return -1;
	}

	return (int) data;
}

static int pl01x_tstc(struct pl01x_regs *regs)
{
	//WATCHDOG_RESET();
	return !(readl(&regs->fr) & UART_PL01x_FR_RXFE);
}

static int pl01x_generic_serial_init(struct pl01x_regs *regs,
				     enum pl01x_type type)
{
	switch (type) {
	case TYPE_PL010:
		/* disable everything */
		writel(0, &regs->pl010_cr);
		break;
	case TYPE_PL011:
		/* disable everything */
		writel(0, &regs->pl011_cr);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pl011_set_line_control(struct pl01x_regs *regs)
{
	unsigned int lcr;
	/*
	 * Internal update of baud rate register require line
	 * control register write
	 */
	lcr = UART_PL011_LCRH_WLEN_8 | UART_PL011_LCRH_FEN;
	writel(lcr, &regs->pl011_lcrh);
	return 0;
}

static int pl01x_generic_setbrg(struct pl01x_regs *regs, enum pl01x_type type,
				int clock, int baudrate)
{
	switch (type) {
	case TYPE_PL010: {
		unsigned int divisor;

		/* disable everything */
		writel(0, &regs->pl010_cr);

		switch (baudrate) {
		case 9600:
			divisor = UART_PL010_BAUD_9600;
			break;
		case 19200:
			divisor = UART_PL010_BAUD_19200;
			break;
		case 38400:
			divisor = UART_PL010_BAUD_38400;
			break;
		case 57600:
			divisor = UART_PL010_BAUD_57600;
			break;
		case 115200:
			divisor = UART_PL010_BAUD_115200;
			break;
		default:
			divisor = UART_PL010_BAUD_38400;
		}

		writel((divisor & 0xf00) >> 8, &regs->pl010_lcrm);
		writel(divisor & 0xff, &regs->pl010_lcrl);

		/*
		 * Set line control for the PL010 to be 8 bits, 1 stop bit,
		 * no parity, fifo enabled
		 */
		writel(UART_PL010_LCRH_WLEN_8 | UART_PL010_LCRH_FEN,
		       &regs->pl010_lcrh);
		/* Finally, enable the UART */
		writel(UART_PL010_CR_UARTEN, &regs->pl010_cr);
		break;
	}
	case TYPE_PL011: {
		unsigned int temp;
		unsigned int divider;
		unsigned int remainder;
		unsigned int fraction;

		/*
		* Set baud rate
		*
		* IBRD = UART_CLK / (16 * BAUD_RATE)
		* FBRD = RND((64 * MOD(UART_CLK,(16 * BAUD_RATE)))
		*		/ (16 * BAUD_RATE))
		*/
		temp = 16 * baudrate;
		divider = clock / temp;
		remainder = clock % temp;
		temp = (8 * remainder) / baudrate;
		fraction = (temp >> 1) + (temp & 1);

		writel(divider, &regs->pl011_ibrd);
		writel(fraction, &regs->pl011_fbrd);

		pl011_set_line_control(regs);
		/* Finally, enable the UART */
		writel(UART_PL011_CR_UARTEN | UART_PL011_CR_TXE |
		       UART_PL011_CR_RXE | UART_PL011_CR_RTS, &regs->pl011_cr);
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

static void pl01x_serial_init_baud(int baudrate)
{
	int clock = 0;
	void *virt_base = 0;

	pl01x_type = TYPE_PL011;
	clock = 150000000;

	virt_base = early_ioremap(0x9000000, 0x1000);
	if (!virt_base)
		return;

	base_regs = (struct pl01x_regs *)virt_base;

	pl01x_generic_serial_init(base_regs, pl01x_type);
	pl01x_generic_setbrg(base_regs, pl01x_type, clock, baudrate);
}

/*
 * Integrator AP has two UARTs, we use the first one, at 38400-8-N-1
 * Integrator CP has two UARTs, use the first one, at 38400-8-N-1
 * Versatile PB has four UARTs.
 */
static void pl01x_serial_init(struct tty_uart_platdata *plat, int port, int baudrate)
{
	pl01x_serial_init_baud(115200);
}

static void pl01x_serial_putc(struct tty_uart_platdata *plat, const char c)
{
	if (c == '\n')
		while (pl01x_putc(base_regs, '\r') == -EAGAIN);

	while (pl01x_putc(base_regs, c) == -EAGAIN);
}

static int pl01x_serial_getc(struct tty_uart_platdata *plat)
{
	while (1) {
		int ch = pl01x_getc(base_regs);

		if (ch == -EAGAIN) {
			//WATCHDOG_RESET();
			continue;
		}

		return ch;
	}
}

static int pl01x_serial_tstc(struct tty_uart_platdata *plat, bool input)
{
	return pl01x_tstc(base_regs);
}

static int pl01x_serial_setbrg(struct tty_uart_platdata *plat, int port, int baudrate)
{
	/*
	 * Flush FIFO and wait for non-busy before changing baudrate to avoid
	 * crap in console
	 */
	while (!(readl(&base_regs->fr) & UART_PL01x_FR_TXFE))
		//WATCHDOG_RESET();
		;
	while (readl(&base_regs->fr) & UART_PL01x_FR_BUSY)
		//WATCHDOG_RESET();
		;
	pl01x_serial_init_baud(baudrate);
	return 0;
}

struct tty_uart_platdata plat = {
	.setbrg = pl01x_serial_setbrg,
	.init = pl01x_serial_init,
	.getc = pl01x_serial_getc,
	.putc = pl01x_serial_putc,
	.pending = pl01x_serial_tstc,
	.port = 0,
	.nr_port = 0,
	.baudrate = 0,
};
