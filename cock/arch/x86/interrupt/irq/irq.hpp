#ifndef ARCH_X86_IRQ_HPP
#define ARCH_X86_IRQ_HPP

/**
 * @file
 * @brief Hardware interrupt (IRQ) dispatch for the 16 PIC lines.
 *
 * The assembly stubs push an InterruptRegisters frame and call irq_handler()
 * with the stack pointer. The handler dispatches to the registered routine,
 * acknowledges the PIC and then lets the core decide whether to switch threads.
 */

#include <cock/arch/x86/utils/helpers.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

/**
 * @brief Defines the function signature for a hardware interrupt handler.
 *
 * Runs in Ring 0 inside the interrupt with interrupts disabled; it must be
 * short and must not block. The PIC end of interrupt is sent by the dispatcher
 * after the routine returns, not by the routine.
 */
typedef void (*IrqRoutine)(InterruptRegisters *reg);

/** @brief Standard number of hardware interrupts provided by the dual PIC
 * setup. */
constexpr size_t IRQ_NUM = 16;

extern "C" {
/**
 * @brief Registers a custom handler function for a specific IRQ.
 *
 * Also unmasks the line in the PIC. Out of range values are logged and
 * ignored. Note that `irq` is the interrupt vector (0x20 to 0x2F, the PIC
 * remapped base plus the line), not the bare line number.
 *
 * @param irq The IRQ index (0-15) to hook into, given as vector 0x20 + line.
 * @param handler Function pointer to the routine to execute.
 */
void irq_install_handler(int irq, IrqRoutine handler);

/**
 * @brief Removes a previously registered IRQ handler.
 *
 * Masks the line in the PIC and clears the routine. Out of range values are
 * logged and ignored. `irq` uses the same vector numbering as
 * irq_install_handler().
 *
 * @param irq The IRQ index (0-15) to clear, given as vector 0x20 + line.
 */
void irq_uninstall_handler(int irq);

/**
 * @brief Main C-level entry point for all hardware interrupts, called from
 * assembly stubs.
 *
 * Filters spurious interrupts on lines 7 and 15 (checking the PIC in-service
 * register), calls the installed routine, sends the end of interrupt to the PIC
 * and finally calls the core event hook, which may pick another thread. Runs in
 * Ring 0 with interrupts disabled.
 *
 * @param current_esp The current stack pointer before servicing the interrupt.
 * @return The updated stack pointer (may point to a different thread if a
 * context switch occurred).
 */
uintptr_t irq_handler(uintptr_t current_esp);

}

} // namespace cock::arch::x86

#endif // !IRQ_HPP
