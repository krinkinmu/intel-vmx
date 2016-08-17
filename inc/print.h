#ifndef __PRINT_H__
#define __PRINT_H__

#include <stdarg.h>
#include <stddef.h>

struct print_ctx {
	int (*out)(struct print_ctx *ctx, const char *buf, int size);
	int written;
};

int __vprintf(struct print_ctx *ctx, const char *fmt, va_list args);

int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);

#endif /*__PRINT_H__*/
