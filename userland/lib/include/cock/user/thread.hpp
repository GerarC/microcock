#ifndef COCK_USER_THREAD_HPP
#define COCK_USER_THREAD_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user {

using ThreadFunction = void (*)(void *);

constexpr uintptr_t THREAD_RETURN_STATUS = 0;
constexpr size_t FRAME_ARGUMENT_SLOT = 1;
constexpr size_t FRAME_RETURN_SLOT = 2;

[[noreturn]] inline void thread_return() { exit_process(THREAD_RETURN_STATUS); }

inline uint32_t thread_start(ThreadFunction function, void *argument, void *stack,
							 size_t stack_size) {
	if (stack_size < abi::process::MIN_THREAD_STACK_BYTES) return NO_PROCESS;

	uintptr_t *top = reinterpret_cast<uintptr_t *>(
		reinterpret_cast<uintptr_t>(stack) + stack_size);
	top[-static_cast<long>(FRAME_ARGUMENT_SLOT)] = reinterpret_cast<uintptr_t>(argument);
	top[-static_cast<long>(FRAME_RETURN_SLOT)] =
		reinterpret_cast<uintptr_t>(&thread_return);
	return thread_create_raw(reinterpret_cast<uintptr_t>(function), stack,
							 stack_size - sizeof(uintptr_t));
}

} // namespace cock::user

#endif // !COCK_USER_THREAD_HPP
