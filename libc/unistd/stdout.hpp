/**
 * @file stdout.hpp
 * @brief The buffer of standard output, kept by the C library of a user program.
 *
 * `printf`, `puts` and `putchar` do not write each piece to the console server: they append to a buffer that is
 * written with one `write` when it fills, when it holds a newline and standard output is a terminal, and at the
 * points where the text already printed must be out before something else happens (`flush`). That is one message to
 * the console server for a whole line instead of one per character, and a colored line leaves in one piece.
 *
 * The buffer is written out before: a `write` to any descriptor, a `read`, a `fork`, an `exec`, the end of the
 * process through `exit_process`, and a `close` or `dup2` that replaces descriptor 1. It is not written out when a
 * process dies by a signal or by a fault, and `_exit` does not write it either: the text still in it is lost.
 *
 * Every function is weak where it is used, so a program that did not link `unistd/stdout.cpp` writes straight to
 * descriptor 1 as before.
 */
#ifndef LIBC_UNISTD_STDOUT_HPP
#define LIBC_UNISTD_STDOUT_HPP

#include <stddef.h>

extern "C" {

/**
 * @brief Appends bytes to the buffer of standard output.
 *
 * Writes the buffer out first whenever it is full. When descriptor 1 is a terminal and the bytes hold a newline, the
 * buffer is written out whole after them.
 *
 * @param data The bytes; not a string, no terminator is read.
 * @param count How many bytes.
 * @return The number of bytes taken, which is `count`: a failed write drops the text it carried and is not reported.
 */
size_t __stdout_put(const char *data, size_t count) __attribute__((weak));

/**
 * @brief Writes out what the buffer holds and empties it.
 *
 * Does nothing when the buffer is empty. A failed write drops the text.
 */
void __stdout_flush() __attribute__((weak));

}

#endif // !LIBC_UNISTD_STDOUT_HPP
