#include <cock/core/task/scheduler.hpp>
#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

#define FOR_ETERNAL for (;;)


namespace cock::arch::x86 {

using cock::utils::Logger;
using cock::core::task::Scheduler;

static constexpr uint32_t YIELD_INT = 0x80;


constexpr const char *EXCEPTION_MESSAGES[] = {"Division by Zero",
											  "Debug",
											  "Non Maskable Interrupt",
											  "Breakpoint",
											  "Into Detected Overflow",
											  "Out of Bounds",
											  "Invalid Opcode",
											  "No Coprocessor",
											  "Double Fault",
											  "Coprocessor Segment Overrun",
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

extern "C" uintptr_t isr_handler(uintptr_t current_esp){
    InterruptRegisters *regs = reinterpret_cast<InterruptRegisters *>(current_esp);

    if (regs->int_no == YIELD_INT) {
        return Scheduler::schedule(current_esp);
    }

	if (regs->int_no < ISR_NUM) {
		Logger::error("Exception{ .idx: %d, .type: %s}", regs->int_no,
					  EXCEPTION_MESSAGES[regs->int_no]);

		FOR_ETERNAL;
	}
    return current_esp;
}

} // namespace cock::arch::x86
