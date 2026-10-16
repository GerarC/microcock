#ifndef ARCH_X86_HAL_INTERRUPT_HPP
#define ARCH_X86_HAL_INTERRUPT_HPP

/**
 * @file
 * @brief HAL: hardware interrupt lines that Ring 3 drivers may bind.
 *
 * Port requirement: define IRQ_LINE_COUNT, CASCADE_IRQ_LINE (or an
 * equivalent reserved line), irq_line_reserved() and implement irq_mask() and
 * irq_unmask() in an architecture source file. The core numbers lines from
 * zero and never sees controller specific details.
 */

#include <hal/cpu.hpp>
#include <stdint.h>

namespace cock::core::hal {

/** @brief Number of IRQ lines of the two cascaded 8259 PICs. */
constexpr uint32_t IRQ_LINE_COUNT = 16;
/** @brief Master PIC input that carries the slave PIC; never a device. */
constexpr uint32_t CASCADE_IRQ_LINE = 2;

/**
 * @brief Tells whether a line cannot be handed to a user driver.
 *
 * Lines outside the controller range, the kernel timer line and the cascade
 * line are reserved. Pure function, safe in any context.
 *
 * @param line IRQ line number as seen by the core.
 * @return True if the line is reserved or does not exist.
 */
constexpr bool irq_line_reserved(uint32_t line) {
	return line >= IRQ_LINE_COUNT || line == TIMER_IRQ_LINE ||
		   line == CASCADE_IRQ_LINE;
}

/**
 * @brief Disables delivery of one interrupt line.
 *
 * On x86 sets the line bit in the interrupt mask register of the PIC that owns
 * it (data port 0x21 or 0xA1). Ring 0 only; `line` must be below
 * IRQ_LINE_COUNT.
 *
 * @param line IRQ line number to mask.
 */
void irq_mask(uint32_t line);

/**
 * @brief Enables delivery of one interrupt line.
 *
 * On x86 clears the line bit in the mask register of its PIC and, for lines of
 * the slave PIC, also unmasks the cascade input so the line can reach the CPU.
 * Ring 0 only; `line` must be below IRQ_LINE_COUNT.
 *
 * @param line IRQ line number to unmask.
 */
void irq_unmask(uint32_t line);

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_INTERRUPT_HPP
