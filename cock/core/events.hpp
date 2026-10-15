#ifndef CORE_EVENTS_HPP
#define CORE_EVENTS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::events {

constexpr size_t SYSCALL_ARGUMENT_COUNT = 6;

struct SyscallRequest {
	uintptr_t number;
	uintptr_t args[SYSCALL_ARGUMENT_COUNT];
};

struct SyscallOutcome {
	uintptr_t value;
	uintptr_t stack_pointer;
	bool deliver_value;
};

struct ExceptionInfo {
	uint32_t vector;
	uint32_t error_code;
	uintptr_t fault_address;
	bool has_fault_address;
	uintptr_t instruction;
	bool from_user;
	const char *name;
};

/**
 * @brief Entry points the architecture calls for every event that may change
 * what runs next. The architecture only reports the event; the kernel decides.
 * Each returns the stack pointer of the context to resume.
 */
SyscallOutcome on_syscall(const SyscallRequest &request,
						  uintptr_t stack_pointer);
uintptr_t on_exception(const ExceptionInfo &info, uintptr_t stack_pointer);
uintptr_t on_irq(uint32_t line, uintptr_t stack_pointer);

} // namespace cock::core::events

#endif // !CORE_EVENTS_HPP
