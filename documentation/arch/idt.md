# Interrupt Descriptor Table (IDT)

The Interrupt Descriptor Table binds CPU exceptions, hardware interrupts, and system call vectors to their handler functions.

## IDT Initialization
The PIC is remapped first (`interrupts_init`), with every line masked, so hardware interrupts land on vectors `0x20` to `0x2F` and do not collide with CPU exceptions. `IDT::init()` then fills the table of 256 entries and loads it. It does **not** enable interrupts: that happens when the first thread starts.

```cpp
void IDT::init() {
	size_t entries_size = sizeof(Entry) * MAX_ENTRIES;
	descriptor.limit = entries_size - 1;
	descriptor.base = reinterpret_cast<uintptr_t>(&entries);
	memset(&entries, 0, entries_size);

	for (size_t isr = 0; isr < ISR_NUM; isr++)
		setGate(isr, reinterpret_cast<uint32_t>(isr_stub_table[isr]),
				KERNEL_CODE_SELECTOR, KERNEL_INTERRUPT_GATE);

	for (size_t irq = 0; irq < IRQ_NUM; irq++)
		setGate(ISR_NUM + irq, reinterpret_cast<uint32_t>(irq_stub_table[irq]),
				KERNEL_CODE_SELECTOR, KERNEL_INTERRUPT_GATE);
	setGate(SYSCALL_VECTOR, reinterpret_cast<uint32_t>(&isr_stub_128),
			KERNEL_CODE_SELECTOR, USER_INTERRUPT_GATE);

	idt_flush(reinterpret_cast<uintptr_t>(&descriptor));
}
```

The gates of vector 2 (NMI) and vector 8 (double fault) are task gates (`0x85`) whose selectors point to the fault TSSs of the GDT, so those two run on their own stack; see [gdt](gdt.md#fault-tasks). Every other gate is an interrupt gate, so the CPU clears the interrupt flag on entry and handlers never nest. Only the system call gate (`0x80`) can be raised from Ring 3; an `int` to any other vector from a program is a general protection fault.

## The stubs
`isr.s` and `irq.s` hold one small stub per vector. All of them go through a common path that:

* saves the general registers and the data segment,
* clears the direction flag, as the C++ code expects,
* loads the kernel data segments,
* reads `CR2` only for a page fault,
* aligns the stack to 16 bytes before the call,
* passes the saved frame to the handler, and uses the stack pointer the handler returns, which is how a context switch happens.

## System Calls and Exceptions (ISRs)
The `isr_handler` translates the frame and hands the event to the core.

```cpp
extern "C" uintptr_t isr_handler(uintptr_t current_esp) {
	InterruptRegisters *regs = reinterpret_cast<InterruptRegisters *>(current_esp);

	if (regs->int_no == core::hal::SYSCALL_VECTOR)
		return handle_syscall(regs, current_esp);
	if (regs->int_no < ISR_NUM) return handle_exception(regs, current_esp);
	return current_esp;
}
```

* A system call becomes a `SyscallRequest`; the core runs the handler and answers with the value for `EAX` and the stack pointer to resume with.
* An exception becomes an `ExceptionInfo` (vector, error code, faulting address, instruction pointer, whether it came from Ring 3). The core terminates a thread that caused it from Ring 3 and panics when it came from Ring 0.

## Hardware Interrupts (IRQs)
Hardware interrupts are handled by `irq_handler` in `cock/arch/x86/interrupt/irq/irq.cpp`.

```cpp
extern "C" uintptr_t irq_handler(uintptr_t current_esp) {
	InterruptRegisters *regs = reinterpret_cast<InterruptRegisters *>(current_esp);

	uint32_t line = regs->int_no - BASE_ROUTINE_INDEX;
	if (line >= IRQ_NUM) return current_esp;
	if (is_spurious(line)) return current_esp;

	IrqRoutine handler = irq_routines[line];
	if (handler) handler(regs);
	PIC_end_of_interrupt(static_cast<uint8_t>(line));

	return core::events::on_irq(line, current_esp);
}
```

The spurious interrupts of lines 7 and 15 are detected and ignored. The end of interrupt is sent before the core may switch context. The core then decides: the timer line runs the scheduler, and any other line is masked and turned into a pending notification of the thread that bound it (see [ipc](../core/ipc.md#interrupts-as-messages)). The architecture never calls the scheduler itself.
