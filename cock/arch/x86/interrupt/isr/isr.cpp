#include "cock/utils/logger.hpp"
#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <stdio.h>

#define FOR_ETERNAL for (;;)

namespace cock::arch::x86 {

using cock::utils::Logger;

constexpr const char *EXCEPTION_MESSAGES[] = {"Division by Zero",
											  "Debug",
											  "Non Maskable Interrupt",
											  "Breakpoint",
											  "Into Detected Overflow",
											  "Out of Bounds",
											  "Invalid Opcode",
											  "No Coprocessor",
											  "Double Fault",
											  "Bad TSS",
											  "Segment Not Present",
											  "Stack Fault",
											  "General Protection Fault",
											  "Page Fault",
											  "Unknow Interrupt",
											  "Coprocessor Fault",
											  "Alignment Fault",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved",
											  "Reserved"};

extern "C" void isr_handler(InterruptRegisters *regs) {
	if (regs->int_no < ISR_NUM) {
        Logger::error(EXCEPTION_MESSAGES[regs->int_no]);

		FOR_ETERNAL;
	}
}

} // namespace cock::arch::x86
