#include <print.h>
#include <string.h>
#include <stdlib.h>
#include <uart8250.h>


static int print(struct print_ctx *ctx, const char *buf, int size)
{
	const int rc = ctx->out(ctx, buf, size);

	if (rc < 0)
		return rc;

	ctx->written += size;
	return 0;
}

enum format_type {
	FMT_INVALID,
	FMT_NONE,
	FMT_STR,
	FMT_CHAR,
	FMT_INT,
	FMT_PERCENT
};

static int decode_format(const char **fmt_ptr)
{
	const char *fmt = *fmt_ptr;
	int type = FMT_INVALID;

	while (*fmt) {
		if (*fmt == '%' && type != FMT_INVALID)
			break;

		const char c = *fmt++;

		if (c != '%') {
			type = FMT_NONE;
			continue;
		}

		switch (*fmt++) {
		case 'd':
			type = FMT_INT;
			break;
		case 's':
			type = FMT_STR;
			break;
		case 'c':
			type = FMT_CHAR;
			break;
		case '%':
			type = FMT_PERCENT;
			break;
		}
		break;
	}
	*fmt_ptr = fmt;

	return type;
}

int __vprintf(struct print_ctx *ctx, const char *fmt, va_list args)
{
	char fmt_buf[64];
	int rc = 0;

	ctx->written = 0;

	while (rc >= 0 && *fmt) {
		const char *start = fmt;
		const int type = decode_format(&fmt);

		switch (type) {
		case FMT_STR: {
			const char *str = va_arg(args, const char *);

			rc = print(ctx, str, strlen(str));
			break;
		}
		case FMT_CHAR: {
			const char c = va_arg(args, int);

			rc = print(ctx, &c, 1);
			break;
		}
		case FMT_INT: {
			const int value = va_arg(args, int);

			itoa(value, fmt_buf, 10);
			rc = print(ctx, fmt_buf, strlen(fmt_buf));
			break;
		}
		case FMT_NONE:
			rc = print(ctx, start, fmt - start);
			break;
		case FMT_PERCENT:
			rc = print(ctx, "%", 1);
			break;
		default:
			rc = -1;
			break;
		}
	}

	return rc < 0 ? rc : ctx->written;
}

static int uart_out(struct print_ctx *ctx, const char *buf, int size)
{
	(void) ctx;

	uart8250_write(buf, size);

	return 0;
}

int vprintf(const char *fmt, va_list args)
{
	struct print_ctx ctx = {
		.out = &uart_out,
		.written = 0
	};

	return __vprintf(&ctx, fmt, args);
}

struct print_str_ctx {
	struct print_ctx ctx;
	char *buf;
	int size;
};

static int str_out(struct print_ctx *ctx, const char *buf, int size)
{
	struct print_str_ctx *sctx = (struct print_str_ctx *)ctx;

	if (ctx->written > sctx->size)
		return 0;

	const int remain = size < sctx->size - ctx->written
			? size : sctx->size - ctx->written;
	memcpy(sctx->buf + ctx->written, buf, remain);

	return 0;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	struct print_str_ctx ctx = {
		.ctx = {.out = &str_out},
		.buf = buf,
		.size = (int)size
	};
	const int rc = __vprintf(&ctx.ctx, fmt, args);

	if (rc >= 0)
		buf[rc < (int)size ? rc - 1 : (int)size - 1] = '\0';
	return rc;
}

int printf(const char *fmt, ...)
{
	va_list args;
	int rc;

	va_start(args, fmt);
	rc = vprintf(fmt, args);
	va_end(args);

	return rc;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int rc;

	va_start(args, fmt);
	rc = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return rc;
}
