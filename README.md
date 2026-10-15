# MicroCOCK OS

MicroCOCK or _Micro Colombian Open Code Kernel_ is a 32-bit x86 higher-half microkernel operating system developed from scratch in C++ and Assembly. It aims to implement a modular and robust architecture where traditional kernel components (like device drivers) are isolated in user space (Ring 3), communicating with the core kernel via Inter-Process Communication (IPC).

## Key Features

* **Microkernel Architecture:** Minimal kernel space. Drivers (like the VGA console) run as isolated userland processes.
* **Higher-Half Kernel:** The kernel is mapped to `0xC0000000`, leaving the lower memory available for user processes.
* **Memory Management:** * Bitmap-based Physical Memory Manager (PMM).
    * Paging-based Virtual Memory Manager (VMM).
    * Custom Kernel Heap (`kmalloc`, `kfree`) with block coalescing and splitting.
* **Task Management & Scheduling:**
    * Multi-Level Feedback Queue (MLFQ) scheduler with priority boosting.
    * Thread isolation and hardware context switching.
* **Inter-Process Communication (IPC):** Ring-buffer-based asynchronous message passing between processes.
* **Custom LibC & LibCXX:** Freestanding implementation of standard C/C++ libraries, including dynamic memory allocation (`new`/`delete`) and global constructors/destructors support.
* **ELF Loader:** Capable of parsing and loading standard 32-bit Executable and Linkable Format binaries into user space.

## Project Architecture

The codebase is strictly separated to maintain the microkernel philosophy. The Core must not directly execute architecture-specific code, relying instead on the Hardware Abstraction Layer (HAL).

```text
.
├── cock/                   # Kernel Space
│   ├── arch/x86/           # Architecture-specific implementation (Boot, IDT, GDT, Paging)
│   ├── core/               # Architecture-agnostic Core (VMM, Heap, Scheduler, IPC, Syscalls)
│   ├── driver/             # Built-in kernel drivers (Serial COM1)
│   └── utils/              # Kernel utilities (Logger, Panic, Stack Smashing Protection)
├── libc/                   # Custom C Standard Library (freestanding)
├── libcxx/                 # Custom C++ ABI and memory management (cxa, new/delete)
├── shared/                 # Headers and data structures shared between Kernel and Userland
│   └── include/cock/       # LinkedList, RingBuffer, IPC Message formats
├── userland/               # User Space Programs (Ring 3)
│   ├── hello/              # Test assembly program
│   └── vga_driver/         # VGA Server running in userland
└── Makefile                # Master build system
```

### The Syscall Interface
Userland processes communicate with the kernel through software interrupts (`int 0x80`). The current system call table includes:
* `SYS_EXIT` (0x01): Terminate current thread.
* `SYS_READ` (0x03): Read from a file descriptor.
* `SYS_WRITE` (0x04): Write to a file descriptor (routed to Serial or VGA Server).
* `SYS_REQUEST_RESOURCE` (0x07): Request I/O port access (IOPM modification).
* `SYS_GETPID` (0x16): Retrieve current Process ID.
* `SYS_MMAP` (0x5A): Allocate dynamic memory (Anonymous or Physical mapped).
* `SYS_MUNMAP` (0x5B): Free dynamically allocated memory.
* `SYS_SCHED_YIELD` (0x9E): Yield CPU to the next ready thread.
* `SYS_IPC_SEND` (0x160): Send a message to another process.
* `SYS_IPC_RECV` (0x161): Receive a message from the thread's inbox.

## Building and Running

### Prerequisites
To build and run MicroCOCK, you need a standard i686-elf cross-compiler toolchain, NASM, GRUB, and QEMU.
* `i686-elf-gcc` / `i686-elf-g++`
* `i686-elf-ld`
* `nasm`
* `grub-mkrescue` (and `xorriso`)
* `qemu-system-i386`

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

Clean the build directory:
```bash
make clean
```

Run in test mode (compiles with `COCK_TEST_MODE` flag):
```bash
make test
```

### References
To create microcock I've reseach a lot of sites, videos and even AI models to learn ways to implement certain things,
so this is the space where I put those references that really helped me to move this project forward:

##### Sites
- [Higher Half Bare Bones](https://wiki.osdev.org/Higher_Half_x86_Bare_Bones): Initial Paging and Higher half kernel were taken from this page

##### Videos
- [OliveStem OS course](https://www.youtube.com/playlist?list=PL2EF13wm-hWAglI8rRbdsCPq_wRpYvQQy): my man olive basically help me to have a path to follow. Interrupts, Keyboard, Multiboot, Memory management and other things at the beginnig where possible because of him.

## Documentation
For more detailed information regarding specific subsystems, please check the `documentation/` folder:
* [Bootloader](documentation/bootloader.md): Multiboot and early entry process.
* [Kernel Main](documentation/kernel_main.md): Initialization sequence.
* [Memory Management](documentation/memory_management.md): PMM, VMM, and Heap.
* [Task Management](documentation/task_management.md): Scheduler and MLFQ.
* [Inter-Process Communication](documentation/ipc.md): Syscalls and messaging.
* [GDT](documentation/gdt.md) & [IDT](documentation/idt.md): CPU descriptor tables and interrupt handling.
* [VGA](documentation/vga.md): Legacy video driver specifications.
