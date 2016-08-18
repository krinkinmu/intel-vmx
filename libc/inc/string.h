#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

int memcmp(const void *l, const void *r, size_t size);
void *memcpy(void *dst, const void *src, size_t size);
void *memset(void *dst, int value, size_t size);

size_t strlen(const char *str);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t size);
int strcmp(const char *l, const char *r);
int strncmp(const char *l, const char *r, size_t size);
char *strchr(const char *str, int ch);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t size);
char *strstr(const char *str, const char *substr);

#endif /*__STRING_H__*/
