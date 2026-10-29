# Processes

A **`Process`** (`cock/core/task/process.hpp`) owns everything that threads of the same program share:

* the address space and the user-space limit of its ELF class,
* the capabilities ([capabilities](../capabilities.md)),
* the architecture context (`ArchProcessContext`), stored **by value**: on x86 it is a pointer to the I/O permission bitmap (null until the process is granted a port, so a process without ports costs nothing) and a version number. See [arch_context](arch_context.md),
* a name (the Multiboot module name),
* a **fixed table of mapping regions** (up to 32) and the count of pages it owns (up to 4096). Anonymous memory counts against that limit, device memory does not. See [process limits](../memory/process_limits.md).

A **`Thread`** (`cock/core/task/thread.hpp`) is an execution context inside a process. Threads can be `KERNEL` (Ring 0) or `USER` (Ring 3). See [threads](../threads/README.md).

* Every thread has a **16 KiB kernel stack** with an unmapped guard page below it.
* User threads also get a **64 KiB user stack** at the top of the user-space limit of their process.
* The kernel has its own process, which owns the kernel threads (the idle thread among them).
* A user process has from 1 to 8 threads (`MAX_THREADS_PER_PROCESS`). The process id is the id of its **first** thread, so it is also the address used by IPC and by `SYS_PROC_ALIVE`; every other thread has an id and an inbox of its own, and a message carries the id of the thread that sent it. Once the first thread ends, the process id no longer receives messages even if other threads run. See [process ids](ids.md).
* Each thread carries, besides its inbox, a mask of **pending interrupt notifications** (see [ipc](../ipc.md#interrupts-as-messages)).

When the last thread of a user process is destroyed, the process is destroyed with it: its address space, the page tables and every owned frame go back to the PMM, and the IRQ lines it had bound are released.

## Pages of this folder

| Page | Concept |
|------|---------|
| [spawn.md](spawn.md) | Building a process piece by piece with the `SYS_PROC_*` calls |
| [exec.md](exec.md) | Replacing the image of a process |
| [fork.md](fork.md) | Copying a process, copy on write |
| [exit_notifications.md](exit_notifications.md) | Telling the creator that a process ended |
| [ids.md](ids.md) | Process ids and their generation counter |
| [reaper.md](reaper.md) | Freeing dead threads and processes |
| [pools.md](pools.md) | Slab pools for `Thread` and `Process` |
| [arch_context.md](arch_context.md) | The per-process architecture context and the lazy I/O bitmap |
