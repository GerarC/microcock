#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/gdt/segments.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <hal/cpu.hpp>
#include <cock/core/hal/tasking.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::hal {

using arch::x86::EFLAGS_INTERRUPTS_ENABLED;
using arch::x86::GDT;
using arch::x86::KERNEL_CODE_SELECTOR;
using arch::x86::KERNEL_DATA_SELECTOR;
using arch::x86::USER_CODE_SELECTOR;
using arch::x86::USER_DATA_SELECTOR;
using arch::x86::utils::InterruptRegisters;

constexpr uint32_t DUMMY_ADDRESS = 0x00000000;
constexpr uint32_t BLANK = 0x0;

uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg) {
	uintptr_t stack_top = reinterpret_cast<uintptr_t>(stack_base) + stack_size;

	uintptr_t context_ptr =
		reinterpret_cast<uintptr_t>(stack_top) - sizeof(InterruptRegisters);
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	memset(regs, 0, sizeof(InterruptRegisters));

	// Simmulate an IRQ
	regs->ds = KERNEL_DATA_SELECTOR;
	regs->eip = reinterpret_cast<uint32_t>(wrapper_function);
	regs->cs = KERNEL_CODE_SELECTOR;
	regs->eflags = EFLAGS_INTERRUPTS_ENABLED;

	regs->useresp = DUMMY_ADDRESS;
	regs->ss = reinterpret_cast<uint32_t>(user_arg);

	regs->eax = BLANK;
	regs->ebx = BLANK;
	regs->ecx = BLANK;
	regs->edx = BLANK;
	regs->esi = BLANK;
	regs->edi = BLANK;
	regs->ebp = BLANK;

	return context_ptr;
}

uintptr_t prepare_user_thread_stack(void *kernel_stack_base,
									size_t kernel_stack_size,
									void *user_stack_base,
									size_t user_stack_size, void *entry_point,
									void *user_arg) {

	uintptr_t kernel_stack_top =
		reinterpret_cast<uintptr_t>(kernel_stack_base) + kernel_stack_size;
	uintptr_t user_stack_top =
		reinterpret_cast<uintptr_t>(user_stack_base) + user_stack_size - 4;
	uintptr_t context_ptr = kernel_stack_top - sizeof(InterruptRegisters);
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	memset(regs, 0, sizeof(InterruptRegisters));

	regs->ds = USER_DATA_SELECTOR;
	regs->ss = USER_DATA_SELECTOR;
	regs->cs = USER_CODE_SELECTOR;
	regs->eip = reinterpret_cast<uint32_t>(entry_point);
	regs->eflags = EFLAGS_INTERRUPTS_ENABLED;

	regs->useresp = user_stack_top;

	regs->eax = reinterpret_cast<uint32_t>(user_arg);
	regs->ebx = BLANK;
	regs->ecx = BLANK;
	regs->edx = BLANK;
	regs->esi = BLANK;
	regs->edi = BLANK;
	regs->ebp = BLANK;

	return context_ptr;
}

uintptr_t clone_thread_context(uintptr_t source_context, void *kernel_stack_base,
							   size_t kernel_stack_size) {
	uintptr_t kernel_stack_top =
		reinterpret_cast<uintptr_t>(kernel_stack_base) + kernel_stack_size;
	uintptr_t context_ptr = kernel_stack_top - sizeof(InterruptRegisters);
	memcpy(reinterpret_cast<void *>(context_ptr),
		   reinterpret_cast<const void *>(source_context), sizeof(InterruptRegisters));
	return context_ptr;
}

void set_kernel_stack(uintptr_t stack_top) { GDT::setKernelStack(stack_top); }

void read_syscall_frame(uintptr_t context_ptr, SyscallFrame &frame) {
	const InterruptRegisters *regs =
		reinterpret_cast<const InterruptRegisters *>(context_ptr);
	frame.result = regs->eax;
	frame.values[0] = regs->ebx;
	frame.values[1] = regs->ecx;
	frame.values[2] = regs->edx;
	frame.values[3] = regs->esi;
	frame.values[4] = regs->edi;
}

void write_syscall_frame(uintptr_t context_ptr, const SyscallFrame &frame) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	regs->eax = static_cast<uint32_t>(frame.result);
	regs->ebx = static_cast<uint32_t>(frame.values[0]);
	regs->ecx = static_cast<uint32_t>(frame.values[1]);
	regs->edx = static_cast<uint32_t>(frame.values[2]);
	regs->esi = static_cast<uint32_t>(frame.values[3]);
	regs->edi = static_cast<uint32_t>(frame.values[4]);
}

void rewind_syscall(uintptr_t context_ptr) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	regs->eip -= SYSCALL_INSTRUCTION_SIZE;
}

void update_thread_entry(uintptr_t context_ptr, void *new_entry) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	regs->eip = reinterpret_cast<uint32_t>(new_entry);
}

void set_thread_argument(uintptr_t context_ptr, uintptr_t value) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	regs->eax = static_cast<uint32_t>(value);
}

} // namespace cock::core::hal
