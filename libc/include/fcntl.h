/**
 * @file fcntl.h
 * @brief File open flags and `open`, backed by the `vfs` file server.
 *
 * Deviation from POSIX: only `open` is provided (no `fcntl`, `creat`), there
 * is no file mode, no `O_EXCL`, `O_NONBLOCK` or `O_CLOEXEC`, and descriptors
 * are private to the calling process (not inherited by `spawn`).
 */
#ifndef FCNTL_H
#define FCNTL_H

#include <sys/cdefs.h>

/** Mask that extracts the access mode from the flags of `open`. */
#define O_ACCMODE 0x0003
/** Open for reading only. */
#define O_RDONLY  0x0000
/** Open for writing only. */
#define O_WRONLY  0x0001
/** Open for reading and writing. */
#define O_RDWR    0x0002
/** Create the file when it does not exist. */
#define O_CREAT   0x0040
/** Truncate the file to length zero on open (needs write access). */
#define O_TRUNC   0x0200
/** Before each write, move the offset to the end of the file. */
#define O_APPEND  0x0400

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opens a file on the virtual file system.
 *
 * Connects to the `vfs` server on first use and takes the lowest free
 * descriptor. Descriptors 0, 1 and 2 are the console until the program closes
 * them, so the first file is normally 3; the table holds 32 descriptors.
 *
 * @param path Absolute path, null terminated. Must not be null.
 * @param flags One access mode (`O_RDONLY`, `O_WRONLY` or `O_RDWR`) combined
 *              with `O_CREAT`, `O_TRUNC` and `O_APPEND`.
 * @param ... Ignored. The POSIX mode argument is accepted and not read.
 * @return The new descriptor (3 or more) or -1 with `errno` set: `EINVAL`
 *         (null path or bad access mode), `EMFILE` (no free slot),
 *         `ENOTCONN` (no file server) or the error reported by the server
 *         (`ENOENT`, `EROFS`, `ENOSPC`, `EIO`...).
 */
int open(const char *path, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif // FCNTL_H
