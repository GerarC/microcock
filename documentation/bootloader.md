# Bootloaader

A bootloader is a piece of software in charge of load a more complex program. Creating a new one is an advance subject. **MicroCOCK** uses *GRUB* to facilitate the Kernel developing.

## How It Works?
In this case, bootloader file can be found in *kernel/arch/x86/boot/entry.s*. Let's talk about each piece of code of that file.

The first code in *entry.s* does is to declare the needed information and constants.
~~~ nasm
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide memory map
MBFLAGS     equ MBALIGN | MEMINFO   ; this is Multiboot 'flag' field
MAGIC       equ 0x1BADB002          ; 'magic number' allows to the bootloader find the header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; this checksum validates the above information
~~~
<br/>


This section push into `.multiboot` the header with the information of the constants.
~~~ nasm
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
~~~
<br/>

As multiboot has no definition of stack pointer then is needed to reservate a portion of memory for the stack in `.bss`.
~~~ nasm
section .bss
align 16
stack_bottom: 
    resb 16384 
stack_top:
~~~
<br/>


`.text` section has the code that serves as entry point for the kernel, sets up the stack, import the kernel funciton and call it. If something fails then it get stuck into an infinite loop.
~~~ nasm
section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top
    extern main_cock ; Here it imports the entry function for the kernel
    call main_cock

    cli
.hang: hlt
    jmp .hang
.end:
~~~
<br/>


To tell CPU where to look up for the information a linker script is needed. In this project this file is placed in *kernel/arch/x86/linker.ld*
~~~ ld
ENTRY(_start)
SECTIONS {
    . = 1M;
    .text ALIGN(4K) : {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))
        *(.text*)
    }
    .rodata ALIGN(4K) : {
        *(.rodata*)
    }
    .data ALIGN(4K) : {
        *(.data*)
    }
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
~~~
This file say which is the entry point, the complete size of the kernel, the size, the alignment and the position of each section, and even pointers to the data.
