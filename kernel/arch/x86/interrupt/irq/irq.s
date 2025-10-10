section .text

extern irq_handler

%macro irq 2
irq_stub_%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

irq_common_stub:
    pusha
    mov eax, ds 
    push eax
    mov eax, cr2
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp

    call irq_handler

    add esp, 8 
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    popa
    add esp, 8
    sti 
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
