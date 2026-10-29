# CPU exceptions

Vectors 0 to 31 are CPU exceptions (`ISR_NUM`). The stubs in `isr.s` save the frame and call `isr_handler` (`cock/arch/x86/interrupt/isr/isr.cpp`), which turns an exception into an `ExceptionInfo` for the core and returns the stack pointer to resume with (see [idt](idt.md#the-stubs)). The vector `0x80` takes the same path but is a system call (see [syscall entry](syscall_entry.md)).

## What the architecture reports

| Field | Meaning |
|-------|---------|
| vector, error code | As the CPU pushed them |
| instruction | The saved `EIP` |
| from user | The privilege level in the saved `CS` is 3 |
| has fault address, fault address | Only for a page fault (vector 14); `CR2` |
| write to present page | A page fault whose error code has both the present and the write bits: a write to a page that is mapped read only |
| name | A fixed message per vector ("Division by Zero", "Page Fault", ...) |

## What the core does

The decision is in `on_exception` (`cock/core/events.cpp`, see [events](../../core/events.md)):

1. **Copy on write.** A write to a present page from Ring 3 is first offered to `VirtualMemoryManager::resolveCopyOnWrite`. If the page was shared by [fork](../../core/process/fork.md) the frame is copied, the page becomes writable, and the thread resumes at the same instruction, as if nothing happened (see [copy on write](../../core/memory/copy_on_write.md)).
2. **Report.** Otherwise it logs the vector, the name, the ring, the instruction and the error code, and the faulting address for a page fault.
3. **Kernel fault.** An exception from Ring 0, or with no current thread, is a [panic](../../core/diagnostics/panic.md).
4. **User fault.** An exception from Ring 3 sets the exit status of the process to `FAULT_STATUS`, ends **only the faulting thread** and reschedules. The other threads of the process keep running (see [threads](../../core/threads/README.md)), and the death goes through the usual path (see [reaper](../../core/process/reaper.md)).

## Own stack for two exceptions

The non maskable interrupt (vector 2) and the double fault (vector 8) are task gates that run on their own stack, so a corrupt kernel stack cannot turn into a triple fault. See [gdt](../cpu/gdt.md#fault-tasks) and [idt](idt.md).
