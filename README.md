# MicroCOCK OS

MicroCOCK or _Micro Colombian Open Code Kernel_ is a 32-bit x86 higher-half microkernel operating system developed from scratch in C++ and Assembly. It aims to implement a modular and robust architecture where traditional kernel components (like device drivers) are isolated in user space (Ring 3), communicating with the core kernel via Inter-Process Communication (IPC). The design goal is _zero abstraction cost_: what the architecture layer provides is inlined into the core at compile time.

## Key Features

* **Microkernel Architecture:** Minimal kernel space. The VGA console and the keyboard run as isolated userland processes.
* **Higher-Half Kernel:** The kernel is mapped to `0xC0000000`, leaving the lower memory available for user processes.
* **Hardware Abstraction Layer without indirection:** each architecture ships its own `include/hal/*.hpp` directory that the build puts on the include path. Primitives such as port I/O, `cli`/`sti` or `invlpg` are inline functions, not calls into another translation unit.
* **Memory Management:**
    * Bitmap-based Physical Memory Manager (PMM) driven by the Multiboot memory map.
    * Page tables and address spaces (`Paging`), with global kernel pages (`CR4.PGE`).
    * Virtual Memory Manager (VMM) that holds the policy: user limits, device memory, guard pages.
    * Strong `PhysAddr` and `VirtAddr` types that cost nothing at run time.
    * Custom Kernel Heap (`kmalloc`, `kfree`) with block coalescing and splitting.
    * Per-process limits: a fixed table of mapping regions and a maximum number of owned pages. The kernel keeps a reserve of frames that programs cannot take.
    * The kernel code and constants are read only (`CR0.WP`), and executables are loaded writable and then sealed according to their segment flags.
* **Processes and Scheduling:**
    * A `Process` owns the address space, the capabilities and the architecture context; a `Thread` runs inside it.
    * Multi-Level Feedback Queue (MLFQ) scheduler with priority boosting and intrusive queues.
    * The core decides when to reschedule; the architecture only reports events.
* **Inter-Process Communication (IPC):** Ring-buffer-based asynchronous message passing between processes. Messages that carry authority or come from the kernel cannot be forged by a program.
* **Capabilities:** I/O ports, IRQ lines and physical memory are only reachable through explicit grants. See [capabilities](documentation/core/capabilities.md).
* **Interrupts to userland:** a driver binds an IRQ line, the kernel masks it and raises a pending notification on the driver thread (never lost, delivered by `SYS_IPC_RECV`), and the driver acknowledges it.
* **Dead threads are reaped by a kernel thread** that sleeps when there is nothing to free and wakes when a thread dies.
* **The FPU is off for programs:** `CR0.EM` is set, so floating point, MMX and SSE fault instead of sharing state between processes.
* **Custom LibC & LibCXX:** Freestanding implementation of standard C/C++ libraries, including dynamic memory allocation (`new`/`delete`) and global constructors/destructors support.
* **ELF Loader:** The kernel keeps a loader only for the Multiboot boot modules (ELF32 and ELF64 through two separate descriptions, each with its own user-space limit; the x86 build accepts ELF32 only). Every other program is loaded in user space by `cock::user::spawn`, which reads ELF32 and builds the process with the `SYS_PROC_*` calls.

## Project Architecture

The codebase is strictly separated to maintain the microkernel philosophy. The Core must not directly execute architecture-specific code, relying instead on the Hardware Abstraction Layer (HAL).

```text
.
├── cock/                   # Kernel Space
│   ├── arch/x86/           # Architecture-specific implementation (Boot, IDT, GDT, Paging, PMM, VGA)
│   │   └── include/hal/    # The HAL headers of this architecture (inline primitives and constants)
│   ├── core/               # Architecture-agnostic Core (VMM, Heap, Scheduler, IPC, Syscalls, Capabilities)
│   └── utils/              # Kernel utilities (Logger, Panic, Stack Smashing Protection)
├── libc/                   # Custom C Standard Library (freestanding)
├── libcxx/                 # Custom C++ ABI and memory management (cxa, new/delete)
├── shared/                 # Headers and data structures shared between Kernel and Userland
│   └── include/cock/       # IPC messages, capability ABI, LinkedList, RingBuffer, IntrusiveQueue
├── userland/               # User Space Programs (Ring 3)
│   ├── lib/                # `cock::user` API used by the programs
│   ├── init/               # Root server: the only process born with authority
│   ├── names/              # Name server: checks the `ipc=` policy when a program connects to another
│   ├── vga_driver/         # VGA Server running in userland
│   └── kbd/                # PS/2 keyboard driver running in userland
├── test/                   # Every test
│   ├── cock/               # In-kernel test suites
│   ├── host/               # Tests of the shared structures, built and run on the host
│   └── userland/           # Test programs run by init: probe, hello, aborter and the file and block clients
├── scripts/                # QEMU based and host test runners
├── documentation/          # Per-subsystem documentation
└── Makefile                # Master build system
```

### The Syscall Interface
Userland processes communicate with the kernel through software interrupts (`int 0x80`). The number goes in `EAX`, the arguments in `EBX`, `ECX`, `EDX`, `ESI`, `EDI` and `EBP`, and the result comes back in `EAX`. The current system call table includes:
* `SYS_EXIT` (0x01): Terminate current thread.
* `SYS_READ` (0x03): Read from a file descriptor.
* `SYS_WRITE` (0x04): Write to a file descriptor (routed to Serial or VGA Server). A call takes at most 128 bytes; the caller writes the rest with more calls.
* `SYS_REQUEST_RESOURCE` (0x07): Enable I/O ports in the process permission bitmap. Requires an `IO_PORTS` capability.
* `SYS_GETPID` (0x16): Retrieve current Process ID.
* `SYS_MMAP` (0x5A): Allocate dynamic memory (anonymous, or physical device memory with a `PHYSICAL_MEMORY` capability).
* `SYS_MUNMAP` (0x5B): Free dynamically allocated memory.
* `SYS_SCHED_YIELD` (0x9E): Yield CPU to the next ready thread.
* `SYS_IPC_SEND` (0x160): Send a message to another process.
* `SYS_IPC_RECV` (0x161): Receive the next interrupt notification or message of the thread, blocking when there is none.
* `SYS_CAP_GRANT` (0x170): Delegate part of a capability to another process; returns the id of the node of the derivation tree that records it. A fifth argument lets the receiver pass the capability on (`GRANT_DELEGATE`), which is how the name server may give `IPC` capabilities.
* `SYS_CAP_ACCEPT` (0x192): Choose which capability types a `CAPABILITY_TRANSFER` message may give the caller (a mask, one bit per type); a process accepts nothing until it calls it. `SYS_IPC_SEND` of that message type gives the receiver a copy of part of a capability of the sender, recorded in the derivation tree, and returns the node id so the sender can revoke it.
* `SYS_CAP_REVOKE` (0x18A): Revoke a delegated capability and everything derived from it; the caller must hold an ancestor of the node. Closes the ports, unbinds the lines and unmaps the device memory that the holders opened with it.
* `SYS_IRQ_BIND` (0x171): Route an interrupt line to the calling process. Requires an `IRQ` capability.
* `SYS_IRQ_ACK` (0x172): Re-enable an interrupt line after handling it.
* `SYS_MODULE_PROCESS` (0x173): Find the process id of the program the kernel started from a boot module, by module name; needs grant authority. Programs find a server through the name server instead.
* `SYS_SHM_CREATE` (0x174): Create a shared memory object of a number of pages, paid by the caller.
* `SYS_SHM_SHARE` (0x175): Let another process map an object, read only or read and write; needs an IPC capability with that process (or in the other direction).
* `SYS_SHM_MAP` (0x176): Map a whole object into the caller, read only or read and write.
* `SYS_SHM_RELEASE` (0x177): Drop the right of the caller to an object.
* `SYS_MODULE_OPEN` (0x178): Open a data boot module by name as a read only shared memory object; needs a `BOOT_MODULE` capability.
* `SYS_MODULE_INDEX` (0x179): Index of a data boot module by name; needs grant authority.
* `SYS_PROC_CREATE` (0x17A): Create a process with an empty address space and no thread, and get a handle for it; needs a `SPAWN` capability. An optional argument gives the service label and IPC capabilities that the process is born with. At most 2 pending processes per caller and 8 in the system.
* `SYS_PROC_MAP` (0x18C), `SYS_PROC_WRITE` (0x18D), `SYS_PROC_PROTECT` (0x18E): Map zeroed writable pages into a pending process, copy bytes into them, and make them read only. Only the creator may use the handle; at most 4 MiB, never the first page or the user stack.
* `SYS_PROC_START` (0x18F): Create the first thread of a pending process at an entry point and let it run; returns the process id. `SYS_PROC_DISCARD` (0x190): destroy a pending process. A pending process dies with its creator. The ELF parsing is not in the kernel path of these calls: `cock::user::spawn` (`userland/lib/include/cock/user/spawn.hpp`) reads the image and calls them.
* `SYS_SERVICE_SET` (0x18B): Give a running process its service label, once; needs grant authority. Used for the boot modules.
* `SYS_PROC_ALIVE` (0x17B): Tell whether a process id is alive.
* `SYS_THREAD_PROCESS` (0x191): Give the process a thread belongs to; a server that serves a synchronous call sees the id of the calling thread and uses this to know whose it is.
* `SYS_IPC_TRY_RECV` (0x17C): Like `SYS_IPC_RECV` but returns 1 at once when there is nothing to receive.
* `SYS_IPC_RECV_TIMEOUT` (0x17D): Like `SYS_IPC_RECV` but returns 1 when the given milliseconds pass with nothing to receive.
* `SYS_SLEEP` (0x17E): Block the caller for at least the given milliseconds (10 ms resolution).
* `SYS_THREAD_CREATE` (0x17F): Start another thread of the caller process at an entry point, on a stack the caller provides; returns the thread id.
* `SYS_FUTEX_WAIT` (0x180): Block while a 32 bit word still holds an expected value, with an optional timeout.
* `SYS_FUTEX_WAKE` (0x181): Wake up to a given number of threads of the caller process waiting on a word.
* `SYS_GETTID` (0x182): Retrieve the id of the calling thread.
* `SYS_SCHED_METRICS` (0x189): Write the response, CPU and turnaround ticks of the calling thread. Registered only in a kernel built with `SCHED_METRICS=1`; otherwise it fails like any unknown call.
* `SYS_IPC_CALL` (0x183), `SYS_IPC_ACCEPT` (0x184), `SYS_IPC_REPLY` (0x185), `SYS_IPC_REPLY_WAIT` (0x186), `SYS_IPC_DEFER` (0x187), `SYS_IPC_REPLY_TO` (0x188): Synchronous calls with four words in registers each way, a direct switch to the server and an optional timeout. `accept` also wakes for an asynchronous message or an interrupt. `defer` detaches the call being served and returns a ticket; `reply_to` answers it later.

## Building and Running

### Prerequisites
To build and run MicroCOCK, you need a standard i686-elf cross-compiler toolchain, NASM, GRUB, and QEMU.
* `i686-elf-gcc` / `i686-elf-g++`
* `i686-elf-ld`
* `nasm`
* `grub-mkrescue` (and `xorriso`, `mtools`)
* `qemu-system-i386`
* `python3`, `sfdisk` from `util-linux` and `mformat`/`mcopy` from `mtools` and `mke2fs` from `e2fsprogs` 1.43 or later (only for the userland end to end test, which builds a test disk)

Without a cross-compiler, the host `g++` and `ld` with `-m32` work too (`HOST_TOOLCHAIN=1`); install `gcc-multilib` and `g++-multilib`.

### Build Commands

The project uses a standard Makefile to handle the compilation of the kernel, libraries, and userland applications.

Compile the kernel and userland binaries:
```bash
make
```

Generate a bootable ISO using GRUB Multiboot:
```bash
make iso
```

Build the OS and launch it automatically in QEMU:
```bash
make run
```

Same as `run`, without a window; the serial port goes to your terminal:
```bash
make run-headless
```

Without a disk, only `/` exists (the `initrd`, read only); `/fat` and `/ext` have nothing behind them. To attach one, build the sample disk (an MBR or GPT image with a FAT32 and an ext2 volume; it needs `sfdisk`, `mtools` and `e2fsprogs`) and pass it to `run`:
```bash
make disk
make run DISK=build/disk.img
```
`DISK_SCHEME=gpt` builds the GPT variant. The guest writes into the image file, so run `make disk` again to get the original contents back.

The system ends in a shell (`cock>`): type `help` for the commands (`ls`, `cat`, `write`, `mkdir`, `rm`, `mv`, `sleep`, `clear`, ...). See [the shell](documentation/userland/shell.md).

Clean the build directory:
```bash
make clean
```

Run in test mode (compiles with `COCK_TEST_MODE` flag):
```bash
make test
```

Build options:

| Option | Effect |
|--------|--------|
| `HOST_TOOLCHAIN=1` | Use the host `g++ -m32` and `ld -m elf_i386` instead of `i686-elf-*` |
| `USE_LTO=1` | Link time optimization (the kernel text shrinks about a quarter) |
| `LOG_LEVEL=<level>` | Lowest kernel log level compiled in: `TRACE` (default), `DEBUG`, `INFO`, `WARN`, `ERROR` or `FATAL`. Calls below it generate no code |
| `SCHED_METRICS=1` | Record the response, CPU and turnaround ticks of every thread and register `SYS_SCHED_METRICS`. Off by default: the loaded kernel (`text`, `data`, `bss`) is the same as without the option |
| `TEST_BUILD=1` | Build the in-kernel test suite |
| `USER_EXTRA_FLAGS=...` | Extra compiler flags for the userland programs, for example `-fno-omit-frame-pointer` |

The userland programs are rebuilt when a header they include changes. After switching toolchains or flags, run `make clean` once.

### Testing

The in-kernel test suite runs inside QEMU and reports through the serial port;
`scripts/qemu_test.sh` builds it, runs it without a window and turns the
`isa-debug-exit` status into the script exit code:
```bash
bash scripts/qemu_test.sh
```

The kernel suites cover the heap, the ELF loader, processes, the I/O permission
map, paging protections, the physical memory manager, the scheduler, IPC,
capabilities and the reaper.

`scripts/host_test.sh` compiles the structures of `shared/` (`RingBuffer`,
`IntrusiveQueue`, `LinkedList`, the message layout), `CapabilitySet` and `CapabilityTree` with the
host `g++` and runs them as a normal program, with no emulator:
```bash
bash scripts/host_test.sh
```

`scripts/qemu_userland_test.sh` boots the normal ISO, types on the simulated
keyboard and checks that the userland drivers, the capability checks and the
screen output work end to end:
```bash
bash scripts/qemu_userland_test.sh
```

Both scripts accept extra `make` arguments. To build with the host `g++`/`ld`
(for example on a CI runner without the cross-compiler) and with link time
optimization:
```bash
bash scripts/qemu_test.sh HOST_TOOLCHAIN=1 USE_LTO=1
```

The userland test is also run with the programs built with frame pointers, which is how some distributions compile by default:
```bash
bash scripts/qemu_userland_test.sh HOST_TOOLCHAIN=1 USER_EXTRA_FLAGS=-fno-omit-frame-pointer
```

The test attaches a disk with an MBR partition table; `DISK_SCHEME=gpt` uses a GPT one:
```bash
DISK_SCHEME=gpt bash scripts/qemu_userland_test.sh HOST_TOOLCHAIN=1
```

`scripts/qemu_fuzz_test.sh` boots the fuzzer, which calls the system calls with plausible and hostile arguments and fails on a kernel panic, a triple fault or a hang (see [the fuzzer](documentation/userland/fuzzer.md)); `FUZZ=root` gives it every capability:
```bash
FUZZ=user SEED=1 ROUNDS=3000 bash scripts/qemu_fuzz_test.sh HOST_TOOLCHAIN=1
```

The same checks run on every push through `.github/workflows/ci.yml`.

### Current limitations

* At most 8 threads per user process. A thread runs on a stack the program provides, an exception ends only the thread that caused it, and `SYS_EXIT` ends only the calling thread: the process is gone when its last thread ends.
* A process that exits tells its spawner only if the spawner asked for it (`PROCESS_EXITED`). The notice is kept in a record of the parent, never lost to a full inbox; a process can have at most 16 notified children running or unread, and `SYS_PROC_START` of a process created with the flag fails past that.
* Capabilities are assigned at boot (the first program module, `init`, holds the authority); what is delegated can be revoked by a holder of an ancestor (`SYS_CAP_REVOKE`), what the first program holds cannot; each process holds up to 16.
* IPC has an asynchronous inbox with a small capacity per thread, and a synchronous call and reply in registers with timeouts.
* The user C library has buffered `printf`, `malloc` and `operator new` (locked with a futex mutex, safe for threads), and file calls (`open`, `read`, `write`, `lseek`, `close`, `unlink`, `mkdir`, `rmdir`, `rename`) over the file server client.
* `SYS_WRITE` is a debug and fallback channel owned by the kernel. Programs write and read descriptors 0 to 2 through the `console` server (the C library links the client by default); `read(0)` blocks in the kernel until a line is ready, through a deferred reply.
* A process maps at most 4096 pages (16 MiB) in at most 32 regions.
* The x86 build has no PAE, so no NX bit and no memory above 4 GiB.
* A kernel stack overflow reaches the guard page but ends in a triple fault, without a message.
* Only exercised on QEMU.

### References
To create microcock I've reseach a lot of sites, videos and even AI models to learn ways to implement certain things,
so this is the space where I put those references that really helped me to move this project forward:

##### Sites
- [Higher Half Bare Bones](https://wiki.osdev.org/Higher_Half_x86_Bare_Bones): Initial Paging and Higher half kernel were taken from this page

##### Videos
- [OliveStem OS course](https://www.youtube.com/playlist?list=PL2EF13wm-hWAglI8rRbdsCPq_wRpYvQQy): my man olive basically help me to have a path to follow. Interrupts, Keyboard, Multiboot, Memory management and other things at the beginnig where possible because of him.

## Roadmap

What the kernel already does, and what is planned next. Planned items are listed in the order they are expected to be built; none of them is started yet unless it is marked as in progress.

### Done

**Kernel foundations**
- Boot through Multiboot, physical and virtual memory management (PMM, paging, VMM, heap), a scheduler, IPC, and ELF32 and ELF64 loaders.
- A zero-cost hardware abstraction layer, typed `PhysAddr` and `VirtAddr`, processes with their own context, and a lazy I/O permission bitmap.
- Own stacks for double fault and NMI, and a generation counter in process ids.
- Timers (`sleep` and receive with a timeout), user threads with blocking primitives and exit notifications, and synchronous IPC with a register fast path.

**Drivers and servers in Ring 3**
- VGA and a PS/2 keyboard, with capabilities, and interrupts delivered as notifications.
- A console server: `printf` and `read(0)` go through it, readers block through deferred replies, and the client is linked by default.
- A minimal shell with built-in commands (version 0.2.0, see [the shell](documentation/userland/shell.md)).
- A syscall fuzzer that runs in Ring 3 and fails on any kernel panic, in unprivileged and root modes.
- A name server in Ring 3 (`names`, the second boot module): a program connects to a server by name or path (`/run/console`, up to 60 bytes, from any thread), the `ipc=` policy of the manifest is checked then, and the right to talk to the server is given then. The kernel no longer looks processes up by name; it only finds the boot modules for `init`.

**Hardening**
- Authenticated reserved messages, per-process memory limits, a reaper thread, a write-protected kernel image, and the FPU disabled for programs.
- `-Werror` in the kernel and userland builds, compile-time filtering of log levels, and fixed-size pools for threads and processes.
- Reuse of kernel address ranges and freeing of the early boot memory.

**File system outside the kernel**
- Shared memory between processes, and data boot modules with an `initrd` built as a tar archive.
- A `ramfs` server, `spawn` from files, and a manifest read by `init` that lists each program and its capabilities.
- An ATA driver (PIO), a block protocol, MBR and GPT partition tables, a FAT32 server and an ext2 server, both with write support.
- A `vfs` with a mount table, and `open`, `read`, `write` and `close` in the C library.
- File system follow-ups: `rename` over an existing name, hard links for ext2, and a single owner of the clock.

**Tests and C library**
- Tests for the physical memory manager, the scheduler, IPC and the reaper, and host-side tests for the shared data structures.
- Buffered `printf`, `malloc`, `calloc`, `realloc`, `free` and `operator new`, with `malloc` locked by a futex mutex so threads can share the heap.
- Scheduler metrics (response, CPU and turnaround ticks per thread) behind `SCHED_METRICS=1`, off by default, read by a thread with `SYS_SCHED_METRICS`.
- The scheduler charges each priority level for the ticks a thread actually uses, summed over its runs, so yielding before the quantum ends no longer keeps a high level.
- A probe check that no thread sleeps forever when a futex word is set and waiters are woken.
- A thread library in `userland/lib` with `JoinableThread` (`join`, optional time limit), `ConditionVariable` and `Semaphore` next to `Mutex`, all over the futex calls and with no change in the kernel.

### Planned

- Keeping the kernel small is the next goal, in this order. First, capabilities as data (below): the kernel stops knowing program names, and IPC gets a capability of its own. Then a page that lists every concept the kernel holds and says why each one cannot live outside it (done: `documentation/core/kernel_concepts.md`), the ELF loader and `spawn` moved out of the kernel into a user library (done for programs started at run time; the kernel keeps a loader only for the Multiboot boot modules), the name lookup moved out of the kernel into the name server (done), and the debug channel (`SYS_WRITE`, `SYS_READ`, kernel console) moved out of the core, kept only behind a build option.
- A route to Unix style IPC, where the capability plays the part of the file descriptor. Step by step: a stable service label with an IPC capability that decides who may send, call or share memory with whom (done); a name service in Ring 3 that replaces the lookup of the kernel, so permission is checked when a client connects (done); capability transfer in messages, so a process can give another a capability it holds and the receiver gets it only when it accepted that type (done); endpoints and handles, so messages are addressed by handle and not by process id; capabilities inherited by `fork` and kept by `exec`; and a descriptor layer in the C library, so the shell can join programs with pipes and redirection.
- Page faults handled by a server, so memory policy can live outside the kernel. This is the largest item and needs a decision first.
- A process model like Linux's, kept by a process manager server (`pm`) outside the kernel, as in Minix 3; the kernel keeps only the primitives it must. It covers process states (running, sleeping, stopped, zombie) shown to `ps`; `wait` (a parent collects a child's exit status, and zombies until then), `exec` (a process replaces its program and keeps its pid), signals with `kill`, process groups so Ctrl+C reaches only the foreground job, a process table for `ps`, and `nice`.
- Fuzzing of corrupt ELF images and of the file server protocols, so a malformed file or request cannot crash a server or the kernel.
- `fork`, next to `spawn`: a process that splits into two, each with its own memory. It comes after `wait` and `exec`. Which file descriptors the child keeps is still to be decided.
- Running programs from the shell (this needs a way to `spawn` from it), then `ps`, history, Ctrl+C and quoting.
- A `tmpfs`: a writable file server that keeps its files in memory and is mounted on `/tmp`, so the system has a place to write without a disk.
- A consistency check when FAT32 and ext2 volumes are mounted, so a damaged volume is reported instead of read silently. This is a check, not a journal.
- A heap test under random allocation and release, to see whether the free space fragments.
- An out-of-memory test: a program that uses up the physical memory must get an error, and the kernel must keep running.
- A kernel stack overflow reported with a message instead of a silent reset.
- Capabilities as data: the kernel no longer knows the name of any program (done for `init` and the VGA driver), a test checks that it boots with no user program at all (done), capabilities can be revoked (done in the kernel; `init` does not use it yet), IPC gets a capability of its own (the first step of the route above, which also decides who may write the disk), a capability may replace the grant authority needed to `spawn`, and DMA-capable ports get a policy.
- Platform: APIC and IOAPIC, PAE with NX and memory above 4 GiB, SMP (with the TLB of every CPU updated when a page table changes), and an x86_64 port.

Symbolic links are postponed until a program needs them.

## Documentation
For more detailed information regarding specific subsystems, please check the `documentation/` folder, split by layer: [core](documentation/core/README.md), [arch](documentation/arch/README.md), [userland](documentation/userland/README.md), [shared](documentation/shared/README.md) and [libs](documentation/libs/README.md). Every program in `userland/` has its own `README.md`. The pages:
* [Learning path](documentation/learning_path.md): Where to start reading, from boot to the userland.
* [Project Structure](documentation/project_structure.md): Directory layout and the HAL.
* [Bootloader](documentation/arch/bootloader.md): Multiboot and early entry process.
* [Kernel Main](documentation/core/kernel_main.md): Initialization sequence.
* [Memory Management](documentation/core/memory_management.md): PMM, paging, VMM, and Heap.
* [Task Management](documentation/core/task_management.md): Processes, threads, scheduler and MLFQ.
* [Inter-Process Communication](documentation/core/ipc.md): Syscalls and messaging.
* [Capabilities](documentation/core/capabilities.md): Authority over ports, IRQs and physical memory.
* [Kernel concepts](documentation/core/kernel_concepts.md): Every concept the kernel holds, why it cannot live outside, and what is planned to move.
* [Userland](documentation/userland/userland.md): Boot modules, the system call ABI and the drivers.
* [GDT](documentation/arch/gdt.md) & [IDT](documentation/arch/idt.md): CPU descriptor tables and interrupt handling.
* [VGA](documentation/userland/vga.md): Kernel console and the userland display server.
* [Keyboard](documentation/userland/keyboard.md): The PS/2 driver in userland and how an interrupt reaches it.
* [Block storage](documentation/userland/block_storage.md): The ATA driver, partition tables and the block protocol.
* [FAT32](documentation/userland/fat32.md): The FAT32 server and how the `vfs` joins it to the initrd.
* [ext2](documentation/userland/ext2.md): The ext2 server, its feature policy and symbolic links.
