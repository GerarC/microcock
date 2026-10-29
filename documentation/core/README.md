# Core

The code under `cock/core`. It is architecture agnostic: it never includes `cock/arch/...`, only `<hal/...>`.

| Page | Topic |
|------|-------|
| [kernel_main.md](kernel_main.md) | `cock_main`: module loading, root capabilities, idle and reaper threads |
| [memory/](memory/README.md) | PMM, paging, copy on write, VMM, kernel heap, shared memory, data boot modules |
| [process/](process/README.md) | Processes, spawn, exec, fork, exit notices, process ids, reaper, pools |
| [scheduler/](scheduler/README.md) | MLFQ scheduler, switching, blocking calls, timeouts, metrics |
| [threads/](threads/README.md) | Threads, futex, mutex, condition variable, semaphore, join |
| [events.md](events.md) | The single place where the architecture hands control to the core |
| [elf_loader.md](elf_loader.md) | The ELF loader that remains in the kernel, for the boot modules only |
| [hal.md](hal.md) | The `<hal/...>` headers and the contract between the core and the architecture |
| [diagnostics/](diagnostics/README.md) | `Logger`, `panic` and the stack protector |
| [syscalls.md](syscalls.md) | Every system call: number, purpose, arguments, return value, authority needed |
| [ipc.md](ipc.md) | Asynchronous messages, synchronous calls, interrupts as messages |
| [capabilities.md](capabilities.md) | Authority over ports, IRQs, physical memory and boot modules |
| [endpoints.md](endpoints.md) | Endpoints, handles and badges: sending by a right instead of a process id |
| [kernel_concepts.md](kernel_concepts.md) | Every concept the kernel holds, why it cannot live outside, and what is planned to move |

Where the core meets the machine: [arch/](../arch/README.md). What the core serves: [userland/](../userland/README.md).
