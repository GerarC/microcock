#ifndef ARCH_X86_IRQ_HPP
#define ARCH_X86_IRQ_HPP

#include <cock/arch/x86/utils/helpers.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

/**
 * @brief Defines the function signature for a hardware interrupt handler.
 */
typedef void (*IrqRoutine)(InterruptRegisters *reg);

/** @brief Standard number of hardware interrupts provided by the dual PIC
 * setup. */
constexpr size_t IRQ_NUM = 16;

extern "C" {
/**
 * @brief Registers a custom handler function for a specific IRQ.
 * @param irq The IRQ index (0-15) to hook into.
 * @param handler Function pointer to the routine to execute.
 */
void irq_install_handler(int irq, IrqRoutine handler);

/**
 * @brief Removes a previously registered IRQ handler.
 * @param irq The IRQ index (0-15) to clear.
 */
void irq_uninstall_handler(int irq);

/**
 * @brief Main C-level entry point for all hardware interrupts, called from
 * assembly stubs.
 * @param current_esp The current stack pointer before servicing the interrupt.
 * @return The updated stack pointer (may point to a different thread if a
 * context switch occurred).
 */
uintptr_t irq_handler(uintptr_t current_esp);

}

} // namespace cock::arch::x86

#endif // !IRQ_HPP
