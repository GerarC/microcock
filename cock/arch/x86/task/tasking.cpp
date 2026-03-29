#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/core/hal/tasking.hpp>
#include <stdint.h>

namespace cock::core::hal {

using arch::x86::utils::InterruptRegisters;

constexpr uint32_t DUMMY_ADDRESS = 0x00000000;
constexpr uint32_t KERNEL_DATA = 0x10;
constexpr uint32_t KERNEL_CODE = 0x08;
constexpr uint32_t RESERVED_AND_INTERRUPT_ENABLED = 0x202;
constexpr uint32_t BLANK = 0x0;

uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg) {
	uintptr_t stack_top = reinterpret_cast<uintptr_t>(stack_base) + stack_size;
	uint32_t *stack = reinterpret_cast<uint32_t *>(stack_top);

	uintptr_t context_ptr =
		reinterpret_cast<uintptr_t>(stack_top) - sizeof(InterruptRegisters);
	InterruptRegisters *regs =
		reinterpret_cast<InterruptRegisters *>(context_ptr);

	// Simmulate an IRG
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

void yield() { __asm__ volatile("int $128"); }

} // namespace cock::core::hal
