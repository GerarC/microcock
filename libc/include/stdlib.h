/**
 * @file stdlib.h
 * @brief Process termination and dynamic memory.
 *
 * The allocator keeps a sorted free list over anonymous chunks obtained with
 * `mmap`. Blocks are 8 byte aligned. Deviation from ISO C: there is no locking,
 * so the functions are NOT safe to call from several threads at once, and
 * `free` does not return chunks to the kernel. `exit`, `atexit`, `qsort`,
 * `rand` and the number conversions are not provided.
 */
#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <sys/cdefs.h>

extern "C" {
/**
 * @brief Terminates the process abnormally.
 *
 * Writes a diagnostic to the console and exits with status 134 (128 plus
 * SIGABRT); signals do not exist. Never returns.
 */
__attribute__((__noreturn__)) void abort(void);

/**
 * @brief Allocates uninitialized memory.
 * @param size Bytes requested; zero returns a valid minimal block.
 * @return An 8 byte aligned pointer, or null when the size is too large or
 *         the kernel refuses more memory.
 */
void *malloc(size_t size);

/**
 * @brief Allocates zeroed memory for an array.
 * @param count Number of elements.
 * @param size Size of each element.
 * @return Zeroed memory, or null when `count * size` overflows or no memory
 *         is available.
 */
void *calloc(size_t count, size_t size);

/**
 * @brief Changes the size of an allocation, moving it when needed.
 * @param pointer Block from `malloc`, `calloc` or `realloc`; null behaves as
 *                `malloc(size)`.
 * @param size New size; zero frees the block and returns null.
 * @return The (possibly moved) block, or null on failure, in which case the
 *         original block is left valid.
 */
void *realloc(void *pointer, size_t size);

/**
 * @brief Releases a block to the free list.
 * @param pointer Block from this allocator, or null (no effect). Passing any
 *                other pointer, or freeing twice, is undefined.
 */
void free(void *pointer);
}

#endif // !STDLIB_H
