// libc/include/unistd.h
#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <sys/cdefs.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

typedef int ssize_t;
typedef int off_t;
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int rmdir(const char *path);
int rename(const char *from, const char *to);
int link(const char *existing, const char *name);
__attribute__((__noreturn__)) void _exit(int status);

#ifdef __cplusplus
}
#endif

#endif // UNISTD_H
