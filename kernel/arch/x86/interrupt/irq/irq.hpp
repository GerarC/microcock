#ifndef ISR_HPP
#define ISR_HPP

#include "../../utils/functs.hpp"

using arch::x86::utils::InterruptRegisters;

extern "C" __attribute__((interrupt)) void
irq_handler(InterruptRegisters *regs);


#endif // !ISR_HPP
#define ISR_HPP
