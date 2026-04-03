#include <unistd.h>
#include <sys/syscalls.h>

ssize_t write(int fd, const void *buf, size_t count) {
    return (ssize_t)syscall(SYS_WRITE, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)count, 0, 0, 0);
}
