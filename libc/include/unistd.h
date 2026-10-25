// libc/include/unistd.h
/**
 * @file unistd.h
 * @brief Descriptor based I/O and process exit.
 *
 * A process has a table of at most 32 descriptors, kept by the library. Each
 * descriptor names an open file description, and `dup` and `dup2` make
 * several descriptors name the same one, which then shares its offset.
 * Descriptors 0, 1 and 2 are born as stdin, stdout and stderr served by the
 * console server (with the kernel debug console as fallback); they can be
 * closed and replaced like any other. The rest are files opened through the
 * `vfs` server, which the library offsets itself.
 * Offsets are 31 bit, so files are limited to 2 GiB minus one byte.
 */
#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <sys/cdefs.h>

/** `lseek` origin: from the start of the file. */
#define SEEK_SET 0
/** `lseek` origin: from the current offset. */
#define SEEK_CUR 1
/** `lseek` origin: from the end of the file. */
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

/** Signed byte count (32 bit on i386). */
typedef int ssize_t;
/** Signed file offset (32 bit on i386). */
typedef int off_t;

/**
 * @brief Reads from a descriptor.
 * @param fd An open descriptor; 0 is the console until it is replaced.
 * @param buf Destination, at least `count` bytes.
 * @param count Maximum bytes to read.
 * @return Bytes read (0 at end of file), or -1 with `errno` set (`EBADF` when
 *         the descriptor is not open or not open for reading, `EFAULT` for a
 *         null buffer, or the error of the server).
 */
ssize_t read(int fd, void *buf, size_t count);

/**
 * @brief Writes to a descriptor.
 * @param fd An open descriptor; 1 and 2 are the console until replaced.
 * @param buf Source, at least `count` bytes.
 * @param count Bytes to write.
 * @return Bytes written (can be less than `count` when the device fills up),
 *         or -1 with `errno` set (`EBADF` when the descriptor is not open or
 *         not open for writing, `EFAULT`, `ENOSPC`, `EROFS`...).
 */
ssize_t write(int fd, const void *buf, size_t count);

/**
 * @brief Closes a descriptor and frees its slot.
 *
 * The description behind it is closed at the server only when no other
 * descriptor names it.
 * @param fd An open descriptor.
 * @return 0, or -1 with `errno` set (`EBADF`, or the server error). The slot
 *         is released even when the server reports a failure.
 */
int close(int fd);

/**
 * @brief Moves the offset of an open file.
 * @param fd A descriptor that names a file.
 * @param offset Displacement in bytes, may be negative.
 * @param whence `SEEK_SET`, `SEEK_CUR` or `SEEK_END`.
 * @return The new offset, or -1 with `errno` set: `EBADF`, `ESPIPE` for the
 *         console, `EINVAL` (bad whence or result outside 0 to 0x7FFFFFFF).
 */
off_t lseek(int fd, off_t offset, int whence);

/**
 * @brief Makes a new descriptor name the same open file description.
 * @param fd An open descriptor.
 * @return The lowest free descriptor, or -1 with `errno` set (`EBADF`,
 *         `EMFILE`). Both descriptors share the offset and the access mode.
 */
int dup(int fd);

/**
 * @brief Makes a chosen descriptor name the same open file description.
 *
 * If `target` is open it is closed first, and the result of that close is
 * ignored.
 * @param fd An open descriptor.
 * @param target The descriptor to use, from 0 to 31.
 * @return `target`, or -1 with `errno` set (`EBADF` when `fd` is not open or
 *         `target` is out of range). When both are the same open descriptor
 *         nothing changes.
 */
int dup2(int fd, int target);

/**
 * @brief Removes a file name.
 * @param path Absolute path, null terminated.
 * @return 0, or -1 with `errno` set (`EINVAL`, `ENOTCONN`, `ENOENT`,
 *         `EBUSY`, `EROFS`...).
 */
int unlink(const char *path);

/**
 * @brief Removes an empty directory.
 * @param path Absolute path, null terminated.
 * @return 0, or -1 with `errno` set (`ENOTEMPTY`, `ENOENT`...).
 */
int rmdir(const char *path);

/**
 * @brief Renames or moves a file or directory.
 * @param from Existing absolute path.
 * @param to New absolute path; both must be on the same mount.
 * @return 0, or -1 with `errno` set (`EXDEV` across mounts, `EEXIST`,
 *         `ENOENT`, `EINVAL` for a null path...).
 */
int rename(const char *from, const char *to);

/**
 * @brief Creates a hard link.
 * @param existing Absolute path of an existing file.
 * @param name Absolute path of the new name, on the same mount.
 * @return 0, or -1 with `errno` set (`EXDEV`, `EEXIST`, `ENOTSUP` on file
 *         systems without links...).
 */
int link(const char *existing, const char *name);

/**
 * @brief Terminates the calling process immediately.
 *
 * Standard `_exit` semantics: no handlers or buffers are flushed (including
 * the `printf` buffer, which is flushed on each call anyway).
 * @param status Exit status handed to the kernel.
 */
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif // UNISTD_H
