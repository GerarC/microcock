#include <unistd.h>
#include <sys/syscalls.h>

namespace {

constexpr uintptr_t UNUSED_ARGUMENT = 0;

}

extern "C" ssize_t __descriptor_read(int fd, void *buf, size_t count) __attribute__((weak));

ssize_t read(int fd, void *buf, size_t count) {
	if (__descriptor_read != nullptr) return __descriptor_read(fd, buf, count);
	return (ssize_t)syscall(SYS_READ, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)count, UNUSED_ARGUMENT,
							UNUSED_ARGUMENT, UNUSED_ARGUMENT);
}
