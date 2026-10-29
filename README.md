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
│   └── include/cock/       # `abi` (protocols and capability types), data structures, ELF, `exec` arguments, IPC messages, paths, the `pm` table and the shell parsers
├── userland/               # User Space Programs (Ring 3)
│   ├── lib/                # `cock::user` API used by the programs
│   ├── drivers/            # Programs that hold hardware capabilities: `ata`, `console`, `kbd`, `rtc`, `vga_driver`
│   ├── servers/            # Servers that hold policy and no hardware: `rs` (the only process born with authority), `pm`, `init`, `names`, `ramfs`, `vfs`, `fat`, `ext`, `pipe`, `tty`
│   └── apps/               # The programs of `/bin`: the shell `sh` and the commands (`ls`, `cat`, `ps`, `echo`, ...)
├── initrd/                 # Files packed into the initial ramdisk (`etc/manifest.conf`, `etc/mounts.conf`)
├── test/                   # Every test
│   ├── cock/               # In-kernel test suites
│   ├── host/               # Tests of the shared structures, built and run on the host
│   ├── initrd/             # Files and manifest lines added to the initrd of the test build
│   └── userland/           # Test programs run by rs: probe, fuzz, the file, block and process clients
├── scripts/                # QEMU based and host test runners
├── documentation/          # Per-subsystem documentation
└── Makefile                # Master build system
```

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

The system ends in a shell (`cock>`): type `help` for the commands (`ls`, `cat`, `echo`, `mkdir`, `rm`, `mv`, `ps`, `sleep`, `clear`, ...); `echo text > file` and `cat file | cat` work. See [the shell](documentation/userland/shell.md).

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
map, paging protections, the physical memory manager and its frame references,
copy on write, the scheduler, IPC, capabilities (and their inheritance by `fork`)
and the reaper.

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
* Capabilities are assigned at boot (the first program module, `rs`, holds the authority); what is delegated can be revoked by a holder of an ancestor (`SYS_CAP_REVOKE`), what the first program holds cannot; each process holds up to 16.
* IPC has an asynchronous inbox with a small capacity per thread, a queue of 16 messages for each endpoint (256 for all of them) and a list of waiting callers per endpoint, and a synchronous call and reply in registers with timeouts.
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
- A pipe server (`pipe`) and `pipe()` in the C library, over a table of descriptors with `dup` and `dup2`: readers and writers wait while a pipe is empty or full, and standard output can be redirected into a pipe or a file (see [the pipe server](documentation/userland/pipe.md)).
- A minimal shell with built-in commands (version 0.2.0, see [the shell](documentation/userland/shell.md)).
- A syscall fuzzer that runs in Ring 3 and fails on any kernel panic, in unprivileged and root modes, with structured phases that send hostile requests to the file servers and corrupt ELF images to `spawn` and `exec`.
- A name server in Ring 3 (`names`, the second boot module): a program connects to a server by name or path (`/run/console`, up to 60 bytes) through the handle it was born with, the `ipc=` policy of the manifest is checked then, and a handle on the server is given then. The kernel no longer looks processes up by name; it only finds the boot modules for `rs`.
- The process model starts to move out of the kernel, as in Minix 3: `rs` (the root server) decides what each program may do and restarts servers, `pm` creates every process after boot, is its parent in the kernel and keeps the table, and `init` (id 1, no authority) starts the shell. `pm` holds the right to create processes and the program labels, never a port or device memory; see [pm](documentation/userland/pm.md). `wait`, `exec` and `fork` (copy on write) are requests to `pm`; signals come next.

**Hardening**
- Authenticated reserved messages, per-process memory limits, a reaper thread, a write-protected kernel image, and the FPU disabled for programs.
- `-Werror` in the kernel and userland builds, compile-time filtering of log levels, and fixed-size pools for threads and processes.
- Reuse of kernel address ranges and freeing of the early boot memory.

**File system outside the kernel**
- Shared memory between processes, and data boot modules with an `initrd` built as a tar archive.
- A `ramfs` server, `spawn` from files, and a manifest read by `rs` that lists each program and its capabilities.
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
- A route to Unix style IPC, where the capability plays the part of the file descriptor. Step by step: a stable service label with an IPC capability that decides who may send, call or share memory with whom (done); a name service in Ring 3 that replaces the lookup of the kernel, so permission is checked when a client connects (done); capability transfer in messages, so a process can give another a capability it holds and the receiver gets it only when it accepted that type (done); endpoints and handles, so messages are addressed by handle and not by process id (done: no message, call or capability message reaches another program by its id, and the message has no sender field); capabilities inherited by `fork` (done for `IPC` and endpoint handles, which the child keeps with the numbers they had, and the name server gives the child the rights of its parent) and kept by `exec` (done); and a descriptor layer in the C library, so the shell can join programs with pipes and redirection. The last one comes in four steps: a table of descriptors with `dup` and `dup2` (done); a `pipe` server and `pipe()` (done); inheritance by `fork` and `exec` of the descriptors that live in a server (done: `rs` tells the servers that keep state for each process about every copy, and `pm` holds the parent until they copied); and owner and mode permissions in the name service.
- Page faults handled by a server, so memory policy can live outside the kernel. This is the largest item and needs a decision first, and the round trip of a synchronous IPC call is measured before it starts: pages the kernel handles itself keep their speed, and only the faults a pager serves pay for the message.
- Faults you can provoke from the shell: kill a server with `kill` and watch `rs` start it again, with a test and a page that explain it.
- Users and groups: an identity (`uid`, `gid`) kept by `pm`, owner and mode in `stat` and in `ext2`, permission checks in the file servers, mount options for FAT32, `/etc/passwd` and `login`, and signals checked by owner. The model is to be decided first.
- A network: a PCI bus server, a card driver (`rtl8139` or `e1000`, as QEMU emulates them) and a network stack server with ARP, IP, ICMP, UDP and TCP, all in Ring 3. It needs the DMA policy first.
- A Linux personality, so static Linux binaries can run: calls the kernel does not serve become messages to a server. Last, after the shell, the accounts and the network.
- A process model like Linux's, kept by the process manager `pm` outside the kernel, as in Minix 3 (`pm` exists and keeps the table, see Done; the rest of this item is not started); the kernel keeps only the primitives it must. It covers process states (running, sleeping, stopped, zombie) shown to `ps`; `wait` (a parent collects a child's exit status, and zombies until then), `exec` (a process replaces its program and keeps its pid), signals with `kill`, process groups so Ctrl+C reaches only the foreground job, a process table for `ps`, and `nice`.
- Running programs from the shell with `fork` and `exec`, with the commands moved out of it into programs of `/bin` (the shell keeps `cd`, `pwd`, `exit` and `help`), drivers, servers and applications in separate folders, `argc` and `argv`, a current directory, quoting, variables, redirection and pipes (done up to here), history, and Ctrl+C. Ctrl+C comes with process groups and sessions and with the split of `console` into a driver that writes to the serial port and the screen and a `tty` server that holds the line discipline.
- A `tmpfs`: a writable file server that keeps its files in memory and is mounted on `/tmp`, so the system has a place to write without a disk.
- A consistency check when FAT32 and ext2 volumes are mounted, so a damaged volume is reported instead of read silently. This is a check, not a journal.
- A heap test under random allocation and release, to see whether the free space fragments.
- An out-of-memory test: a program that uses up the physical memory must get an error, and the kernel must keep running.
- A kernel stack overflow reported with a message instead of a silent reset.
- Capabilities as data: the kernel no longer knows the name of any program (done for `rs` and the VGA driver), a test checks that it boots with no user program at all (done), capabilities can be revoked (done in the kernel; `rs` does not use it yet), IPC gets a capability of its own (the first step of the route above, which also decides who may write the disk), a capability may replace the grant authority needed to `spawn`, and DMA-capable ports get a policy.
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
* [System calls](documentation/core/syscalls.md): Every call with its number, purpose, arguments, return value and the authority it needs.
* [Inter-Process Communication](documentation/core/ipc.md): Asynchronous messages and synchronous calls.
* [Capabilities](documentation/core/capabilities.md): Authority over ports, IRQs and physical memory.
* [Kernel concepts](documentation/core/kernel_concepts.md): Every concept the kernel holds, why it cannot live outside, and what is planned to move.
* [Userland](documentation/userland/userland.md): Boot modules, the system call ABI and the drivers.
* [GDT](documentation/arch/gdt.md) & [IDT](documentation/arch/idt.md): CPU descriptor tables and interrupt handling.
* [VGA](documentation/userland/vga.md): Kernel console and the userland display server.
* [Keyboard](documentation/userland/keyboard.md): The PS/2 driver in userland and how an interrupt reaches it.
* [Block storage](documentation/userland/block_storage.md): The ATA driver, partition tables and the block protocol.
* [FAT32](documentation/userland/fat32.md): The FAT32 server and how the `vfs` joins it to the initrd.
* [ext2](documentation/userland/ext2.md): The ext2 server, its feature policy and symbolic links.
