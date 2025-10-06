#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <sys/cdefs.h>

extern "C" {
int memcmp(const void *s1, const void *s2, size_t size);
void *memcpy(void *__restrict dest, const void *__restrict src, int c,
			 size_t n);
void *memmove(void *dest, const void *src, size_t size);
void *memset(void *str, int c, size_t n);
size_t strlen(const char *str);
}

#endif // STRING_H
