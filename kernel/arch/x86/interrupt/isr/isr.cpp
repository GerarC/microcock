#include "isr.hpp"
#include <stdio.h>

#define ETERNAL                                                                \
	;                                                                          \
	;

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

extern "C" __attribute__((interrupt)) void
isr_handler(InterruptRegisters *regs) {
	if (regs->int_no < 32) {
		puts(EXCEPTION_MESSAGES[regs->int_no]);
		puts("System Exception Halted");
		for (ETERNAL)
			;
	}
}
