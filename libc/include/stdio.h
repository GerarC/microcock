#ifndef STDIO_H
#define STDIO_H

#ifndef EOF
#define EOF (-1)
#endif // !EOF

extern "C" {
int printf(const char *__restrict, ...);
int putchar(int);
int puts(const char *);
}

#endif // !STDIO_H
