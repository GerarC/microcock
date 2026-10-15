#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using cock::core::syscall::SyscallManager;
using cock::core::task::Scheduler;
using cock::utils::Logger;
using cock::utils::panic;
using core::syscall::SystemCall;

static constexpr uint32_t SYSCALL_CODE = 0x80;
static constexpr uint32_t PAGE_FAULT = 14;
static constexpr uint32_t RING_MASK = 0x3;
static constexpr uint32_t USER_RING = 0x3;

constexpr const char *EXCEPTION_MESSAGES[ISR_NUM] = {
	"Division by Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved",
	"x87 Floating-Point Exception",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point Exception",
	"Virtualization Exception",
	"Control Protection Exception",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Hypervisor Injection Exception",
	"VMM Communication Exception",
	"Security Exception",
	"Reserved"};

static uintptr_t handle_syscall(InterruptRegisters *regs,
								uintptr_t current_esp) {
	uint32_t syscall_num = regs->eax;
	uint32_t arg0 = regs->ebx;
	uint32_t arg1 = regs->ecx;
	uint32_t arg2 = regs->edx;
	uint32_t arg3 = regs->esi;
	uint32_t arg4 = regs->edi;
	uint32_t arg5 = regs->ebp;

	SystemCall handler = SyscallManager::getSyscall(syscall_num);

	if (handler == nullptr) {
		regs->eax = (uint32_t)-1;
		return current_esp;
	}

	uintptr_t result = handler(arg0, arg1, arg2, arg3, arg4, arg5);

	if (syscall_num == SyscallNum::SYS_EXIT ||
		syscall_num == SyscallNum::SYS_SCHED_YIELD) {
		return Scheduler::onYield(current_esp);
	}

	regs->eax = static_cast<uint32_t>(result);
	return current_esp;
}

static uintptr_t handle_exception(InterruptRegisters *regs,
								  uintptr_t current_esp) {
	bool from_user = (regs->cs & RING_MASK) == USER_RING;

	Logger::error("Exception{ .idx: %u, .type: %s, .ring: %u}", regs->int_no,
				  EXCEPTION_MESSAGES[regs->int_no], regs->cs & RING_MASK);
	Logger::error("  -> EIP: 0x%x  Error Code: 0x%x", regs->eip,
				  regs->err_code);
	if (regs->int_no == PAGE_FAULT)
		Logger::error("  -> Faulting Address (CR2): 0x%x", regs->cr2);

	if (!from_user || !Scheduler::getCurrentThread())
		panic("Unrecoverable exception in kernel mode");

	Logger::error("Thread %d killed by exception",
				  Scheduler::getCurrentThread()->getId());
	Scheduler::terminateCurrentThread();
	return Scheduler::onYield(current_esp);
}

extern "C" uintptr_t isr_handler(uintptr_t current_esp) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(current_esp);

	if (regs->int_no == SYSCALL_CODE) return handle_syscall(regs, current_esp);
	if (regs->int_no < ISR_NUM) return handle_exception(regs, current_esp);
	return current_esp;
}

} // namespace cock::arch::x86
