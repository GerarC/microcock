#ifndef ARCH_X86_HAL_INTERRUPT_HPP
#define ARCH_X86_HAL_INTERRUPT_HPP

#include <hal/cpu.hpp>
#include <stdint.h>

namespace cock::core::hal {

constexpr uint32_t IRQ_LINE_COUNT = 16;
constexpr uint32_t CASCADE_IRQ_LINE = 2;

constexpr bool irq_line_reserved(uint32_t line) {
	return line >= IRQ_LINE_COUNT || line == TIMER_IRQ_LINE ||
		   line == CASCADE_IRQ_LINE;
}

void irq_mask(uint32_t line);
void irq_unmask(uint32_t line);

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_INTERRUPT_HPP
