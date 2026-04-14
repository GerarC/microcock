#ifndef ARCH_X86_ISR_HPP
#define ARCH_X86_ISR_HPP

#include <cock/arch/x86/utils/helpers.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

/** @brief Standard number of CPU-reserved exception vectors. */
constexpr int ISR_NUM = 32;

/**
 * @brief Main C-level entry point for all CPU exceptions and software interrupts (Syscalls).
 * * Dispatches syscalls to the SyscallManager or triggers kernel panics for unhandled CPU faults.
 * @param current_esp The current stack pointer holding the saved context.
 * @return The updated stack pointer to resume execution from.
 */
extern "C" uintptr_t isr_handler(uintptr_t current_esp);

} // namespace cock::arch::x86

#endif // !ISR_HPP
