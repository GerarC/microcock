#ifndef FCNTL_H
#define FCNTL_H

#include <sys/cdefs.h>

#define O_ACCMODE 0x0003
#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_CREAT   0x0040
#define O_TRUNC   0x0200
#define O_APPEND  0x0400

#ifdef __cplusplus
extern "C" {
#endif

int open(const char *path, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif // FCNTL_H
