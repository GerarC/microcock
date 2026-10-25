#include "descriptor.hpp"
#include <errno.h>
#include <stdint.h>
#include <sys/syscalls.h>

extern "C" ssize_t __console_read(int fd, void *buf, size_t count);
extern "C" ssize_t __console_write(int fd, const void *buf, size_t count);

namespace cock::libc {

namespace {

constexpr uintptr_t UNUSED_ARGUMENT = 0;
constexpr uint32_t START_OFFSET = 0;
constexpr uint32_t ONE_REFERENCE = 1;
constexpr int NO_DESCRIPTOR = -1;

struct Slot {
	bool used;
	uint8_t description;
};

ssize_t consoleRead(Description &, void *buffer, size_t count) {
	ssize_t received = __console_read(STDIN_DESCRIPTOR, buffer, count);
	if (received >= 0) return received;
	return static_cast<ssize_t>(syscall(SYS_READ, static_cast<uintptr_t>(STDIN_DESCRIPTOR),
										reinterpret_cast<uintptr_t>(buffer), count, UNUSED_ARGUMENT,
										UNUSED_ARGUMENT, UNUSED_ARGUMENT));
}

ssize_t consoleWrite(Description &, const void *buffer, size_t count) {
	ssize_t written = __console_write(STDOUT_DESCRIPTOR, buffer, count);
	if (written >= 0) return written;
	return static_cast<ssize_t>(syscall(SYS_WRITE, static_cast<uintptr_t>(STDOUT_DESCRIPTOR),
										reinterpret_cast<uintptr_t>(buffer), count, UNUSED_ARGUMENT,
										UNUSED_ARGUMENT, UNUSED_ARGUMENT));
}

off_t consoleSeek(Description &, off_t, int) {
	errno = ESPIPE;
	return FAILURE;
}

int consoleClose(Description &) {
	return CLOSED;
}

constexpr Operations CONSOLE_OPERATIONS = {consoleRead, consoleWrite, consoleSeek, consoleClose};

Description descriptions[MAX_DESCRIPTORS] = {
	{&CONSOLE_OPERATIONS, ONE_REFERENCE, NO_HANDLE, START_OFFSET, true, false, false},
	{&CONSOLE_OPERATIONS, ONE_REFERENCE, NO_HANDLE, START_OFFSET, false, true, false},
	{&CONSOLE_OPERATIONS, ONE_REFERENCE, NO_HANDLE, START_OFFSET, false, true, false},
};

Slot slots[MAX_DESCRIPTORS] = {
	{true, STDIN_DESCRIPTOR},
	{true, STDOUT_DESCRIPTOR},
	{true, STDERR_DESCRIPTOR},
};

int lowestFreeSlot() {
	for (int descriptor = 0; descriptor < MAX_DESCRIPTORS; descriptor++)
		if (!slots[descriptor].used) return descriptor;
	return NO_DESCRIPTOR;
}

int freeDescription() {
	for (int index = 0; index < MAX_DESCRIPTORS; index++)
		if (descriptions[index].operations == nullptr) return index;
	return NO_DESCRIPTOR;
}

int closeDescriptor(int descriptor) {
	Slot &slot = slots[descriptor];
	Description &description = descriptions[slot.description];
	slot.used = false;
	if (--description.references > 0) return CLOSED;

	int result = description.operations->close(description);
	description = {};
	return result;
}

} // namespace

int failWith(int code) {
	errno = code;
	return FAILURE;
}

int openDescription(const Operations &operations, uint32_t handle, bool readable, bool writable,
					bool append) {
	int descriptor = lowestFreeSlot();
	int index = freeDescription();
	if (descriptor == NO_DESCRIPTOR || index == NO_DESCRIPTOR) return failWith(EMFILE);

	descriptions[index] = {&operations, ONE_REFERENCE, handle, START_OFFSET, readable, writable, append};
	slots[descriptor] = {true, static_cast<uint8_t>(index)};
	return descriptor;
}

Description *describe(int descriptor) {
	if (descriptor < 0 || descriptor >= MAX_DESCRIPTORS || !slots[descriptor].used) return nullptr;
	return &descriptions[slots[descriptor].description];
}

void dropDescriptions(const Operations &operations) {
	for (Description &description : descriptions)
		if (description.operations == &operations) description = {};
	for (Slot &slot : slots)
		if (slot.used && descriptions[slot.description].operations == nullptr) slot.used = false;
}

} // namespace cock::libc

using namespace cock::libc;

namespace {

constexpr ssize_t NO_BYTES = 0;

}

extern "C" {

ssize_t __descriptor_read(int fd, void *buf, size_t count) {
	Description *description = describe(fd);
	if (description == nullptr || !description->readable) return failWith(EBADF);
	if (buf == nullptr) return failWith(EFAULT);
	if (count == 0) return NO_BYTES;
	return description->operations->read(*description, buf, count);
}

ssize_t __descriptor_write(int fd, const void *buf, size_t count) {
	Description *description = describe(fd);
	if (description == nullptr || !description->writable) return failWith(EBADF);
	if (buf == nullptr) return failWith(EFAULT);
	return description->operations->write(*description, buf, count);
}

int close(int fd) {
	if (describe(fd) == nullptr) return failWith(EBADF);
	int code = closeDescriptor(fd);
	return code == CLOSED ? CLOSED : failWith(code);
}

off_t lseek(int fd, off_t offset, int whence) {
	Description *description = describe(fd);
	if (description == nullptr) return failWith(EBADF);
	return description->operations->seek(*description, offset, whence);
}

int dup(int fd) {
	Description *description = describe(fd);
	if (description == nullptr) return failWith(EBADF);

	int copy = lowestFreeSlot();
	if (copy == NO_DESCRIPTOR) return failWith(EMFILE);
	slots[copy] = {true, slots[fd].description};
	description->references++;
	return copy;
}

int dup2(int fd, int target) {
	Description *description = describe(fd);
	if (description == nullptr || target < 0 || target >= MAX_DESCRIPTORS) return failWith(EBADF);
	if (fd == target) return target;

	if (slots[target].used) closeDescriptor(target);
	slots[target] = {true, slots[fd].description};
	description->references++;
	return target;
}

}
