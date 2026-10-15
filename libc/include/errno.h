#ifndef ERRNO_H
#define ERRNO_H

#include <sys/cdefs.h>

#define ENOENT       2
#define EIO          5
#define EBADF        9
#define EACCES       13
#define EFAULT       14
#define EBUSY        16
#define EEXIST       17
#define EXDEV        18
#define EINVAL       22
#define EMFILE       24
#define ENOSPC       28
#define EROFS        30
#define ENOTEMPTY    39
#define ENOTSUP      95
#define ENOTCONN     107

#ifdef __cplusplus
extern "C" {
#endif

extern int errno;

#ifdef __cplusplus
}
#endif

#endif // ERRNO_H
