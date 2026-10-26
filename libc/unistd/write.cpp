#include "stdout.hpp"
#include <unistd.h>
#include <sys/syscalls.h>

namespace {

constexpr uintptr_t UNUSED_ARGUMENT = 0;

}

extern "C" ssize_t __descriptor_write(int fd, const void *buf, size_t count) __attribute__((weak));

ssize_t write(int fd, const void *buf, size_t count) {
	if (__stdout_flush != nullptr) __stdout_flush();
	if (__descriptor_write != nullptr) return __descriptor_write(fd, buf, count);
	return (ssize_t)syscall(SYS_WRITE, (uintptr_t)fd, (uintptr_t)buf, (uintptr_t)count, UNUSED_ARGUMENT,
							UNUSED_ARGUMENT, UNUSED_ARGUMENT);
}
