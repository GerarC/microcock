#ifndef ARCH_X86_ISR_HPP
#define ARCH_X86_ISR_HPP

/**
 * @file
 * @brief CPU exception and system call entry from the interrupt stubs.
 *
 * Vectors 0..31 are CPU exceptions; vector 0x80 carries system calls. Both
 * arrive here from the assembly stubs with a saved InterruptRegisters frame on
 * the kernel stack, and are forwarded to the architecture independent core
 * events.
 */

#include <cock/arch/x86/utils/helpers.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

/** @brief Standard number of CPU-reserved exception vectors. */
constexpr int ISR_NUM = 32;

/**
 * @brief Main C-level entry point for all CPU exceptions and software interrupts (Syscalls).
 * * Dispatches syscalls to the SyscallManager or triggers kernel panics for unhandled CPU faults.
 *
 * For vector 0x80 it packs EAX (number) and EBX, ECX, EDX, ESI, EDI, EBP
 * (arguments) into a request and writes the result back into the saved EAX.
 * For vectors below ISR_NUM it reports the exception, including CR2 for page
 * faults and whether it came from Ring 3 (CS privilege level 3), to the core.
 * Runs in Ring 0 with interrupts disabled.
 *
 * @param current_esp The current stack pointer holding the saved context.
 * @return The updated stack pointer to resume execution from.
 */
extern "C" uintptr_t isr_handler(uintptr_t current_esp);

} // namespace cock::arch::x86

#endif // !ISR_HPP
