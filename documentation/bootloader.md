# Bootloader

A bootloader is a piece of software in charge of loading a more complex program. **MicroCOCK** uses GRUB to facilitate Kernel development.

## How It Works
The bootloader entry code is located in `cock/arch/x86/boot/entry.s`.

First, it defines Multiboot constants including `MBALIGN`, `MEMINFO`, and `MBGFX`.
```nasm
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide memory map
MBGFX       equ 0                   ; Use graphics
MBFLAGS     equ MBALIGN | MEMINFO | MBGFX   ; this is Multiboot 'flag' field
MAGIC       equ 0x1BADB002          ; 'magic number' lets to the bootloader find the header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; this is the checksum of above
```

This section pushes the header into `.multiboot` so GRUB can find it.
```nasm
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
```

As multiboot has no definition of a stack pointer, it reserves a portion of memory for the stack in the `.bss` section.
```nasm
section .bss
align 16
stack_bottom: 
    resb 16384 ; 16 KiB is reserved for stack
stack_top:
```

The `.text` section sets up paging, mounts the Higher-Half offset, and calls the kernel entry point `init_cock`.
```nasm
section .text
higher_half:
    mov dword [page_directory + 0], 0
    mov eax, cr3
    mov cr3, eax
    mov esp, stack_top
    
    ; ... pushes arguments ...
    extern init_cock
    call init_cock
    cli             ; disables interrupts
hang: hlt
    jmp hang
```

## Linker Script
To tell the CPU where to look up the information, a linker script is needed. In this project, this file is placed in `cock/arch/x86/linker.ld`. It offsets the virtual addresses by `0xC0000000` to enforce the Higher-Half design.
```ld
ENTRY(_start)
SECTIONS {
    . = 0x00100000;

    _kernel_start = .;
    .multiboot ALIGN(4K) : {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))
    }

    . += 0xC0000000;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
        *(.text*)
    }
    /* ... rodata, data, bss ... */
}
```
