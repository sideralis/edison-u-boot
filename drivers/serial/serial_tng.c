/*
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <linux/compiler.h>

#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <watchdog.h>

#define cpu_relax() asm volatile("rep; nop")

#define UART_GLOBAL	0xff010000

#define UART_OFFSET	0x80

#define SERIAL_BASE_ADDR (UART_GLOBAL + (UART_OFFSET * (dev_index + 1)))

#define XMTRDY			0x20
#define LSR_DR		(0x01)

#define DLAB		0x80

#define TXR             0	/*  Transmit register (WRITE) */
#define RXR             0	/*  Receive register  (READ)  */
#define IER             1	/*  Interrupt Enable          */
#define IIR             2	/*  Interrupt ID              */
#define FCR             2	/*  FIFO control              */
#define LCR             3	/*  Line control              */
#define MCR             4	/*  Modem control             */
#define LSR             5	/*  Line Status               */
#define MSR             6	/*  Modem Status              */
#define DLL             0	/*  Divisor Latch Low         */
#define DLH             1	/*  Divisor latch High        */
#define MUL				0x34	/*  DDS Multiplier                        */
#define DIV				0x38	/*  DDS Divisor                           */

/* Multi serial device functions */
#define DECLARE_TNG_SERIAL_FUNCTIONS(port) \
	int tng_serial##port##_init(void) \
	{ \
		return serial_init_dev(port); \
	} \
	int tng_serial##port##_shutdown(void) \
	{ \
		return serial_shutdown_dev(port); \
	} \
	void tng_serial##port##_setbrg(void) \
	{ \
		serial_setbrg_dev(port); \
	} \
	int tng_serial##port##_getc(void) \
	{ \
		return serial_getc_dev(port); \
	} \
	int tng_serial##port##_tstc(void) \
	{ \
		return serial_tstc_dev(port); \
	} \
	void tng_serial##port##_putc(const char c) \
	{ \
		serial_putc_dev(port, c); \
	} \
	void tng_serial##port##_puts(const char *s) \
	{ \
		serial_puts_dev(port, s); \
	}

#define INIT_TNG_SERIAL_STRUCTURE(port, __name) {	\
	.name	= __name,				\
	.start	= tng_serial##port##_init,		\
	.stop	= tng_serial##port##_shutdown,					\
	.setbrg	= tng_serial##port##_setbrg,		\
	.getc	= tng_serial##port##_getc,		\
	.tstc	= tng_serial##port##_tstc,		\
	.putc	= tng_serial##port##_putc,		\
	.puts	= tng_serial##port##_puts,		\
}

#ifdef CONFIG_HWFLOW
static int hwflow;
#endif

void _serial_setbrg(const int dev_index)
{
	return;
}

static inline void serial_setbrg_dev(unsigned int dev_index)
{
	_serial_setbrg(dev_index);
}

/*
 * Initialise the serial port.
 */
static int serial_init_dev(const int dev_index)
{
	unsigned char c;
	unsigned divisor = 16;

	c = readb(SERIAL_BASE_ADDR + LCR);
	writeb(c | DLAB, SERIAL_BASE_ADDR + LCR);
	writeb(divisor & 0xff, SERIAL_BASE_ADDR + DLL);
	writeb((divisor >> 8) & 0xff, SERIAL_BASE_ADDR + DLH);
	writeb(c & ~DLAB, SERIAL_BASE_ADDR + LCR);
	writeb(0x2, SERIAL_BASE_ADDR + IER);	/* TIE enable */
	writeb(0x3, SERIAL_BASE_ADDR + LCR);	/* 8n1 */
	writeb(0, SERIAL_BASE_ADDR + FCR);	/* no fifo */
	writeb(0x3, SERIAL_BASE_ADDR + MCR);	/* DTR + RTS */
	writeb(7, SERIAL_BASE_ADDR + FCR);
	writel(0x2ee0, SERIAL_BASE_ADDR + MUL);
	writel(0x3d09, SERIAL_BASE_ADDR + DIV);

	return 0;
}

/*
 * Shutdown the serial port.
 */
static int serial_shutdown_dev(const int dev_index)
{
	unsigned char c;

	writeb(0x0, SERIAL_BASE_ADDR + IER);	/* TIE disable */
	c = readb(SERIAL_BASE_ADDR + MCR);
	writeb(c & ~0x08, SERIAL_BASE_ADDR + MCR);	/* DTR + RTS */
	c = readb(SERIAL_BASE_ADDR + LCR);
	writeb(c & ~0x40, SERIAL_BASE_ADDR + LCR);
	writeb(0x7, SERIAL_BASE_ADDR + FCR);
	writeb(0x0, SERIAL_BASE_ADDR + FCR);	/* no fifo */

	return 0;
}

/*
 * Read a single byte from the serial port.
 */
int _serial_getc(const int dev_index)
{
	if ((readb(SERIAL_BASE_ADDR + LSR) & LSR_DR) == 0)
		return -EAGAIN;

	return readb(SERIAL_BASE_ADDR + RXR) & 0xff;
}

static inline int serial_getc_dev(unsigned int dev_index)
{
	return _serial_getc(dev_index);
}

/*
 * Output a single byte to the serial port.
 */
int _serial_putc(const char c, const int dev_index)
{
	if ((readb(SERIAL_BASE_ADDR + LSR) & XMTRDY) == 0)
		return -EAGAIN;

	writeb(c, SERIAL_BASE_ADDR + TXR);

	/* If \n, also do \r */
	if (c == '\n') {
		WATCHDOG_RESET();
		serial_putc('\r');
	}

	return 0;
}

static inline int serial_putc_dev(unsigned int dev_index, const char c)
{
	return _serial_putc(c, dev_index);
}

/*
 * Test whether a character is in the RX buffer
 */
int _serial_tstc(const int dev_index)
{
	return (readb(SERIAL_BASE_ADDR + LSR) & LSR_DR) != 0;
}

static inline int serial_tstc_dev(unsigned int dev_index)
{
	return _serial_tstc(dev_index);
}

void _serial_puts(const char *s, const int dev_index)
{
	while (*s) {
		_serial_putc(*s++, dev_index);
	}
	return;
}

static inline void serial_puts_dev(int dev_index, const char *s)
{
	return _serial_puts(s, dev_index);
}

#if defined(CONFIG_SYS_TNG_SERIAL0)
DECLARE_TNG_SERIAL_FUNCTIONS(0);
#elif defined(CONFIG_SYS_TNG_SERIAL1)
DECLARE_TNG_SERIAL_FUNCTIONS(1);
#elif defined(CONFIG_SYS_TNG_SERIAL2)
DECLARE_TNG_SERIAL_FUNCTIONS(2);
#endif

static int tng_serial_probe(struct udevice *dev) {
#if defined(CONFIG_SYS_TNG_SERIAL0)
	serial_init_dev(0);
#elif defined(CONFIG_SYS_TNG_SERIAL1)
	serial_init_dev(1);
#elif defined(CONFIG_SYS_TNG_SERIAL2)
	serial_init_dev(2);
#endif
	return 0;
}

static int tng_serial_putc(struct udevice *dev, const char ch) {
#if defined(CONFIG_SYS_TNG_SERIAL0)
	return serial_putc_dev(0, ch);
#elif defined(CONFIG_SYS_TNG_SERIAL1)
	return serial_putc_dev(1, ch);
#elif defined(CONFIG_SYS_TNG_SERIAL2)
	return serial_putc_dev(2, ch);
#else
	#error
#endif
}

static int tng_serial_getc(struct udevice *dev) {
#if defined(CONFIG_SYS_TNG_SERIAL0)
	return serial_getc_dev(0);
#elif defined(CONFIG_SYS_TNG_SERIAL1)
	return serial_getc_dev(1);
#elif defined(CONFIG_SYS_TNG_SERIAL2)
	return serial_getc_dev(2);
#else
	#error
#endif
}

static int tng_serial_pending(struct udevice *dev, bool input)
{
	if (input)
#if defined(CONFIG_SYS_TNG_SERIAL0)
		return serial_tstc_dev(0);
#elif defined(CONFIG_SYS_TNG_SERIAL1)
		return serial_tstc_dev(1);
#elif defined(CONFIG_SYS_TNG_SERIAL2)
		return serial_tstc_dev(2);
#else
	#error
#endif
	else
		return 0;
}

const struct dm_serial_ops tng_serial_ops = {
	.getc = tng_serial_getc,
	.pending = tng_serial_pending,
	.putc = tng_serial_putc,
};


static const struct udevice_id tng_serial_ids[] = {
	{ .compatible = "intel,tng-uart" },
	{ }
};

U_BOOT_DRIVER(serial_tng_drv) = {
	.name	= "serial_tng_drv",
	.id	= UCLASS_SERIAL,
	.of_match = tng_serial_ids,
	.probe  = tng_serial_probe,
	.ops	= &tng_serial_ops,
	.priv_auto_alloc_size = sizeof(int),
};
