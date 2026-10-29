SYS_EXIT equ 1
SYS_WRITE equ 4
STDOUT equ 1
EXIT_SUCCESS equ 0

section .data
    msg db "Hello from cock ring 3!!!", 10
    msg_len equ $ - msg

section .text
global _start

_start:
    mov eax, SYS_WRITE
    mov ebx, STDOUT
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, EXIT_SUCCESS
    int 0x80
