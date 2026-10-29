# Events

`cock/core/events.cpp` is the single place where the architecture hands control to the core:

| Event | What the core does |
|-------|--------------------|
| System call | Runs the handler, then applies a pending restart and reschedule (see [syscall entry](../arch/interrupts/syscall_entry.md) and [blocking calls](scheduler/blocking_calls.md)) |
| Exception from Ring 3 | Resolves a copy on write fault and resumes; otherwise logs it, ends the faulting thread, and reschedules (see [exceptions](../arch/interrupts/exceptions.md)) |
| Exception from Ring 0 | Panics (see [panic](diagnostics/panic.md)) |
| Timer interrupt | `Scheduler::onTick` (see [scheduler](scheduler/README.md)) |
| Any other IRQ (see [irq](../arch/interrupts/irq.md)) | Masks the line and raises a pending notification on the thread that bound it, waking it if it waits |
