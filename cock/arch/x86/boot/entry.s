; Declaration of constants for multiboot header
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide memory map
MBGFX       equ 0                   ;  Use graphics
MBFLAGS     equ MBALIGN | MEMINFO | MBGFX   ; this is Multiboot 'flag' field
MAGIC       equ 0x1BADB002          ; 'magic number' lets to the bootloader find the header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; this is the checksum of above

; Section of the kernel. You have complete use of computer resources
section .boot
global _start:function 
_start:

    ; set kernel in 0xC000000
    mov ecx, initial_page_dir
    sub ecx, 0xC0000000 
    mov cr3, ecx

    mov ecx, cr4
    or ecx, 0x10
    mov cr4, ecx

    mov ecx, cr0
    or ecx, 0x80000000
    mov cr0, ecx

    jmp higher_half

section .text
higher_half:
    ; set the stack in the top
    mov esp, stack_top
    push ebx
    push eax
    xor ebp, ebp

    extern init_cock
    call init_cock

    cli             ; disables interrupts

; If the program has nothing to do then this put kernel into an infinite loop
hang: hlt
    jmp hang

; Declaration of the multiboot header. This marks the whole program as a kernel.
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0

    ;; Graphic vals
    dd 0
    dd 800  ; width
    dd 600  ; height
    dd 32   ; depth

; As multiboot standard has no  definition of the stack pointer. The next section is to allocate a small stack
section .bss
align 16
stack_bottom: 
    resb 16384 ; 16 KiB is reserved for stac KiB is reserved for stack
stack_top:

section .data
align 4096
global initial_page_dir
initial_page_dir:
    DD 10000011b
    TIMES 768-1 DD 0

    DD (0 << 22) | 10000011b
    DD (1 << 22) | 10000011b
    DD (2 << 22) | 10000011b
    DD (3 << 22) | 10000011b
    TIMES 256-4 DD 0

