org 0x7C00 ; this is the sector of the memory that says that we are in bios mode
bits 16; says how many bits we are working with, to emit them

%define ENDL 0x0D, 0x0A

start:
    jmp main


; A function that prints a string in the screen
; Params:
;   ds:si points to the string
puts:
    ; Save registers we will modify
    push si
    push ax
    push bx

.loop:
    lodsb           ; loads next char in al
    or al, al       ;
    jz .done

    mov ah, 0x0E
    mov bh, 0x00
    int 0x10

    jmp .loop

.done:
    pop bx
    pop ax
    pop si
    ret
    

main:

    ; setup data segments
    mov ax, 0
    mov ds, ax
    mov es, ax

    ; setup stact
    mov ss, ax
    mov sp, 0x7c00 ; stack grow downwards so, it never touches the SO code

    mov si, msg_hello
    call puts

    hlt


.halt:              ; in case the processor restarts, it gets stuck in a infinite loop becuase is not good Idea to let the processor run after the code
    jmp .halt


msg_hello: db 'Hello world!', ENDL, 0
times 510-($-$$) db 0 ; ($-$$) says the complete size of the section of the program
dw 0AA55h ; Write in the first position of the memory  
