#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

#define FOR_ETERNAL for (;;)

namespace cock::arch::x86 {

using cock::core::syscall::SyscallManager;
using cock::core::task::Scheduler;
using cock::utils::Logger;
using core::syscall::SystemCall;

static constexpr uint32_t SYSCALL_CODE = 0x80;

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

extern "C" uintptr_t isr_handler(uintptr_t current_esp) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(current_esp);

	if (regs->int_no == SYSCALL_CODE) {
		uint32_t syscall_num = regs->eax;
		uint32_t arg0 = regs->ebx;
		uint32_t arg1 = regs->ecx;
		uint32_t arg2 = regs->edx;
		uint32_t arg3 = regs->esi;
		uint32_t arg4 = regs->edi;
		uint32_t arg5 = regs->ebp;

		SystemCall handler =
			SyscallManager::SyscallManager::getSyscall(syscall_num);

		if (handler == nullptr) {
			regs->eax = (uint32_t)-1;
			return current_esp;
		}

		uintptr_t result = handler(arg0, arg1, arg2, arg3, arg4, arg5);

		if (syscall_num == SyscallNum::SYS_EXIT ||
			syscall_num == SyscallNum::SYS_SCHED_YIELD) {
			return Scheduler::schedule(current_esp);
		}

		regs->eax = static_cast<uint32_t>(result);
		return current_esp;
	}
	if (regs->int_no < ISR_NUM) {
		Logger::error("Exception{ .idx: %d, .type: %s}", regs->int_no,
					  EXCEPTION_MESSAGES[regs->int_no]);
		uint32_t faulting_address = utils::read_cr2();
		Logger::error("  -> Faulting Address (CR2): 0x%x", faulting_address);
		Logger::error("  -> Error Code: 0x%x", regs->err_code);

		FOR_ETERNAL;
	}
	return current_esp;
}

} // namespace cock::arch::x86
