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
constexpr uintptr_t SUCCESS = 0;
constexpr int STDOUT_FILE = 1;

inline bool ipc_send(uint32_t pid, const ipc::Message &message) {
	return syscall(SYS_IPC_SEND, pid, reinterpret_cast<uintptr_t>(&message), 0,
				   0, 0, 0) == SUCCESS;
}

inline bool ipc_recv(ipc::Message &message) {
	return syscall(SYS_IPC_RECV, reinterpret_cast<uintptr_t>(&message), 0, 0, 0,
				   0, 0) == SUCCESS;
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

inline long write_text(const char *text, size_t length) {
	return static_cast<long>(syscall(SYS_WRITE, STDOUT_FILE,
									 reinterpret_cast<uintptr_t>(text), length,
									 0, 0, 0));
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

} // namespace cock::user

#endif // !COCK_USER_API_HPP
