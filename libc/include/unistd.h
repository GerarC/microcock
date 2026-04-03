// libc/include/unistd.h
#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ssize_t;
ssize_t write(int fd, const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif // UNISTD_H
