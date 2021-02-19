/*
 *(C) Copyright Black Sesame Technologies (Shanghai)Ltd. Co., 2020. All rights reserved.
 *
 * This file contains proprietary information that is the sole intellectual property of Black
 * Sesame Technologies (Shanghai)Ltd. Co. No part of this material or its documentation 
 * may be reproduced, distributed, transmitted, displayed or published in any manner 
 * without the written permission of Black Sesame Technologies (Shanghai)Ltd. Co.
 */

#ifndef __MODULE_UART_H_
#define __MODULE_UART_H_

enum serial_par {
	SERIAL_PAR_ODD,
	SERIAL_PAR_EVEN
};

enum serial_bits {
	SERIAL_5_BITS,
	SERIAL_6_BITS,
	SERIAL_7_BITS,
	SERIAL_8_BITS
};

enum serial_stop {
	SERIAL_ONE_STOP,	/*   1 stop bit */
	SERIAL_TWO_STOP,		/*   2 stop bit */
	SERIAL_ONE_HALF_STOP	/* 1.5 stop bit */
};

enum serial_chip_type {
	SERIAL_CHIP_UNKNOWN = -1,
	SERIAL_CHIP_16550_COMPATIBLE,
};

enum adr_space_type {
	SERIAL_ADDRESS_SPACE_MEMORY = 0,
	SERIAL_ADDRESS_SPACE_IO,
};

/**
 * struct serial_device_info - structure to hold serial device info
 *
 * @type:	type of the UART chip
 * @addr_space:	address space to access the registers
 * @addr:	physical address of the registers
 * @reg_width:	size (in bytes) of the IO accesses to the registers
 * @reg_offset:	offset to apply to the @addr from the start of the registers
 * @reg_shift:	quantity to shift the register offsets by
 * @baudrate:	baud rate
 */
struct serial_device_info {
	enum serial_chip_type type;
	enum adr_space_type addr_space;
	ulong addr;
	u8 reg_width;
	u8 reg_offset;
	u8 reg_shift;
	unsigned int baudrate;
};

struct tty_uart_platdata {
	int (*setbrg)(struct tty_uart_platdata *plat, int port, int baudrate);
	void (*init)(struct tty_uart_platdata *plat, int port, int baudrate);
	int (*getc)(struct tty_uart_platdata *plat);
	void (*putc)(struct tty_uart_platdata *plat, const char ch);
	int (*pending)(struct tty_uart_platdata *plat, bool input);
	/**
	 * getconfig() - Get the uart configuration
	 * (parity, 5/6/7/8 bits word length, stop bits)
	 *
	 * Get a current config for this device.
	 *
	 * @dev: Device pointer
	 * @serial_config: Returns config information (see SERIAL_... above)
	 * @return 0 if OK, -ve on error
	 */
	int (*getconfig)(struct tty_uart_platdata *plat, uint *serial_config);
	/**
	 * setconfig() - Set up the uart configuration
	 * (parity, 5/6/7/8 bits word length, stop bits)
	 *
	 * Set up a new config for this device.
	 *
	 * @dev: Device pointer
	 * @serial_config: number of bits, parity and number of stopbits to use
	 * @return 0 if OK, -ve on error
	 */
	int (*setconfig)(struct tty_uart_platdata *plat, uint serial_config);
	/**
	 * getinfo() - Get serial device information
	 *
	 * @dev: Device pointer
	 * @info: struct serial_device_info to fill
	 * @return 0 if OK, -ve on error
	 */
	int (*getinfo)(struct tty_uart_platdata *plat, struct serial_device_info *info);
	int port;
	int nr_port;
	int baudrate;
};

void uart_init(int port, int baudrate);
int uart_setbrg(int port, int baudrate);
int uart_pending(bool input);
int uart_get_baudrate(void);
int uart_get_port(void);

#endif