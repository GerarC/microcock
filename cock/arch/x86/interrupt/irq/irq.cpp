#include <cock/arch/x86/interrupt/irq/irq.hpp>
#include <cock/arch/x86/interrupt/pic/pic.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/logger.hpp>
namespace cock::arch::x86 {

using utils::InterruptRegisters;

constexpr int BASE_ROUTINE_INDEX = 0x20;
constexpr uint32_t TIMER_NO = 0x20;
constexpr uint32_t SPURIOUS_MASTER_IRQ = 7;
constexpr uint32_t SPURIOUS_SLAVE_IRQ = 15;
constexpr uint8_t SPURIOUS_BIT = 0x80;

using cock::utils::Logger;

IrqRoutine irq_routines[IRQ_NUM] = {};

static bool validate_irq(int irq) {
	return irq >= BASE_ROUTINE_INDEX &&
		   irq < BASE_ROUTINE_INDEX + static_cast<int>(IRQ_NUM);
}

void irq_install_handler(int irq, IrqRoutine handler) {
	if (!validate_irq(irq)) {
		Logger::error("Invalid Interrupt Number %d.", irq);
		return;
	}
	irq_routines[irq - BASE_ROUTINE_INDEX] = handler;
	PIC_unmask(static_cast<uint8_t>(irq - BASE_ROUTINE_INDEX));
}

void irq_uninstall_handler(int irq) {
	if (!validate_irq(irq)) {
		Logger::error("Invalid Interrupt Number %d.", irq);
		return;
	}
	PIC_mask(static_cast<uint8_t>(irq - BASE_ROUTINE_INDEX));
	irq_routines[irq - BASE_ROUTINE_INDEX] = nullptr;
}

static bool is_spurious(uint32_t irq) {
	if (irq == SPURIOUS_MASTER_IRQ)
		return !(PIC_read_isr(false) & SPURIOUS_BIT);

	if (irq == SPURIOUS_SLAVE_IRQ) {
		if (PIC_read_isr(true) & SPURIOUS_BIT) return false;
		PIC_end_of_interrupt(static_cast<uint8_t>(CASCADE_IRQ));
		return true;
	}
	return false;
}

extern "C" uintptr_t irq_handler(uintptr_t current_esp) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(current_esp);

	uint32_t handler_index = regs->int_no - BASE_ROUTINE_INDEX;
	if (handler_index >= IRQ_NUM) return current_esp;
	if (is_spurious(handler_index)) return current_esp;

	IrqRoutine handler = irq_routines[handler_index];
	if (handler) handler(regs);
	PIC_end_of_interrupt(static_cast<uint8_t>(handler_index));

	if (regs->int_no == TIMER_NO)
		return cock::core::task::Scheduler::onTick(current_esp);
	return current_esp;
}

} // namespace cock::arch::x86
