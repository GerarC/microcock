# The HAL

The core never includes anything from `cock/arch/`. It includes `<hal/...>` headers, and the Makefile adds `-Icock/arch/$(ARCH)/include` to the compiler flags, so the architecture chosen at build time decides which headers answer.

## Headers the architecture provides

They live in `cock/arch/<arch>/include/hal/`.

| Header | Provides |
|--------|----------|
| `hal/page.hpp` | `PAGE_SIZE`, `PAGE_SHIFT`, `PAGE_MASK` |
| `hal/address_space.hpp` | Raw address types, the user-space limit of each ELF class, where the kernel half starts, where `mmap` starts |
| `hal/cpu.hpp` | Inline primitives: port I/O, interrupt flag save and restore, `invlpg`, page-table root, `cpuid`, global pages, time stamp counter, emulator exit; and the constants of the platform (`TIMER_IRQ_LINE`, `TIMER_FREQUENCY_HZ`, `SYSCALL_VECTOR`, `SYSCALL_INSTRUCTION_SIZE`, `IO_PORT_COUNT`) |
| `hal/interrupt.hpp` | Number of IRQ lines, which ones are reserved for the kernel, and `irq_mask` / `irq_unmask` |
| `hal/serial.hpp`, `hal/console.hpp` | Serial port and the kernel text console |
| `hal/process_context.hpp` | The `ArchProcessContext` stored by value in each `Process` |
| `hal/executable.hpp` | Which ELF classes and machine numbers the architecture runs |
| `hal/boot.hpp` | Hooks into the architecture specific boot path |

Everything in these headers is `constexpr` or `inline` (except the interrupt and console functions, which the architecture implements), so a call from the core compiles to the instruction itself. Porting to another architecture means providing the same set of headers, plus the implementations that cannot be inline (`cock/arch/<name>/...`).

## Contract the core defines

The pieces that the core declares and the architecture implements live in `cock/core/hal/`:

| Header | Provides |
|--------|----------|
| `tasking.hpp` | Building the stack of a kernel or user thread, cloning a saved context (for [fork](process/fork.md)), choosing the kernel stack for the next entry from Ring 3, changing the saved instruction pointer or first argument, rewinding a system call (see [blocking calls](scheduler/blocking_calls.md)), reading and writing the registers a call returns in, switching the page directory, granting and taking back hardware resources, and applying the process context at a switch (see [arch context](process/arch_context.md)) |
| `interrupt_guard.hpp` | `InterruptGuard`, an RAII object that blocks interrupts for a scope and restores the previous state |
| `utils.hpp` | Pulls in the CPU primitives of the active architecture |

## The other direction

The architecture reaches the core in one place only, the events: a system call, an exception, a timer tick or another IRQ. See [events](events.md). Mechanism lives under `cock/arch`, policy lives in the core.
