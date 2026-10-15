#include <cock/arch/x86/interrupt/isr/isr.hpp>
#include <cock/core/events.hpp>
#include <hal/cpu.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using core::events::ExceptionInfo;
using core::events::SyscallOutcome;
using core::events::SyscallRequest;

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
	SyscallRequest request = {
		regs->eax, {regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi,
					regs->ebp}};

	SyscallOutcome outcome = core::events::on_syscall(request, current_esp);
	if (outcome.deliver_value)
		regs->eax = static_cast<uint32_t>(outcome.value);
	return outcome.stack_pointer;
}

static uintptr_t handle_exception(InterruptRegisters *regs,
								  uintptr_t current_esp) {
	ExceptionInfo info = {regs->int_no,
						  regs->err_code,
						  regs->cr2,
						  regs->int_no == PAGE_FAULT,
						  regs->eip,
						  (regs->cs & RING_MASK) == USER_RING,
						  EXCEPTION_MESSAGES[regs->int_no]};
	return core::events::on_exception(info, current_esp);
}

extern "C" uintptr_t isr_handler(uintptr_t current_esp) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(current_esp);

	if (regs->int_no == core::hal::SYSCALL_VECTOR)
		return handle_syscall(regs, current_esp);
	if (regs->int_no < ISR_NUM) return handle_exception(regs, current_esp);
	return current_esp;
}

} // namespace cock::arch::x86
