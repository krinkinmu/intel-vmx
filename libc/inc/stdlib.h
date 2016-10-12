#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <stddef.h>

unsigned long strtoul(const char *str, char **endptr, int base);

char *ulltoa(unsigned long long value, char *str, int base);
char *lltoa(long long value, char *str, int base);
char *ultoa(unsigned long value, char *str, int base);
char *ltoa(long value, char *str, int base);
char *utoa(unsigned value, char *str, int base);
char *itoa(int value, char *str, int base);

void qsort(void *base, size_t count, size_t size,
			int (*cmp)(const void *, const void *));

#endif /*__STDLIB_H__*/
