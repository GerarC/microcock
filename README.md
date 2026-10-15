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
* **Capabilities:** I/O ports, IRQ lines and physical memory are only reachable through explicit grants. See [capabilities](documentation/capabilities.md).
* **Interrupts to userland:** a driver binds an IRQ line, the kernel masks it and raises a pending notification on the driver thread (never lost, delivered by `SYS_IPC_RECV`), and the driver acknowledges it.
* **Dead threads are reaped by a kernel thread** that sleeps when there is nothing to free and wakes when a thread dies.
* **The FPU is off for programs:** `CR0.EM` is set, so floating point, MMX and SSE fault instead of sharing state between processes.
* **Custom LibC & LibCXX:** Freestanding implementation of standard C/C++ libraries, including dynamic memory allocation (`new`/`delete`) and global constructors/destructors support.
* **ELF Loader:** Loads ELF32 and ELF64 executables through two separate descriptions. Each class has its own user-space limit. The x86 build accepts ELF32 only.

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
│   ├── vga_driver/         # VGA Server running in userland
│   ├── kbd/                # PS/2 keyboard driver running in userland
│   ├── probe/              # Security probe: checks that the kernel denies what it must
│   └── hello/              # Test assembly program
├── test/                   # In-kernel test suite
├── host_test/              # Tests of the shared structures, built and run on the host
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
* `SYS_CAP_GRANT` (0x170): Delegate part of a capability to another process.
* `SYS_IRQ_BIND` (0x171): Route an interrupt line to the calling process. Requires an `IRQ` capability.
* `SYS_IRQ_ACK` (0x172): Re-enable an interrupt line after handling it.
* `SYS_PROC_LOOKUP` (0x173): Find the process id of a boot module by name.
* `SYS_SHM_CREATE` (0x174): Create a shared memory object of a number of pages, paid by the caller.
* `SYS_SHM_SHARE` (0x175): Let another process map an object, read only or read and write.
* `SYS_SHM_MAP` (0x176): Map a whole object into the caller, read only or read and write.
* `SYS_SHM_RELEASE` (0x177): Drop the right of the caller to an object.
* `SYS_MODULE_OPEN` (0x178): Open a data boot module by name as a read only shared memory object; needs a `BOOT_MODULE` capability.
* `SYS_MODULE_INDEX` (0x179): Index of a data boot module by name; needs grant authority.

## Building and Running

### Prerequisites
To build and run MicroCOCK, you need a standard i686-elf cross-compiler toolchain, NASM, GRUB, and QEMU.
* `i686-elf-gcc` / `i686-elf-g++`
* `i686-elf-ld`
* `nasm`
* `grub-mkrescue` (and `xorriso`, `mtools`)
* `qemu-system-i386`
* `python3` (only for the userland end to end test)

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
`IntrusiveQueue`, `LinkedList`, the message layout) and `CapabilitySet` with the
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

The same checks run on every push through `.github/workflows/ci.yml`.

### Current limitations

* One thread per user process, and no syscall to create processes or threads at run time: every process comes from a Multiboot module.
* Capabilities are assigned at boot (the module named `init` holds the authority) and cannot be revoked; each process holds up to 16.
* IPC is asynchronous with a small inbox per thread; there are no timeouts and no synchronous call and reply.
* The user C library is minimal: it has buffered `printf`, `malloc` and `operator new` (no locking), and no file calls yet.
* `SYS_WRITE` is a debug channel owned by the kernel; it goes away once there is a console server.
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

The path the project follows, in order. `[x]` is done, `[ ]` is planned.

### Done
- [x] Boot, memory management (PMM, paging, VMM, heap), scheduler, IPC, ELF32 and ELF64 loader.
- [x] Zero-cost HAL, `PhysAddr` and `VirtAddr`, processes with their own context, lazy I/O permission bitmap.
- [x] Drivers in Ring 3: VGA server and PS/2 keyboard, with capabilities and interrupts delivered as notifications.
- [x] Hardening: authenticated reserved messages, per-process memory limits, reaper thread, write-protected kernel image, FPU disabled for programs.
- [x] QEMU based tests and CI.

### Next
- [x] Tests for the physical memory manager, the scheduler, IPC and the reaper, and host-side tests for the shared data structures.
- [x] **C library basics for programs:** `printf` with buffered output, and `malloc`, `calloc`, `realloc`, `free` and `operator new`.
- [x] A separate stack for double fault and NMI.
- [x] A generation counter in process ids.

### File system
The file system lives outside the kernel, as servers that talk by IPC. The kernel only gains small general mechanisms. The first one is a read-only initial RAM disk; a disk comes after it, on the same protocol.
- [x] **F1** Shared memory between processes (the way to move file data).
- [x] **F2** Data boot modules, a capability to open them, and an `initrd` built as a tar archive.
- [x] **F3** `ramfs` server and a client library: a program reads a file from the initrd.
- [ ] **F4** `spawn`: `init` starts the drivers from files instead of from boot modules.
- [ ] **F5** A manifest read by `init` that lists each program and its capabilities. Adding a driver becomes adding a line.
- [ ] **F6** ATA driver (PIO), a block protocol and partition tables (MBR and GPT).
- [ ] **F7** FAT32 server (read only first) and a `vfs` with a mount table.
- [ ] **F8** `ext` server, ext2 read only.
- [ ] **F9** Writing for FAT32 and ext2.
- [ ] **F10** `open`, `read`, `write` and `close` in the C library, over the file client.

### After that
- [ ] Capabilities as data: the boot policy comes from the manifest, plus revocation, a capability for IPC and a policy for DMA-capable ports.
- [ ] Timers and `sleep`, threads, synchronous IPC with a register fast path, and a console server that replaces `SYS_WRITE`.
- [ ] A syscall fuzzer that runs in Ring 3 and fails on any kernel panic.
- [ ] Smaller debts: reuse of kernel address ranges, pools for threads and processes, compile-time log filtering, `-Werror`.
- [ ] Platform: APIC and IOAPIC, PAE with NX and memory above 4 GiB, SMP, an x86_64 port.

## Documentation
For more detailed information regarding specific subsystems, please check the `documentation/` folder:
* [Project Structure](documentation/project_structure.md): Directory layout and the HAL.
* [Bootloader](documentation/bootloader.md): Multiboot and early entry process.
* [Kernel Main](documentation/kernel_main.md): Initialization sequence.
* [Memory Management](documentation/memory_management.md): PMM, paging, VMM, and Heap.
* [Task Management](documentation/task_management.md): Processes, threads, scheduler and MLFQ.
* [Inter-Process Communication](documentation/ipc.md): Syscalls and messaging.
* [Capabilities](documentation/capabilities.md): Authority over ports, IRQs and physical memory.
* [Userland](documentation/userland.md): Boot modules, the system call ABI and the drivers.
* [GDT](documentation/gdt.md) & [IDT](documentation/idt.md): CPU descriptor tables and interrupt handling.
* [VGA](documentation/vga.md): Kernel console and the userland display server.
* [Keyboard](documentation/keyboard.md): The PS/2 driver in userland and how an interrupt reaches it.
