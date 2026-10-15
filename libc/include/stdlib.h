#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <sys/cdefs.h>

extern "C" {
__attribute__((__noreturn__)) void abort(void);
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *pointer, size_t size);
void free(void *pointer);
}

#endif // !STDLIB_H
