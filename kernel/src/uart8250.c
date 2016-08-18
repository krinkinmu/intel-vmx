#include <stddef.h>
#include <ioport.h>

#define UART_BASE		0x3f8
#define UART_REG(reg)		(UART_BASE + reg)
#define UART_LATCH(rate)	(115200 / rate)
#define UART_RATE		9600

/* registers */
#define UART_THR	0
#define UART_RBR	0
#define UART_DLLR	0
#define UART_IER	1
#define UART_DLHR	1
#define UART_IIR	2
#define UART_FCR	2
#define UART_LCR	3
#define UART_MCR	4
#define UART_LSR	5
#define UART_MSR	6
#define UART_SR		7

void uart8250_setup(void)
{
	const unsigned latch_value = UART_LATCH(UART_RATE);

	out8(UART_REG(UART_IER), 0);
	out8(UART_REG(UART_LCR), 1 << 7);
	out8(UART_REG(UART_DLLR), latch_value & 0xff);
	out8(UART_REG(UART_DLHR), (latch_value >> 8) & 0xff);
	out8(UART_REG(UART_LCR), (1 << 1) | (1 << 0));
}

void uart8250_write(const void *src, size_t size)
{
	const unsigned char *ptr = src;

	for (size_t i = 0; i != size; ++i) {
		while (!(in8(UART_REG(UART_LSR)) & (1 << 5)));
		out8(UART_REG(UART_THR), *ptr++);
	}
}

/* If you use this function you have to know how much bytes you
 * want to read, otherwise use interrupt driven IO. Barely useful. */
void uart8250_read(void *dst, size_t size)
{
	unsigned char *ptr = dst;

	for (size_t i = 0; i != size; ++i) {
		while (!(in8(UART_REG(UART_LSR) & (1 << 0))));
		*ptr++ = in8(UART_REG(UART_RBR));
	}
}
