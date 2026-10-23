/**
 * @file string.h
 * @brief Memory and string primitives.
 *
 * Standard ISO C semantics. Only the functions that the kernel and the user
 * programs need are provided (no `strcpy`, `strncmp`, `strchr`, `strerror`).
 * The same sources are built for the kernel (libk) and for user space.
 */
#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <sys/cdefs.h>

extern "C" {
/**
 * @brief Compares two memory areas byte by byte as unsigned values.
 * @param s1 First area, at least `size` bytes.
 * @param s2 Second area, at least `size` bytes.
 * @param size Number of bytes to compare.
 * @return -1, 0 or 1 when the first differing byte of `s1` is lower, equal
 *         (no difference) or greater. Deviation: the magnitude is not the
 *         byte difference.
 */
int memcmp(const void *s1, const void *s2, size_t size);

/**
 * @brief Copies bytes between non overlapping areas.
 * @param dest Destination, at least `n` bytes.
 * @param src Source, at least `n` bytes; must not overlap `dest`.
 * @param n Number of bytes.
 * @return `dest`.
 */
void *memcpy(void *__restrict dest, const void *__restrict src,
			 size_t n);

/**
 * @brief Copies bytes between areas that may overlap.
 * @param dest Destination, at least `size` bytes.
 * @param src Source, at least `size` bytes.
 * @param size Number of bytes.
 * @return `dest`.
 */
void *memmove(void *dest, const void *src, size_t size);

/**
 * @brief Fills memory with a byte value.
 * @param str Area to fill, at least `n` bytes.
 * @param c Value, converted to `unsigned char`.
 * @param n Number of bytes.
 * @return `str`.
 */
void *memset(void *str, int c, size_t n);

/**
 * @brief Length of a null terminated string.
 * @param str String, must not be null.
 * @return Number of bytes before the terminator.
 */
size_t strlen(const char *str);

/**
 * @brief Compares two null terminated strings.
 * @param s1 First string.
 * @param s2 Second string.
 * @return Zero when equal; otherwise the difference of the first differing
 *         bytes taken as unsigned characters (negative when `s1` is lower).
 */
int strcmp(const char *s1, const char *s2);
}

#endif // STRING_H
