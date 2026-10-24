# Core

The code under `cock/core`. It is architecture agnostic: it never includes `cock/arch/...`, only `<hal/...>`.

| Page | Topic |
|------|-------|
| [kernel_main.md](kernel_main.md) | `cock_main`: module loading, root capabilities, idle and reaper threads |
| [memory_management.md](memory_management.md) | PMM, paging, VMM, kernel heap, shared memory, data boot modules |
| [task_management.md](task_management.md) | Processes, threads, MLFQ scheduler, timeouts, futexes, the reaper |
| [ipc.md](ipc.md) | Asynchronous messages, synchronous calls, interrupts as messages |
| [capabilities.md](capabilities.md) | Authority over ports, IRQs, physical memory and boot modules |
| [endpoints.md](endpoints.md) | Endpoints, handles and badges: sending by a right instead of a process id |
| [kernel_concepts.md](kernel_concepts.md) | Every concept the kernel holds, why it cannot live outside, and what is planned to move |

Where the core meets the machine: [arch/](../arch/README.md). What the core serves: [userland/](../userland/README.md).
