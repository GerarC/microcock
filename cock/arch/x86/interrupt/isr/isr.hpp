#ifndef ARCH_X86_ISR_HPP
#define ARCH_X86_ISR_HPP

#include <cock/arch/x86/utils/helpers.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

constexpr int ISR_NUM = 32;

extern "C" uintptr_t isr_handler(uintptr_t current_esp);

} // namespace cock::arch::x86

#endif // !ISR_HPP
