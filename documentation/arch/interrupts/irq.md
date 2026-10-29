# Hardware interrupts (IRQ)

Hardware interrupts arrive on vectors `0x20` to `0x2F` (the [PIC](../devices/pic.md) after its remap). The stubs in `irq.s` push an `InterruptRegisters` frame and call `irq_handler` in `cock/arch/x86/interrupt/irq/irq.cpp` with the stack pointer (see [idt](idt.md#the-stubs)).

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

In order: the line is computed, spurious interrupts of lines 7 and 15 are dropped, the routine installed for the line runs, the end of interrupt is sent, and only then the core decides what runs next (see [events](../../core/events.md)). The architecture never calls the scheduler itself, and the end of interrupt goes out before a context switch may happen, so the line is not blocked while another thread runs.

## Installing a routine

`irq_install_handler(irq, handler)` stores a routine in a table of 16 entries and unmasks the line. `irq_uninstall_handler(irq)` masks the line and clears the entry. Both take the **vector** (`0x20 + line`), not the bare line number, and an out of range value is logged and ignored.

A routine (`IrqRoutine`) runs in Ring 0 inside the interrupt with interrupts off, so it must be short and must not block. It does not send the end of interrupt: the dispatcher does. Inside the kernel only the [timer](../devices/timer.md) installs a routine; every other line is handled by the core, which turns it into a pending notification for the Ring 3 driver that bound it (see [ipc](../../core/ipc.md#interrupts-as-messages) and [capabilities](../../core/capabilities.md)).
