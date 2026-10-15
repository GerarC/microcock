# Bootloader

A bootloader is a piece of software in charge of loading a more complex program. **MicroCOCK** uses GRUB to facilitate Kernel development.

## How It Works
The bootloader entry code is located in `cock/arch/x86/boot/entry.s`.

First, it defines Multiboot constants including `MBALIGN`, `MEMINFO`, and `MBGFX`.
```nasm
MBALIGN     equ 1 << 0
MEMINFO     equ 1 << 1
MBGFX       equ 0
MBFLAGS     equ MBALIGN | MEMINFO | MBGFX
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + MBFLAGS)
```

`MBALIGN` asks GRUB to align the modules on page boundaries and `MEMINFO` asks for the memory map, which the physical memory manager relies on.

This section pushes the header into `.multiboot` so GRUB can find it.
```nasm
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
```

As multiboot has no definition of a stack pointer, it reserves a portion of memory for the stack in the `.bss` section (16 KiB).
```nasm
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
```

## Entering the higher half
The code in `.boot` runs at its physical address, before paging exists. It fills two page tables, one that maps the first 4 MiB at address 0 and one that maps them at `0xC0000000`, puts both in the page directory, installs the directory in `CR3`, sets the paging bit in `CR0` and jumps to the higher half. It also maps the VGA text buffer in the kernel half and installs the recursive entry in the last slot of the directory.

The `.text` section then removes the temporary identity mapping, reloads `CR3`, sets the stack and calls the kernel entry point `init_cock` with the Multiboot magic and the physical address of the information structure.
```nasm
section .text
higher_half:
    mov dword [page_directory + 0], 0
    mov eax, cr3
    mov cr3, eax
    mov esp, stack_top

    push ebp
    push esi
    xor ebp, ebp

    extern init_cock
    call init_cock
    cli
hang: hlt
    jmp hang
```

Only the first 4 MiB are mapped when `init_cock` starts. Everything the kernel needs from the bootloader beyond that (the information structure, the memory map, the modules) is reached through the early window described in [memory management](memory_management.md#early-boot-window). The kernel image, with its `.bss`, must fit in those 4 MiB.

## Linker Script
To tell the CPU where to look up the information, a linker script is needed. In this project, this file is placed in `cock/arch/x86/linker.ld`. It offsets the virtual addresses by `0xC0000000` to enforce the Higher-Half design, while the load addresses stay physical.
```ld
ENTRY(_start)
SECTIONS {
    . = 0x00100000;

    _kernel_start = .;
    .multiboot ALIGN(4K) : {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))
    }

    .boot ALIGN(4K) : {
        *(.boot*)
    }

    . += 0xC0000000;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
        *(.text*)
    }
    ...
    _kernel_end = .;
}
```

`_kernel_end` marks where the physical memory manager starts to consider memory free. The `.init_array` and `.ctors` sections are collected here too, and `call_global_constructors` runs whichever the toolchain produced.

## Boot modules
GRUB loads the userland programs as Multiboot modules, with the name of each one as its command line. The Makefile writes the `grub.cfg` and puts `init` first (`USERLAND_BOOT_ORDER`). See [userland](userland.md).
