/**
 * @file thread.hpp
 * @brief Starting threads inside the calling process.
 *
 * The program owns the stack of each thread: the kernel never maps or frees it, so the
 * caller must keep it valid until the thread ends.
 */
#ifndef COCK_USER_THREAD_HPP
#define COCK_USER_THREAD_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user {

/** Entry point of a thread; receives the pointer given to `thread_start`. */
using ThreadFunction = void (*)(void *);

/** Exit status used when a thread function returns. */
constexpr uintptr_t THREAD_RETURN_STATUS = 0;
/** Slot, counted down from the stack top, that holds the argument of the thread function. */
constexpr size_t FRAME_ARGUMENT_SLOT = 1;
/** Slot, counted down from the stack top, that holds the return address (`thread_return`). */
constexpr size_t FRAME_RETURN_SLOT = 2;

/**
 * @brief Return target of a thread function: ends the thread (`SYS_EXIT`) and never returns.
 *
 * The kernel ends only the calling thread; the process ends with its last thread.
 */
[[noreturn]] inline void thread_return() { exit_process(THREAD_RETURN_STATUS); }

/**
 * @brief Starts a thread that runs `function(argument)` on a caller supplied stack.
 *
 * Builds the first frame (argument and return address `thread_return`) at the top of
 * the stack and calls `thread_create_raw` (`SYS_THREAD_CREATE`). Does not block.
 * @param function Thread entry point.
 * @param argument Pointer passed to `function`.
 * @param stack Lowest address of a writable stack range owned by the caller.
 * @param stack_size Size of the range in bytes; at least `MIN_THREAD_STACK_BYTES`.
 * @return The thread id, or `NO_PROCESS` when the stack is too small or the kernel
 *         refused the entry or the range.
 */
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
