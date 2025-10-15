#ifndef STDIO_H
#define STDIO_H

#ifndef EOF
#define EOF (-1)
#endif // !EOF

#include <stdarg.h>

extern "C" {
int printf(const char *__restrict, ...);
int putchar(int);
int puts(const char *);
int vprintf(const char *format, va_list args);
}

#endif // !STDIO_H
