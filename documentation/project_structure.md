# Project Structure

The codebase strictly separates the architecture-dependent code, the kernel core, the standard libraries, and the userland space.

```text
.
├── cock/                           # Kernel Space
│   ├── arch/x86/                   # Architecture-specific implementation
│   │   ├── boot/                   # Entry, Multiboot structures, early boot memory window
│   │   ├── driver/                 # Kernel text console (VGA)
│   │   ├── gdt/                    # GDT and TSS (with the I/O permission bitmap)
│   │   ├── include/hal/            # The HAL headers of this architecture
│   │   ├── interrupt/              # IDT, ISR and IRQ stubs, PIC
│   │   ├── loader/                 # Multiboot module list
│   │   ├── memory/                 # Layout constants, PMM, paging, VMM glue
│   │   ├── syscall/                # Syscall numbers and table
│   │   ├── task/                   # Initial stacks and process context
│   │   ├── timer/                  # PIT
│   │   └── linker.ld
│   ├── core/                       # Architecture-agnostic Core
│   │   ├── boot/                   # BootModule description
│   │   ├── hal/                    # HAL pieces that the core itself defines (interrupt guard, tasking)
│   │   ├── irq/                    # IRQ routing to userland
│   │   ├── loader/                 # ELF32 and ELF64 loader
│   │   ├── memory/                 # PhysAddr/VirtAddr, VMM, heap
│   │   ├── security/               # Capabilities
│   │   ├── syscall/                # One file per system call
│   │   ├── task/                   # Process, Thread, Scheduler, TaskManager, IPC
│   │   ├── cock.cpp                # cock_main
│   │   └── events.cpp              # What the core does when the architecture reports something
│   └── utils/                      # Logger, Panic, Stack Smashing Protection
├── libc/                           # Custom C Standard Library (freestanding)
├── libcxx/                         # Custom C++ ABI and memory management (cxa, new/delete)
├── shared/                         # Headers and data structures shared between Kernel and Userland
│   └── include/cock/
│       ├── abi/                    # Capability types
│       ├── data_structure/         # LinkedList, RingBuffer, IntrusiveQueue
│       └── ipc/                    # IPC Message format
├── userland/                       # User Space Programs (Ring 3)
│   ├── common.mk                   # Shared build rules
│   ├── linker.ld                   # Link address of every program
│   ├── lib/include/cock/user/      # `cock::user` API
│   ├── init/                       # Root server
│   ├── console/                    # Console server (input line, serial and screen output)
│   ├── kbd/                        # Keyboard driver
│   ├── sh/                         # The shell
│   └── vga_driver/                 # Display server
├── test/                           # Every test
│   ├── cock/                       # In-kernel test suites
│   ├── host/                       # Tests of shared/ and CapabilitySet, run on the host
│   ├── initrd/                     # Manifest lines, mounts and files that only the test boot adds to the initrd
│   └── userland/                   # Test programs (probe, hello, aborter, syncbench, contruncated, fuzz, fsclient, fatclient, extclient, blkclient, cfile, mute, muteclient, phoenix, phoenixclient, crasher) and lib/include/cock/user/write_check.hpp
├── documentation/                  # Pages split by layer: core, arch, userland, shared, libs
├── scripts/                        # QEMU based and host test runners
└── Makefile                        # Master build system
```

## The HAL

The core never includes anything from `cock/arch/`. It includes `<hal/...>` headers, and the Makefile adds `-Icock/arch/$(ARCH)/include` to the compiler flags, so the architecture chosen at build time decides which headers answer.

| Header | Provides |
|--------|----------|
| `hal/page.hpp` | `PAGE_SIZE`, `PAGE_SHIFT`, `PAGE_MASK` |
| `hal/address_space.hpp` | Raw address types, the user-space limit of each ELF class, where the kernel half starts, where `mmap` starts |
| `hal/cpu.hpp` | Inline primitives: port I/O, interrupt flag save and restore, `invlpg`, page-table root, `cpuid`, global pages, emulator exit |
| `hal/interrupt.hpp` | Number of IRQ lines and which ones are reserved for the kernel |
| `hal/serial.hpp`, `hal/console.hpp` | Serial port and the kernel text console |
| `hal/process_context.hpp` | The `ArchProcessContext` stored by value in each `Process` |
| `hal/executable.hpp` | Which ELF classes and machine numbers the architecture runs |

Everything in these headers is `constexpr` or `inline`, so a call from the core compiles to the instruction itself. Porting to another architecture means providing the same set of headers, plus the implementations that cannot be inline (`cock/arch/<name>/...`).

The pieces that the core declares and the architecture implements (switching the stack of a new thread, the I/O permission bitmap, the page-table root) live in `cock/core/hal/tasking.hpp`.
