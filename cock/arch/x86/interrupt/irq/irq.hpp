#ifndef ARCH_X86_IRQ_HPP
#define ARCH_X86_IRQ_HPP

#include <cock/arch/x86/utils/helpers.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

typedef void (*IrqRoutine)(InterruptRegisters *reg);

constexpr size_t IRQ_NUM = 16;

extern "C" {
void irq_install_handler(int irq, IrqRoutine handler);
void irq_uninstall_handler(int irq);
uintptr_t irq_handler(uintptr_t current_esp);
}

} // namespace cock::arch::x86

#endif // !IRQ_HPP
