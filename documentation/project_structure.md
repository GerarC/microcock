# Project Structure

The codebase strictly separates the architecture-dependent code, the kernel core, the standard libraries, and the userland space.

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
