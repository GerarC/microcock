/**
 * @file stdio.h
 * @brief Minimal formatted output to the console (standard output).
 *
 * Deviation from ISO C: there is no `FILE`, `stdout`, `fprintf` or input
 * function. Output goes through `write(1, ...)`, which reaches the console
 * server (or the kernel debug console as fallback). In a user program the text
 * goes through a buffer of standard output, written out at a newline when
 * standard output is a terminal, when it fills, and before a read, a write, a
 * `fork`, an `exec` and the end of the process.
 */
#ifndef STDIO_H
#define STDIO_H

/** End of file or error indicator returned by the character functions. */
#ifndef EOF
#define EOF (-1)
#endif // !EOF

#include <stdarg.h>

extern "C" {
/**
 * @brief Formats text and writes it to standard output.
 *
 * Supported conversions: `%d` (signed decimal), `%u` (unsigned decimal),
 * `%x` (lowercase hexadecimal), `%o` (octal), `%p` (pointer as `0x` plus
 * hexadecimal), `%c`, `%s` (a null pointer prints a placeholder text) and
 * `%%`. Width, precision, flags and length modifiers are NOT supported: an
 * unknown conversion is copied verbatim. Arguments are read as 32 bit values.
 * Output is buffered in a fixed size buffer and written in chunks.
 *
 * @param format Format string, null terminated.
 * @return Number of characters produced, or -1 when a write failed.
 */
int printf(const char *__restrict, ...);

/**
 * @brief Writes one character to standard output.
 * @param character Character to write, converted to `char`.
 * @return The character passed in. Write errors are not reported.
 */
int putchar(int);

/**
 * @brief Writes a string and a newline to standard output.
 * @param string Null terminated string.
 * @return Number of characters written by the underlying `printf`, or -1.
 */
int puts(const char *);

/**
 * @brief `printf` taking an already started argument list.
 * @param format Format string, same conversions as `printf`.
 * @param args Argument list; the caller must `va_end` it.
 * @return Number of characters produced, or -1 when a write failed.
 */
int vprintf(const char *format, va_list args);
}

#endif // !STDIO_H
