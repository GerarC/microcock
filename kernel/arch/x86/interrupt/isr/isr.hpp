#ifndef ISR_HPP
#define ISR_HPP

#include "../utils/functs.hpp"



namespace cock::arch::x86 {

using utils::InterruptRegisters;

constexpr int ISR_NUM = 32;

extern "C" __attribute__((interrupt)) void
isr_handler(InterruptRegisters *regs);

}

#endif // !ISR_HPP
