# Panic

`cock::utils::panic(format, ...)` (`cock/utils/panic.hpp`) reports an unrecoverable kernel error and stops the machine. It is `noreturn`, and it can be called from any context, including an interrupt handler.

It prints `cock panic: ` and the formatted message with a newline to the kernel console, and then loops forever on `cli; hlt`: interrupts are off and the CPU never runs again. Nothing else happens, so the last lines of the console are the whole report.

## When the kernel panics

A panic is for a state the kernel cannot repair or a promise it cannot keep. A mistake by a program is never one: those fail the call or end the thread (see [exceptions](../../arch/interrupts/exceptions.md)).

* An exception raised in Ring 0, or with no current thread (see [events](../events.md)).
* A double fault or a non maskable interrupt, which panic with the instruction and stack pointer of the interrupted code (see [gdt](../../arch/cpu/gdt.md#fault-tasks)).
* A stack canary that changed (see [stack protector](stack_protector.md)).
* The pools of `Thread` or `Process` objects cannot grow, or a thread cannot get its kernel stack (see [pools](../process/pools.md)).
* The registry has no free process id (see [process ids](../process/ids.md)).
* The scheduler finds nothing to run, or the idle thread ends (see [scheduler](../scheduler/README.md)).
* A boot that cannot continue: a wrong Multiboot magic, no boot information or memory map, or no memory for the kernel page tables (see [bootloader](../../arch/boot/bootloader.md)).
