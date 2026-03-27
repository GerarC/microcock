#include <cock/core/task/scheduler.hpp>
#include <cock/arch/x86/interrupt/irq/irq.hpp>
#include <cock/arch/x86/interrupt/pic/pic.hpp>
#include <cock/utils/panic.hpp>
namespace cock::arch::x86 {

using utils::InterruptRegisters;

constexpr int BASE_ROUTINE_INDEX = 0x20;
constexpr uint32_t TIMER_NO = 0x20;
constexpr int MAX_ROUTINE_INDEX = BASE_ROUTINE_INDEX + IRQ_NUM;
constexpr const char *INVALID_INTERRUPT_MESSAGE =
	"Invalid Interrupt Number %u.";

volatile uint64_t irq_counter = 0;

using cock::utils::panic;
using utils::out_port_b;

IrqRoutine irq_routines[IRQ_NUM] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void validate_irq(int irq) {
	if (irq < BASE_ROUTINE_INDEX || irq > MAX_ROUTINE_INDEX)
		panic(INVALID_INTERRUPT_MESSAGE, irq);
}

void irq_install_handler(int irq, IrqRoutine handler) {
	irq_routines[irq - BASE_ROUTINE_INDEX] = handler;
}

void irq_uninstall_handler(int irq) {
	irq_routines[irq - BASE_ROUTINE_INDEX] = 0;
}

extern "C" uintptr_t irq_handler(uintptr_t current_esp) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(current_esp);

	size_t handler_index = regs->int_no - BASE_ROUTINE_INDEX;

	if (handler_index < IRQ_NUM) {
		IrqRoutine handler = irq_routines[handler_index];

		if (handler) handler(regs);
		if (regs->int_no >= 40) out_port_b((uint16_t)PIC2::COMMAND, 0x20);
		out_port_b((uint16_t)PIC1::COMMAND, 0x20);
	}

	if (regs->int_no == TIMER_NO)
		return cock::core::task::Scheduler::schedule(current_esp);
	return current_esp;
}

} // namespace cock::arch::x86
