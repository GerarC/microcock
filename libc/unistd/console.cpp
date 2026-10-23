#include <cock/user/console.hpp>
#include <stddef.h>
#include <unistd.h>

namespace {

constexpr int STDIN_DESCRIPTOR = 0;
constexpr int STDOUT_DESCRIPTOR = 1;
constexpr int STDERR_DESCRIPTOR = 2;
constexpr ssize_t FAILURE = -1;

cock::user::console::Client client;

}

extern "C" ssize_t __console_write(int fd, const void *buf, size_t count) {
	if (fd != STDOUT_DESCRIPTOR && fd != STDERR_DESCRIPTOR) return FAILURE;
	if (!client.connect()) return FAILURE;
	return static_cast<ssize_t>(client.write(static_cast<const char *>(buf), count));
}

extern "C" ssize_t __console_read(int fd, void *buf, size_t count) {
	if (fd != STDIN_DESCRIPTOR) return FAILURE;
	if (!client.connect()) return FAILURE;
	return static_cast<ssize_t>(client.read(static_cast<char *>(buf), count));
}
