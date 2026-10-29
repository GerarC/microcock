#include "stdout.hpp"
#include <cock/user/mutex.hpp>
#include <stdint.h>
#include <sys/syscalls.h>
#include <unistd.h>

extern "C" ssize_t __descriptor_write(int fd, const void *buf, size_t count) __attribute__((weak));
extern "C" bool __descriptor_is_terminal(int fd) __attribute__((weak));

namespace {

constexpr int STANDARD_OUTPUT = 1;
constexpr size_t BUFFER_BYTES = 512;
constexpr char NEWLINE = '\n';
constexpr size_t NOTHING = 0;
constexpr uintptr_t UNUSED_ARGUMENT = 0;

cock::user::Mutex bufferLock;
char buffer[BUFFER_BYTES];
size_t used = NOTHING;

ssize_t rawWrite(const char *data, size_t count) {
	if (__descriptor_write != nullptr) return __descriptor_write(STANDARD_OUTPUT, data, count);
	return static_cast<ssize_t>(syscall(SYS_WRITE, static_cast<uintptr_t>(STANDARD_OUTPUT),
										reinterpret_cast<uintptr_t>(data), count, UNUSED_ARGUMENT,
										UNUSED_ARGUMENT, UNUSED_ARGUMENT));
}

void drain() {
	size_t sent = NOTHING;
	while (sent < used) {
		ssize_t step = rawWrite(buffer + sent, used - sent);
		if (step <= 0) break;
		sent += static_cast<size_t>(step);
	}
	used = NOTHING;
}

bool isTerminal() {
	return __descriptor_is_terminal == nullptr || __descriptor_is_terminal(STANDARD_OUTPUT);
}

}

extern "C" size_t __stdout_put(const char *data, size_t count) {
	bufferLock.lock();
	bool sawNewline = false;
	for (size_t index = NOTHING; index < count; index++) {
		if (used == BUFFER_BYTES) drain();
		buffer[used++] = data[index];
		sawNewline = sawNewline || data[index] == NEWLINE;
	}
	if (sawNewline && isTerminal()) drain();
	bufferLock.unlock();
	return count;
}

extern "C" void __stdout_flush() {
	bufferLock.lock();
	if (used != NOTHING) drain();
	bufferLock.unlock();
}
