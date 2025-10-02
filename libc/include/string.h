#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <sys/cdefs.h>

extern "C" {
int memcpy(void *__restrict dest, const void *__restrict src, int c, size_t n);
void *memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *str, int c, size_t n);
size_t strlen(const char *str);
}

#endif // STRING_H
