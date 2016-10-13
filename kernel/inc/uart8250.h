#ifndef __UART8250_H__
#define __UART8250_H__

#include <stddef.h>

void uart8250_setup(void);
void uart8250_write(const void *src, size_t size);
void uart8250_read(void *dst, size_t size);

#endif /*__UART8250_H__*/
