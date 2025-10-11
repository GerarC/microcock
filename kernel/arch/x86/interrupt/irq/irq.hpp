#ifndef IRQ_HPP
#define IRQ_HPP

#include "../utils/functs.hpp"
#include <stddef.h>

namespace cock::arch::x86 {
using utils::InterruptRegisters;

constexpr size_t IRQ_NUM = 16;

extern "C" __attribute__((interrupt)) void
irq_handler(InterruptRegisters *regs);

} // namespace cock::arch::x86

#endif // !IRQ_HPP
