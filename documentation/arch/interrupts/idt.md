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

The gates of vector 2 (NMI) and vector 8 (double fault) are task gates (`0x85`) whose selectors point to the fault TSSs of the GDT, so those two run on their own stack; see [gdt](../cpu/gdt.md#fault-tasks). Every other gate is an interrupt gate, so the CPU clears the interrupt flag on entry and handlers never nest. Only the system call gate (`0x80`) can be raised from Ring 3; an `int` to any other vector from a program is a general protection fault.

## The stubs
`isr.s` and `irq.s` hold one small stub per vector. All of them go through a common path that:

* saves the general registers and the data segment,
* clears the direction flag, as the C++ code expects,
* loads the kernel data segments,
* reads `CR2` only for a page fault,
* aligns the stack to 16 bytes before the call,
* passes the saved frame to the handler, and uses the stack pointer the handler returns, which is how a context switch happens.

## What the stubs lead to
The stub of a vector calls one of two C++ entry points with the saved frame, and resumes with the stack pointer they return:

* Vectors 0 to 31 and `0x80` go to `isr_handler`: a CPU fault is described in [exceptions](exceptions.md) and a system call in [syscall entry](syscall_entry.md).
* Vectors `0x20` to `0x2F` go to `irq_handler`, described in [irq](irq.md). The controller behind them is the [PIC](../devices/pic.md), and the line that drives the scheduler is the [timer](../devices/timer.md).
