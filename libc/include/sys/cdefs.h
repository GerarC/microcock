/**
 * @file cdefs.h
 * @brief Internal definitions shared by the libc headers.
 */
#ifndef SYS_CDEFS_H
#define SYS_CDEFS_H 1

/** Marks that the headers come from the MicroCOCK libc. */
#define __cock_libc 1

/** Keeps a libc function in the link even when no code references it (LTO). */
#define LIBC_KEEP_SYMBOL __attribute__((used, externally_visible))

#endif // !SYS_CDEFS_H
