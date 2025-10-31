#ifndef ISR_HPP
#define ISR_HPP

#include <cock/arch/x86/utils/helpers.hpp>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

constexpr int ISR_NUM = 32;

extern "C" void isr_handler(InterruptRegisters *regs);

} // namespace cock::arch::x86

#endif // !ISR_HPP
