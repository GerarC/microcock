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
│   ├── drivers/                    # Programs that hold hardware capabilities (installed under `/drivers`)
│   │   ├── ata/                    # Block driver
│   │   ├── console/                # Console driver (serial and screen output); the input line is in servers/tty
│   │   ├── kbd/                    # Keyboard driver
│   │   ├── rtc/                    # Clock driver
│   │   └── vga_driver/             # Display server
│   ├── servers/                    # Servers that hold policy and no hardware (installed under `/servers`)
│   │   ├── rs/                     # Root server (reincarnation server): grants, starts servers through `pm`, restarts them
│   │   ├── pm/                     # Process manager: creates processes, parent in the kernel, table
│   │   ├── init/                   # First process (id 1) without authority: starts the shell
│   │   ├── names/                  # Name server (connect by name, `ipc=` policy)
│   │   ├── tty/                    # Line discipline of the terminals (echo, history, readers that wait)
│   │   └── vfs/, ramfs/, fat/, ext/, pipe/
│   └── apps/                       # Programs a person may run (installed under `/bin`)
│       ├── sh/                     # The shell
│       └── ls/, cat/, ps/, echo/, ...  # One program for each command
├── initrd/                         # Files packed into the initial ramdisk: `etc/manifest.conf` and `etc/mounts.conf`
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

The core never includes anything from `cock/arch/`: it includes `<hal/...>` headers, and the architecture chosen at build time decides which ones answer. The headers and the contract are described in [the HAL](core/hal.md).
