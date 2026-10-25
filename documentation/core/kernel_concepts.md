# Kernel concepts

The kernel of a microkernel is as small as the concepts it must know, not as the number of lines it takes. This page lists every concept the kernel holds, says why it cannot live in Ring 3, and gives a verdict. It is the check for every change: a change that adds a concept to the kernel must say why that concept cannot live outside.

The rule comes from Liedtke: a concept belongs in the kernel only if moving it out would break protection (a program could then harm another one or the machine) or would make the system too slow to use.

## Verdicts

| Verdict | Meaning |
|---------|---------|
| **primitive** | Stays. It needs the privilege of the kernel, or the cost of leaving would be too high. |
| **service** | Must move out. The roadmap item that moves it is named. |
| **debug** | Not needed by a working system. It stays only behind a build option. |
| **policy** | A decision, not a mechanism. It stays for now on purpose, with the reason. |

## System calls

| Call | What it is | Why it is in the kernel | Verdict |
|------|-----------|-------------------------|---------|
| `SYS_EXIT` | End the caller and record its status | Only the kernel can free a process | primitive |
| `SYS_GETPID`, `SYS_GETTID` | Ids of the caller | The ids belong to the kernel's tables | primitive |
| `SYS_SCHED_YIELD` | Give up the CPU | Only the kernel switches threads | primitive |
| `SYS_SLEEP` | Block for some milliseconds | Needs the timer and the scheduler | primitive |
| `SYS_THREAD_CREATE` | New thread in the caller's process | Thread contexts are kernel objects | primitive |
| `SYS_THREAD_PROCESS` | The process a thread belongs to | A server sees the id of the calling thread of a synchronous call and must know whose it is; only the kernel knows the thread tables. It reads, it grants nothing | primitive |
| `SYS_PROC_ALIVE` | Is a thread alive | A query on the kernel's tables; the exit notice covers the same need for a parent, so this call may become redundant | primitive (to review) |
| `SYS_FUTEX_WAIT`, `SYS_FUTEX_WAKE` | Block and wake on a user address | The check of the word and the sleep must be one step, which only the kernel can do | primitive |
| `SYS_MMAP`, `SYS_MUNMAP` | Map and unmap anonymous pages, or device memory with `PHYSICAL_MEMORY` | Page tables cannot leave the kernel | primitive |
| `SYS_SHM_CREATE`, `SYS_SHM_SHARE`, `SYS_SHM_MAP`, `SYS_SHM_RELEASE` | Shared memory objects | Frames are mapped into two address spaces; this is the map and grant of L4 | primitive |
| `SYS_IPC_SEND`, `SYS_IPC_RECV`, `SYS_IPC_TRY_RECV`, `SYS_IPC_RECV_TIMEOUT` | Asynchronous messages with a bounded inbox; `SYS_IPC_SEND` reaches only the threads of the caller's own process | Messages cross address spaces (through `SYS_EP_SEND`); the inbox also carries interrupt notifications and exit notices | primitive |
| `SYS_IPC_CALL`, `SYS_IPC_ACCEPT`, `SYS_IPC_REPLY`, `SYS_IPC_REPLY_WAIT`, `SYS_IPC_DEFER`, `SYS_IPC_REPLY_TO` | Synchronous call with a direct hand-off | The fast path of the system; the kernel writes the answer into the saved frame of the waiter | primitive |
| `SYS_IRQ_BIND`, `SYS_IRQ_ACK` | Deliver a hardware interrupt as a message | Only the kernel takes interrupts | primitive |
| `SYS_REQUEST_RESOURCE` | Ask for I/O ports | The port bitmap is read by the CPU | primitive |
| `SYS_CAP_GRANT` | Pass part of a capability to another process, optionally with the right to pass it on | The kernel enforces capabilities, so it must see every transfer. The right to pass on (`GRANT_DELEGATE`) lets the name server hand out handles without holding root authority | primitive |
| `SYS_SERVICE_SET`, the service label of a process, and the `IPC` capability that limits which labels a spawner may give | The stable identity under which the name server applies its policy and under which a restarted program is the same program. The kernel enforces who may give a label, so that check cannot live in Ring 3; the policy (which program gets which label and which rights) is data in the manifest read by `rs`. Since step E3c the kernel compares no label when a message is sent, a call is made or memory is shared: the handle is the permission | primitive |
| `SYS_CAP_ACCEPT` and the `CAPABILITY_TRANSFER` message | A capability travels in a message, as `SCM_RIGHTS` does; the receiver chooses which types it accepts | The copy is a node of the derivation tree and the kernel enforces capabilities, so only it can make the copy; the accept mask keeps a sender from filling the table of a victim | primitive |
| `SYS_EP_REPLY`, `SYS_EP_FORWARD` and the table of reply tokens | A one-shot right to answer a message, which can be passed on with the request | The kernel has to know who asked and who may answer; a token held by a process cannot be forged or used twice, and it takes no entry of the capability table | primitive |
| `SYS_EP_CREATE`, `SYS_EP_BIND`, `SYS_EP_SEND`, `SYS_EP_CALL`, the endpoint table and the `ENDPOINT` capability | A numbered mailbox bound to one process, and a handle that sends to it with a badge the kernel stamps | A handle is a capability and the kernel enforces capabilities; the badge has to come from the kernel for a server to trust it. The policy (which client gets which badge) is in Ring 3, with whoever holds the mint right | primitive |
| `SYS_CAP_REVOKE` | Take a delegated capability, and what derives from it, away from its holders | Only the kernel can close ports, unbind lines and unmap device pages of another process | primitive |
| `SYS_MODULE_OPEN` | Open a boot module as a read only shared object | The boot loader hands the modules to the kernel, and only the kernel can turn their frames into a shared object | primitive |
| `SYS_MODULE_INDEX` | Find a boot module by name | A lookup by name; a name service | service (stage 9, with the data policy of the boot) |
| `SYS_PROC_CREATE`, `SYS_PROC_MAP`, `SYS_PROC_WRITE`, `SYS_PROC_PROTECT`, `SYS_PROC_START`, `SYS_PROC_DISCARD` | Build a process piece by piece: an empty address space, zeroed pages, bytes copied in, pages made read only, the first thread, or the whole thing thrown away | Only the kernel can create an address space, map frames into it and create the first thread. The ELF format is not here: `cock::user::spawn` reads it (M1, done). Every range is validated by the kernel, so a broken loader can only hurt its own spawner | primitive |
| `SYS_MODULE_PROCESS` | Find the process the kernel started from a boot module, by module name (grant authority only) | `rs` needs the process id of the programs that exist before it runs, and only the kernel knows which process each boot module became. It searches no process but a boot module, so it is not a registry of names. The name service of every other program is the `names` server (E1, done) | primitive (narrow) |
| `SYS_WRITE`, `SYS_READ` | Console output and input for descriptors 0 to 2 | A debug channel: the console server does the real work, and the kernel forwards to the display server or to its own console | debug for now; kept as the door of a Linux personality (M2b, skipped; see the roadmap) |
| `SYS_SCHED_METRICS` | Read the scheduler figures of the caller | Counts are taken inside the scheduler | debug (already behind `SCHED_METRICS=1`) |

## Kernel objects and mechanisms

| Concept | Why it is in the kernel | Verdict |
|---------|-------------------------|---------|
| Process and address space | Page tables | primitive |
| Thread, saved context, kernel stack | Context switch | primitive |
| Capability table (`IO_PORTS`, `IRQ`, `PHYSICAL_MEMORY`, `BOOT_MODULE`) | The kernel is what enforces them | primitive |
| Message and reserved messages with the kernel as an authenticated sender | Only the kernel can prove who sent a message | primitive |
| Exit notice record of a parent (`MAX_NOTIFIED_CHILDREN`) | Only the kernel knows when a process ends; a bounded inbox could lose it | primitive |
| Timeouts | Needs the timer tick | primitive |
| Physical memory manager, virtual memory manager, kernel heap, slab and range pools | The kernel's own memory | primitive |
| Per process frame quota | A memory policy; a memory server of the sigma0 kind could hold it. Not planned | policy |
| Multilevel scheduler and its allowance per level | The mechanism needs the kernel; the levels are a small fixed policy | policy |
| GDT, TSS, IDT, PIC, timer, double fault and NMI stacks | The CPU and the interrupt controller | primitive |
| Idle thread and reaper thread | The kernel frees its own objects | primitive |
| Port bitmap per process, built on the first grant | The CPU reads it | primitive |
| Pending processes (`PendingProcesses`) | A process has no id until it has a thread, so a process under construction needs a name that only its creator can use, and it must die with the creator | primitive |
| ELF loader for boot modules (`core/loader`, ELF32 and ELF64) | Reads a file format, but only for the Multiboot modules `rs`, `names`, `ramfs` and `vga_driver`, which start before any program that could load them. It is reachable from no system call | boot only (M1: the loader that served `spawn` is gone) |
| Process names | Only the name of a boot module, which `SYS_MODULE_PROCESS` reads (the names `rs` and `vga_driver` went on 2026-10-23: the first program module is the root task, and the screen goes to whoever maps it). The registry of names for programs is the `names` server (E1, done); the kernel must not know programs by name | boot only |
| Kernel console and `Logger` output | Debug output and the early boot | debug (M2a, skipped for now) |

## Open questions
These come out of the list and are not decided; each one is a question for the next time the area is touched, not a roadmap item.

- **Two message paths.** L4 has only synchronous IPC. Here the asynchronous inbox is also used for interrupt notifications and exit notices, so it stays for now. Check, when stage 9 gives IPC its capability, whether the asynchronous send can be cut down to notifications from the kernel.
- **`SYS_PROC_ALIVE`.** With the exit notice of a parent, the call may no longer be needed. A process that is not the parent still uses it.
- **Who may spawn.** `SYS_PROC_CREATE` needs the `SPAWN` capability (9d), and `SYS_MODULE_INDEX` is open only to the holder of a grantable capability. With M1 done, the `SPAWN` capability is what decides who may create an address space.

## Keeping the list true
Update this page in the same change that adds, removes or moves a system call, an object or a policy of the kernel. When a roadmap item in stage M or stage 9 ends, the rows it names change verdict or go.
