#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

unsigned long strtoul(const char *str, char **endptr, int base)
{
	const char *ptr = str;
	unsigned long ret = 0;
	int negate = 0, consumed = 0;

	while (isspace(*ptr))
		++ptr;

	if (*ptr == '-' || *ptr == '+') {
		negate = *ptr == '-';
		++ptr;
	}

	if (!base) {
		base = 10;
		if (*ptr == '0') {
			base = 8;
			if (tolower(*(++ptr)) == 'x') {
				base = 16;
				++ptr;
			}
		}
	} else {
		if (base == 8 && *ptr == '0')
			++ptr;
		if (base == 16 && *ptr == '0' && tolower(*(ptr + 1) == 'x'))
			ptr += 2;
	}

	while (*ptr) {
		const char c = toupper(*ptr);
		const int digit = isdigit(c) ? c - '0' : c - 'A' + 10;

		if (ret > (ULONG_MAX - digit) / base) {
			ret = ULONG_MAX;
			break;
		}
		ret = ret * base + digit;
		consumed = 1;
		++ptr;
	}

	if (endptr)
		*endptr = (char *)(consumed ? ptr : str);

	if (negate)
		ret = -ret;

	return ret;
}

char *ulltoa(unsigned long long value, char *str, int base)
{
	static const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	char *ptr = str;
	char *start = str;

	do {
		const int r = value % base;

		value = value / base;
		*ptr++ = digits[r];
	} while (value);

	*ptr-- = '\0';

	while (start < ptr) {
		const char c = *ptr;

		*ptr-- = *start;
		*start++ = c;
	}

	return str;
}

char *lltoa(long long value, char *str, int base)
{
	char *ptr = str;

	if (value < 0) {
		*ptr++ = '-';
		value = -value;
	}

	ulltoa(value, ptr, base);

	return str;
}

char *ultoa(unsigned long value, char *str, int base)
{
	return ulltoa(value, str, base);
}

char *ltoa(long value, char *str, int base)
{
	return lltoa(value, str, base);
}

char *utoa(unsigned value, char *str, int base)
{
	return ulltoa(value, str, base);
}

char *itoa(int value, char *str, int base)
{
	return lltoa(value, str, base);
}

static inline size_t left_child(size_t pos)
{
	return (pos + 1) * 2 - 1;
}

static inline size_t right_child(size_t pos)
{
	return (pos + 1) * 2;
}

static inline void swap(char *l, char *r, size_t size)
{
	for (size_t i = 0; i != size; ++i) {
		const char tmp = l[i];

		l[i] = r[i];
		r[i] = tmp;
	}
}

static void sift_down(char *base, size_t count, size_t size,
			int (*cmp)(const void *, const void *), size_t p)
{
	while (1) {
		const size_t l = left_child(p);
		const size_t r = right_child(p);
		size_t m = p;

		if (l < count && cmp(base + l * size, base + m * size) > 0)
			m = l;

		if (r < count && cmp(base + r * size, base + m * size) > 0)
			m = r;

		if (m == p)
			return;

		swap(base + p * size, base + m * size, size);
		p = m;
	}
}

static void pop_heap(char *base, size_t count, size_t size,
			int (*cmp)(const void *, const void *))
{
	swap(base, base + (count - 1) * size, size);
	sift_down(base, count - 1, size, cmp, 0);
}

static void make_heap(char *base, size_t count, size_t size,
			int (*cmp)(const void *, const void *))
{
	if (count <= 1)
		return;

	for (size_t i = count / 2; i != 0; --i)
		sift_down(base, count, size, cmp, i - 1);
}

void qsort(void *base, size_t count, size_t size,
			int (*cmp)(const void *, const void *))
{
	make_heap(base, count, size, cmp);
	while (count)
		pop_heap(base, count--, size, cmp);
}
