# ELF loader

`cock/core/loader` loads an ELF executable into a user address space. It exists only for the Multiboot boot modules (`rs`, `names`, `ramfs`, `vga_driver`), which start before any program that could load them. No system call reaches it: a program started at run time is loaded in Ring 3 by `cock::user::spawn` through the `SYS_PROC_*` calls (see [spawn](process/spawn.md)). Its place in the list of kernel concepts is in [kernel concepts](kernel_concepts.md).

## Interface

| Function | What it does |
|----------|--------------|
| `has_elf_magic(image, size)` | True when the buffer is large enough and starts with the ELF magic. `cock_main` uses it to tell a program module from a data module (see [boot modules](memory/boot_modules.md)) |
| `load_executable(image, size, address_space, out)` | Validates the image and maps its loadable segments into the address space whose page directory root is `address_space`. On success `out` has the entry point and the user space limit of the ELF class; on failure `out` is zero |

The image is untrusted. The loader never reads outside it: every offset and size is checked, without integer overflow, before it is used.

## ELF classes

`load_executable` looks at the class byte of the header and loads the image with `Elf32` or `Elf64` (`elf32.hpp`, `elf64.hpp`), two traits types over one template (`detail::load_image` in `elf_loader.hpp`). Each class has its machine number, its header layouts and its user space limit. An architecture runs only the classes that `hal/executable.hpp` says it supports (see [the HAL](hal.md)); on i386 a 64 bit image is refused with a message, and the 64 bit code costs nothing because the unsupported branch is removed with `if constexpr`. The byte order of the architecture must match the compiler's, which a `static_assert` checks.

## What is validated

Before anything is mapped:

* **Header:** class, byte order, type (executable only), machine, size of a program header entry, at least one program header, and the whole table inside the image.
* **Every loadable segment:** the size in the file does not exceed the size in memory, the contents lie inside the image, the segment starts above the first page and ends below the image limit (the user space limit minus the user stack), all without overflow. An empty segment is valid.
* **Entry point:** it must fall inside a loadable segment.

Program headers are copied out of the image one at a time, so the alignment of the image never matters.

## Loading

With interrupts blocked, the loader switches to the target address space and, for each loadable segment:

1. maps its pages writable (`VirtualMemoryManager::mapPagesAt`, which zeroes them; see [vmm](memory/vmm.md)) with the user limit of the class,
2. copies the bytes that are in the file; the rest of the pages stays zero (the `.bss`).

Then `seal_read_only_segments` makes the pages of the read only segments read only again, except a page that a writable segment also touches. The kernel obeys the write bit of user pages (see [paging](memory/paging.md#protection-of-the-kernel-image)), which is why a segment is copied first and sealed afterwards. The previous address space is restored before the function returns.

## Failure

A failure in validation changes nothing. A failure while mapping leaves the segments already mapped in the address space and returns false: the caller destroys the address space. Every failure writes a line at error level with the reason (see [logger](diagnostics/logger.md)).
