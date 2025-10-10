#include "irq.hpp"
#include "../pic/pic.hpp"

typedef void (*IrqRoutine)(InterruptRegisters *reg);

using arch::x86::PIC1;
using arch::x86::PIC2;
using arch::x86::utils::out_port_b;

IrqRoutine irq_routines[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void irq_install_handler(int irq, IrqRoutine handler) {
	irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) { irq_routines[irq] = 0; }

extern "C" __attribute__((interrupt)) void
irq_handler(InterruptRegisters *regs) {
	IrqRoutine handler;

	int handler_index = regs->int_no - 32;

	if (handler_index < 16) {
		handler = irq_routines[handler_index];

		if (handler) handler(regs);
		if (regs->int_no >= 40) out_port_b((uint16_t)PIC2::COMMAND, 0x20);
		out_port_b((uint16_t)PIC1::COMMAND, 0x20);
	}
}
