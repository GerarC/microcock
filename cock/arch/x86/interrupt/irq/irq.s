%define STACK_ALIGNMENT_MASK -16
%define ARGUMENT_PADDING 12
%define KERNEL_DATA_SELECTOR 0x10

section .text

extern irq_handler

%macro irq 2
irq_stub_%1:
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

irq_common_stub:
    pusha
    cld

    mov eax, ds
    push eax
    push dword 0

    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, esp
    and esp, STACK_ALIGNMENT_MASK
    sub esp, ARGUMENT_PADDING
    push ebp

    call irq_handler

    mov esp, eax

    add esp, 4
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    popa
    add esp, 8
    iret

irq 0, 32
irq 1, 33
irq 2, 34
irq 3, 35
irq 4, 36
irq 5, 37
irq 6, 38
irq 7, 39
irq 8, 40
irq 9, 41
irq 10, 42
irq 11, 43
irq 12, 44
irq 13, 45
irq 14, 46
irq 15, 47


section .rodata

%define IRQ_COUNT 16

global irq_stub_table
irq_stub_table:
%assign i 0
%rep IRQ_COUNT
    dd irq_stub_%+i
%assign i i+1
%endrep
