#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/core/hal/tasking.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::hal {

using arch::x86::GDT;
using arch::x86::utils::InterruptRegisters;
using arch::x86::SyscallNum;

constexpr uint32_t DUMMY_ADDRESS = 0x00000000;
constexpr uint32_t KERNEL_DATA = 0x10;
constexpr uint32_t KERNEL_CODE = 0x08;
constexpr uint32_t USER_DATA = 0x23;
constexpr uint32_t USER_CODE = 0x1B;
constexpr uint32_t RESERVED_AND_INTERRUPT_ENABLED = 0x202;
constexpr uint32_t BLANK = 0x0;

uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg) {
	uintptr_t stack_top = reinterpret_cast<uintptr_t>(stack_base) + stack_size;

	uintptr_t context_ptr =
		reinterpret_cast<uintptr_t>(stack_top) - sizeof(InterruptRegisters);
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);

	// Simmulate an IRQ
	regs->ds = KERNEL_DATA;
	regs->eip = reinterpret_cast<uint32_t>(wrapper_function);
	regs->cs = KERNEL_CODE;
	regs->eflags = RESERVED_AND_INTERRUPT_ENABLED;

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

	regs->ds = USER_DATA;
	regs->ss = USER_DATA;
	regs->cs = USER_CODE;
	regs->eip = reinterpret_cast<uint32_t>(entry_point);
	regs->eflags = RESERVED_AND_INTERRUPT_ENABLED;

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

void yield() { 
    __asm__ volatile("int $128" : : "a"(SyscallNum::SYS_SCHED_YIELD) : "memory"); 
}
void set_kernel_stack(uintptr_t stack_top) { GDT::setKernelStack(stack_top); }

void update_thread_entry(uintptr_t context_ptr, void *new_entry) {
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);
	regs->eip = reinterpret_cast<uint32_t>(new_entry);
}

void switch_address_space(uintptr_t pd_phys) {
	__asm__ volatile("mov %0, %%cr3" ::"r"(pd_phys) : "memory");
}

uintptr_t get_current_address_space() {
	uintptr_t cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}

void set_iopm(const uint8_t* iopm) {
    arch::x86::GDT::setIOPM(iopm);
}

} // namespace cock::core::hal
