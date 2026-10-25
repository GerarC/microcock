/**
 * @file stat.h
 * @brief Directory creation. `stat` and `struct stat` are not provided.
 */
#ifndef SYS_STAT_H
#define SYS_STAT_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/** File mode type. Accepted for compatibility and ignored. */
typedef unsigned int mode_t;

/**
 * @brief Creates a directory through the `vfs` server.
 * @param path Path, null terminated, absolute or relative to the working directory.
 * @param mode Ignored: there are no permission bits.
 * @return 0 on success, or -1 with `errno` set: `EINVAL` (null path),
 *         `ENOTCONN`, or the server error (`EEXIST`, `ENOENT`, `ENOSPC`,
 *         `EROFS`...).
 */
int mkdir(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // SYS_STAT_H
