#ifndef COCK_USER_API_HPP
#define COCK_USER_API_HPP

#include <cock/abi/capability.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>
#include <sys/syscalls.h>

namespace cock::user {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uint32_t NO_PROCESS = 0xFFFFFFFF;
constexpr uint32_t NO_SHM = 0xFFFFFFFF;
constexpr uint32_t NO_MODULE = 0xFFFFFFFF;
constexpr uintptr_t SUCCESS = 0;
constexpr uintptr_t NO_MESSAGE = 1;
constexpr int STDOUT_FILE = 1;

inline uint32_t self() {
	return static_cast<uint32_t>(syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0));
}

inline bool ipc_send(uint32_t pid, const ipc::Message &message) {
	return syscall(SYS_IPC_SEND, pid, reinterpret_cast<uintptr_t>(&message), 0,
				   0, 0, 0) == SUCCESS;
}

inline bool ipc_recv(ipc::Message &message) {
	return syscall(SYS_IPC_RECV, reinterpret_cast<uintptr_t>(&message), 0, 0, 0,
				   0, 0) == SUCCESS;
}

enum class Received : uint32_t {
	MESSAGE,
	EMPTY,
	FAILED,
};

inline Received ipc_try_recv(ipc::Message &message) {
	uintptr_t result = syscall(SYS_IPC_TRY_RECV, reinterpret_cast<uintptr_t>(&message),
							   0, 0, 0, 0, 0);
	if (result == SUCCESS) return Received::MESSAGE;
	return result == NO_MESSAGE ? Received::EMPTY : Received::FAILED;
}

inline bool request_ports(uint32_t first, uint32_t count) {
	return syscall(SYS_REQUEST_RESOURCE, first, count, 0, 0, 0, 0) == SUCCESS;
}

inline bool irq_bind(uint32_t line) {
	return syscall(SYS_IRQ_BIND, line, 0, 0, 0, 0, 0) == SUCCESS;
}

inline bool irq_ack(uint32_t line) {
	return syscall(SYS_IRQ_ACK, line, 0, 0, 0, 0, 0) == SUCCESS;
}

inline bool cap_grant(uint32_t pid, abi::CapabilityType type, uintptr_t first,
					  uintptr_t count) {
	return syscall(SYS_CAP_GRANT, pid, static_cast<uintptr_t>(type), first,
				   count, 0, 0) == SUCCESS;
}

inline uint32_t lookup(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_PROC_LOOKUP, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

inline uint32_t shm_create(size_t pages) {
	uintptr_t result = syscall(SYS_SHM_CREATE, pages, 0, 0, 0, 0, 0);
	return result == FAILURE ? NO_SHM : static_cast<uint32_t>(result);
}

inline bool shm_share(uint32_t id, uint32_t pid, bool writable) {
	return syscall(SYS_SHM_SHARE, id, pid, writable ? 1 : 0, 0, 0, 0) == SUCCESS;
}

inline void *shm_map(uint32_t id, bool writable) {
	uintptr_t result = syscall(SYS_SHM_MAP, id, writable ? 1 : 0, 0, 0, 0, 0);
	return result == FAILURE ? nullptr : reinterpret_cast<void *>(result);
}

inline bool shm_release(uint32_t id) {
	return syscall(SYS_SHM_RELEASE, id, 0, 0, 0, 0, 0) == SUCCESS;
}

inline uint32_t module_open(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_MODULE_OPEN, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_SHM : static_cast<uint32_t>(result);
}

inline uint32_t module_index(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_MODULE_INDEX, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_MODULE : static_cast<uint32_t>(result);
}

inline uint32_t spawn(const void *image, size_t size, const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result =
		syscall(SYS_SPAWN, reinterpret_cast<uintptr_t>(image), size,
				reinterpret_cast<uintptr_t>(name), length, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

inline bool process_alive(uint32_t pid) {
	return syscall(SYS_PROC_ALIVE, pid, 0, 0, 0, 0, 0) == SUCCESS;
}

inline long write_text(const char *text, size_t length) {
	size_t written = 0;
	while (written < length) {
		long step = static_cast<long>(
			syscall(SYS_WRITE, STDOUT_FILE,
					reinterpret_cast<uintptr_t>(text + written), length - written,
					0, 0, 0));
		if (step <= 0) return written == 0 ? step : static_cast<long>(written);
		written += static_cast<size_t>(step);
	}
	return static_cast<long>(written);
}

[[noreturn]] inline void exit_process(uintptr_t status) {
	syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
	for (;;) {
	}
}

inline uint8_t port_in8(uint16_t port) {
	uint8_t value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

inline void port_out8(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline void port_read_words(uint16_t port, void *buffer, size_t words) {
	__asm__ volatile("rep insw" : "+D"(buffer), "+c"(words) : "d"(port) : "memory");
}

} // namespace cock::user

#endif // !COCK_USER_API_HPP
