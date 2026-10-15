#include <unistd.h>
#include <sys/syscalls.h>

namespace {

constexpr int FIRST_FILE_DESCRIPTOR = 3;
constexpr ssize_t FAILURE = -1;

}

extern "C" ssize_t __file_read(int fd, void *buf, size_t count) __attribute__((weak));

ssize_t read(int fd, void *buf, size_t count) {
	if (fd >= FIRST_FILE_DESCRIPTOR) return __file_read != nullptr ? __file_read(fd, buf, count) : FAILURE;
	return (ssize_t)syscall(SYS_READ, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)count, 0, 0, 0);
}
