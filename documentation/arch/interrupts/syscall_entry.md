# System call entry

A program asks the kernel for something with `int 0x80`. It is the only gate the IDT lets Ring 3 raise (see [idt](idt.md)); an `int` to any other vector is a general protection fault. The list of calls, with numbers and arguments, is in [syscalls](../../core/syscalls.md).

## Convention

| Register | In | Out |
|----------|----|-----|
| `EAX` | System call number | Result |
| `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `EBP` | Arguments 1 to 6 | Unchanged, except for the calls that answer with more words: a call that is answered later, such as a reply to a synchronous call, writes the result and up to five more values into the saved registers of the waiting thread (`hal::SyscallFrame` in `cock/core/hal/tasking.hpp`) |

`isr_handler` (`cock/arch/x86/interrupt/isr/isr.cpp`) packs these registers into a `SyscallRequest` and passes it to the core. The core answers with the value for the saved `EAX`, the stack pointer to resume with, and whether to deliver the value at all.

## Dispatch

`on_syscall` (`cock/core/events.cpp`) records the frame, looks the handler up in the table of `SyscallManager`, runs it, and then applies what the handler asked for:

* **restart:** the saved instruction pointer moves back over the `int` (`hal::SYSCALL_INSTRUCTION_SIZE`, 2 bytes) and no value is delivered, so the call runs again when the thread is resumed;
* **reschedule:** `Scheduler::onYield` picks another thread and its stack pointer is returned.

This is how a call blocks without sleeping inside the kernel; see [blocking calls](../../core/scheduler/blocking_calls.md).

## The table

`SyscallManager::init()` (`cock/arch/x86/syscall/syscall_manager.cpp`) fills a table of `SYSCALL_TABLE_SIZE` entries (`0x273`) with `sys_not_implemented` and then registers each handler under its number from `cock/arch/x86/syscall/syscall_nums.hpp`. `SYS_SCHED_METRICS` is registered only in a kernel built with `SCHED_METRICS=1` (see [metrics](../../core/scheduler/metrics.md)).

A number outside the table, or one with no handler, runs `sys_not_implemented`, logs at trace level, and returns `-1` to the caller. It never panics: an unknown number is the caller's mistake, like any bad argument.

The handlers themselves live in the core, under `cock/core/syscall/`, grouped by area (`fs`, `hw`, `ipc`, `irq`, `mm`, `proc`, `security`). They validate every pointer and number the program passes before using it.
