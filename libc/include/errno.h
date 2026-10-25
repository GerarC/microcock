/**
 * @file errno.h
 * @brief Error numbers and the global `errno` of the user C library.
 *
 * Only the codes that this libc can produce are defined. The numeric values
 * follow the Linux/POSIX numbering. Calls that fail return -1 (or another
 * documented sentinel) and store one of these codes in `errno`; successful
 * calls leave `errno` untouched. Deviation from ISO C: `errno` is a plain
 * global `int`, not thread local, so it is shared by all threads of a process.
 */
#ifndef ERRNO_H
#define ERRNO_H

#include <sys/cdefs.h>

/** No such file or directory: the path does not exist on the file server. */
#define ENOENT       2
/** Input/output error: the block driver or the file server failed. */
#define EIO          5
/** Bad file descriptor: not open, or not open for the requested access. */
#define EBADF        9
/** Permission denied. */
#define EACCES       13
/** Bad address: a required buffer pointer was null. */
#define EFAULT       14
/** Resource busy: the object is in use (open or mounted). */
#define EBUSY        16
/** File exists: the target of a create, rename or link already exists. */
#define EEXIST       17
/** Cross-device link: rename or link between two different mounts. */
#define EXDEV        18
/** Invalid argument: null path, bad flags, bad whence or out of range value. */
#define EINVAL       22
/** Too many open files: all of the 32 descriptor slots of the process are in use. */
#define EMFILE       24
/** No space left on the device. */
#define ENOSPC       28
/** Read-only file system. */
#define EROFS        30
/** Illegal seek: the descriptor is the console or another object with no offset. */
#define ESPIPE       29
/** Directory not empty: rmdir or rename onto a directory that has entries. */
#define ENOTEMPTY    39
/** Operation not supported by the file system or the server. */
#define ENOTSUP      95
/** Not connected: the file server could not be reached. */
#define ENOTCONN     107

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error code of the last failed libc call.
 *
 * Set by the file calls (open, close, dup, dup2, read, write, lseek, unlink,
 * mkdir, rmdir, rename, link) when they return -1. Never cleared by the library.
 */
extern int errno;

#ifdef __cplusplus
}
#endif

#endif // ERRNO_H
