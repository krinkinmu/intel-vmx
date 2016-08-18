#include <string.h>

int memcmp(const void *l, const void *r, size_t size)
{
	const char *ptr1 = l;
	const char *ptr2 = r;

	while (size--) {
		if (*ptr1 != *ptr2)
			return *ptr1 < *ptr2 ? -1 : 1;
		ptr1++;
		ptr2++;
	}
	return 0;
}

void *memcpy(void *dst, const void *src, size_t size)
{
	const char *ptr2 = src;
	char *ptr1 = dst;

	while (size--)
		*ptr1++ = *ptr2++;

	return dst;
}

void *memset(void *dst, int value, size_t size)
{
	char *ptr = dst;

	while (size--)
		*ptr++ = value;

	return dst;
}

size_t strlen(const char *str)
{
	const char *ptr = str;

	while (*ptr)
		++ptr;

	return ptr - str;
}

char *strcpy(char *dst, const char *src)
{
	char *ptr = dst;

	while (*src)
		*ptr++ = *src++;
	*ptr = '\0';

	return dst;
}

char *strncpy(char *dst, const char *src, size_t size)
{
	char *ptr = dst;

	while (size && *src) {
		*ptr++ = *src++;
		--size;
	}
	memset(ptr, '\0', size);

	return dst;
}

int strcmp(const char *l, const char *r)
{
	while (*l == *r) {
		if (!*l)
			return 0;
		++l;
		++r;
	}

	return *l < *r ? -1 : 1;
}

int strncmp(const char *l, const char *r, size_t size)
{
	while (size && *l == *r) {
		if (!*l)
			return 0;
		--l;
		--r;
		--size;
	}

	if (!size)
		return 0;

	return *l < *r ? -1 : 1;
}

char *strchr(const char *str, int ch)
{
	while (*str) {
		if (*str == ch)
			return (char *)str;
		++str;
	}
	return NULL;
}

char *strcat(char *dst, const char *src)
{
	strcpy(strchr(dst, '\0'), src);

	return dst;
}

char *strncat(char *dst, const char *src, size_t size)
{
	strncpy(strchr(dst, '\0'), src, size);

	return dst;
}

char *strstr(const char *str, const char *substr)
{
	const size_t sslen = strlen(substr);
	const size_t slen = strlen(str);

	if (!sslen)
		return (char *)str;

	if (sslen > slen)
		return NULL;

	for (size_t pos = 0; pos != slen - sslen + 1; ++pos) {
		if (!memcmp(str + pos, substr, sslen))
			return (char *)str + pos;
	}

	return NULL;
}
