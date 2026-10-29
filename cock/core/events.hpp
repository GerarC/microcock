/** @file
 * @brief Architecture independent entry points the HAL calls when a syscall,
 * a CPU exception or a hardware interrupt reaches the kernel core.
 */
#ifndef CORE_EVENTS_HPP
#define CORE_EVENTS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::events {

/** @brief Number of argument registers a system call carries. */
constexpr size_t SYSCALL_ARGUMENT_COUNT = 6;

/**
 * @brief A system call as captured by the HAL from the user registers.
 */
struct SyscallRequest {
	uintptr_t number; /**< System call number chosen by the caller. Untrusted. */
	uintptr_t args[SYSCALL_ARGUMENT_COUNT]; /**< Raw argument registers.
											   Untrusted, handlers validate. */
};

/**
 * @brief What the HAL must do when a system call handler has finished.
 */
struct SyscallOutcome {
	uintptr_t value; /**< Value to return to the caller in the result register. */
	uintptr_t stack_pointer; /**< Saved context to resume. It differs from the
								one passed in when the scheduler switched
								thread. */
	bool deliver_value; /**< False when the call is restarted (the instruction
						   is rewound) and the result register must stay
						   untouched. */
};

/**
 * @brief Description of a CPU exception, filled in by the HAL.
 */
struct ExceptionInfo {
	uint32_t vector; /**< Exception vector number. */
	uint32_t error_code; /**< Error code pushed by the CPU, 0 when none. */
	uintptr_t fault_address; /**< Faulting address (CR2 on x86), valid only when
								has_fault_address is true. */
	bool has_fault_address; /**< True for page faults and similar. */
	uintptr_t instruction; /**< Instruction pointer at the time of the fault. */
	bool from_user; /**< True when the fault happened in Ring 3. */
	bool write_to_present_page; /**< True for a page fault caused by a write to a
								   page that is mapped, which is what a copy on
								   write page raises. */
	const char *name; /**< Human readable name of the exception. */
};

/**
 * @brief Dispatches a system call to its handler.
 *
 * Records the saved context as the current syscall frame, looks the handler up
 * and runs it with interrupts in the state the HAL left them. A restart
 * request rewinds the call, a reschedule request switches thread.
 *
 * @param request Number and arguments of the call, never trusted.
 * @param stack_pointer Saved context of the calling thread.
 * @return The outcome. An unknown number returns all ones with the same stack
 * pointer and the value delivered.
 * @note Called from the syscall gate, in the context of the caller.
 */
SyscallOutcome on_syscall(const SyscallRequest &request,
						  uintptr_t stack_pointer);

/**
 * @brief Handles a CPU exception.
 *
 * A write from Ring 3 to a page that is mapped read only because it is shared
 * copy on write is not an error: the page gets a private frame and the
 * instruction runs again, with nothing logged. Otherwise the exception is
 * logged. A fault in Ring 0, or without a current thread, panics.
 * A fault in Ring 3 kills the faulting thread, sets the process exit status to
 * the fault status and schedules another thread.
 *
 * @param info Description of the exception.
 * @param stack_pointer Saved context of the interrupted code.
 * @return Saved context to resume. It never returns to a killed thread.
 * @note Called by the exception gates with interrupts disabled.
 */
uintptr_t on_exception(const ExceptionInfo &info, uintptr_t stack_pointer);

/**
 * @brief Handles a hardware interrupt.
 *
 * The timer line drives the scheduler tick. Any other line is forwarded as a
 * notification to the thread that bound it, which makes the scheduler yield.
 * Lines nobody owns are ignored.
 *
 * @param line Interrupt line, already translated by the HAL.
 * @param stack_pointer Saved context of the interrupted code.
 * @return Saved context to resume, possibly of another thread.
 * @note Called from the interrupt gates with interrupts disabled.
 */
uintptr_t on_irq(uint32_t line, uintptr_t stack_pointer);

} // namespace cock::core::events

#endif // !CORE_EVENTS_HPP
