#include <unistd.h>
#include <sys/syscalls.h>

namespace {

constexpr int FIRST_FILE_DESCRIPTOR = 3;
constexpr ssize_t FAILURE = -1;

}

extern "C" ssize_t __file_write(int fd, const void *buf, size_t count) __attribute__((weak));
extern "C" ssize_t __console_write(int fd, const void *buf, size_t count) __attribute__((weak));

ssize_t write(int fd, const void *buf, size_t count) {
    if (fd >= FIRST_FILE_DESCRIPTOR) return __file_write != nullptr ? __file_write(fd, buf, count) : FAILURE;
    if (__console_write != nullptr) {
        ssize_t written = __console_write(fd, buf, count);
        if (written >= 0) return written;
    }
    return (ssize_t)syscall(SYS_WRITE, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)count, 0, 0, 0);
}
