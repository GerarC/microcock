section .data
    msg db "Hello from cock ring 3!!!", 10
    msg_len equ $ - msg

section .text
global _start

_start:
    mov eax, 7
    mov ebx, 0x60
    mov ecx, 1
    int 0x80

    in al, 0x60

    mov eax, 4
    mov ebx, 1       
    mov ecx, msg     
    mov edx, msg_len 
    int 0x80

    mov eax, 1
    mov ebx, 0 
    int 0x80
