#ifndef IRQ_HPP
#define IRQ_HPP

#include "../utils/functs.hpp"
#include <stddef.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

typedef void (*IrqRoutine)(InterruptRegisters *reg);

constexpr size_t IRQ_NUM = 16;

extern "C" {
void irq_install_handler(int irq, IrqRoutine handler);
void irq_uninstall_handler(int irq);
void irq_handler(InterruptRegisters *regs);
}

} // namespace cock::arch::x86

#endif // !IRQ_HPP
