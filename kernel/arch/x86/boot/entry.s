; Declaration of constants for multiboot header
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide memory map
MBFLAGS     equ MBALIGN | MEMINFO   ; this is Multiboot 'flag' field
MAGIC       equ 0x1BADB002          ; 'magic number' lets to the bootloader find the header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; this is the checksum of above

; Declaration of the multiboot header. This marks the whole program as a kernel.
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

; As multiboot standard has no  definition of the stack pointer. The next section is to allocate a small stack
section .bss
align 16
stack_bottom: 
    resb 16384 ; 16 KiB is reserved for stac KiB is reserved for stack
stack_top:


; Section of the kernel. You have complete use of computer resources
section .text
global _start:function (_start.end - _start)
_start:
    ; set the stack in the top
    mov esp, stack_top
    extern cock_main
    call cock_main

    cli             ; disables interrupts

; If the program has nothing to do then this put kernel into an infinite loop
.hang: hlt
    jmp .hang
.end:
