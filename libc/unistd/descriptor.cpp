#include "descriptor.hpp"
#include <errno.h>
#include <stdint.h>
#include <sys/syscalls.h>

extern "C" ssize_t __console_read(int fd, void *buf, size_t count);
extern "C" ssize_t __console_write(int fd, const void *buf, size_t count);
extern "C" const cock::libc::Operations *__file_operations() __attribute__((weak));
extern "C" const cock::libc::Operations *__pipe_operations() __attribute__((weak));
extern "C" void __file_inherited() __attribute__((weak));

namespace cock::libc {

namespace {

constexpr uintptr_t UNUSED_ARGUMENT = 0;
constexpr uint32_t START_OFFSET = 0;
constexpr uint32_t ONE_REFERENCE = 1;
constexpr int NO_DESCRIPTOR = -1;
constexpr uint32_t NO_FLAGS = 0;

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

constexpr Operations CONSOLE_OPERATIONS = {cock::abi::exec::KIND_CONSOLE, consoleRead, consoleWrite,
										   consoleSeek, consoleClose};

const Operations *operationsOf(uint32_t kind) {
	switch (kind) {
	case cock::abi::exec::KIND_CONSOLE:
		return &CONSOLE_OPERATIONS;
	case cock::abi::exec::KIND_FILE:
		return __file_operations != nullptr ? __file_operations() : nullptr;
	case cock::abi::exec::KIND_PIPE:
		return __pipe_operations != nullptr ? __pipe_operations() : nullptr;
	}
	return nullptr;
}

uint32_t flagsOf(const Description &description) {
	return (description.readable ? cock::abi::exec::FLAG_READABLE : NO_FLAGS) |
		   (description.writable ? cock::abi::exec::FLAG_WRITABLE : NO_FLAGS) |
		   (description.append ? cock::abi::exec::FLAG_APPEND : NO_FLAGS);
}

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
constexpr uint32_t NO_DESCRIPTORS = 0;
constexpr uint32_t DESCRIPTORS_HANDED_ON = 1;
constexpr uint32_t NO_REFERENCES = 0;

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

void __descriptors_export(cock::abi::exec::ArgsBlock *block) {
	block->hasDescriptors = DESCRIPTORS_HANDED_ON;
	for (size_t index = 0; index < cock::abi::exec::DESCRIPTOR_COUNT; index++) {
		block->descriptions[index] = {};
		block->slots[index] = cock::abi::exec::NO_DESCRIPTION;
	}

	for (size_t index = 0; index < cock::abi::exec::DESCRIPTOR_COUNT; index++) {
		const Description &description = descriptions[index];
		if (description.operations == nullptr) continue;
		block->descriptions[index] = {description.operations->kind, description.handle, description.offset,
									  flagsOf(description)};
	}
	for (size_t descriptor = 0; descriptor < cock::abi::exec::DESCRIPTOR_COUNT; descriptor++)
		if (slots[descriptor].used) block->slots[descriptor] = slots[descriptor].description;
}

void __descriptors_import(const cock::abi::exec::ArgsBlock *block) {
	if (block->hasDescriptors == NO_DESCRIPTORS) return;

	for (size_t index = 0; index < cock::abi::exec::DESCRIPTOR_COUNT; index++) {
		descriptions[index] = {};
		slots[index] = {};
	}

	bool hasFiles = false;
	for (size_t index = 0; index < cock::abi::exec::DESCRIPTOR_COUNT; index++) {
		const cock::abi::exec::DescriptorRecord &record = block->descriptions[index];
		const Operations *operations = operationsOf(record.kind);
		if (operations == nullptr) continue;

		descriptions[index] = {operations,
							   NO_REFERENCES,
							   record.handle,
							   record.offset,
							   (record.flags & cock::abi::exec::FLAG_READABLE) != 0,
							   (record.flags & cock::abi::exec::FLAG_WRITABLE) != 0,
							   (record.flags & cock::abi::exec::FLAG_APPEND) != 0};
		hasFiles = hasFiles || record.kind == cock::abi::exec::KIND_FILE;
	}

	for (size_t descriptor = 0; descriptor < cock::abi::exec::DESCRIPTOR_COUNT; descriptor++) {
		uint8_t index = block->slots[descriptor];
		if (index == cock::abi::exec::NO_DESCRIPTION || descriptions[index].operations == nullptr) continue;
		slots[descriptor] = {true, index};
		descriptions[index].references++;
	}
	for (Description &description : descriptions)
		if (description.references == NO_REFERENCES) description = {};

	if (hasFiles && __file_inherited != nullptr) __file_inherited();
}

}
