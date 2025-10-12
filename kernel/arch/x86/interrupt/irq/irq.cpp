#include "irq.hpp"
#include "../pic/pic.hpp"
#include <cock/utils/panic.hpp>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

typedef void (*IrqRoutine)(InterruptRegisters *reg);

using utils::out_port_b;
using cock::utils::panic;

IrqRoutine irq_routines[16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void irq_install_handler(int irq, IrqRoutine handler) {
	irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) { irq_routines[irq] = 0; }

extern "C" __attribute__((interrupt)) void
irq_handler(InterruptRegisters *regs) {
	if (regs->int_no < 32 || regs->int_no > 48) panic("Invalid Interrupt Number.");
	IrqRoutine handler;

	int handler_index = regs->int_no - 32;

	if (handler_index < 16) {
		handler = irq_routines[handler_index];

		if (handler) handler(regs);
		if (regs->int_no >= 40) out_port_b((uint16_t)PIC2::COMMAND, 0x20);
		out_port_b((uint16_t)PIC1::COMMAND, 0x20);
	}
}

} // namespace cock::arch::x86
