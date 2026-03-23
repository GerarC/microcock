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
    mov esi, eax    ; save magic
    mov edi, ebx    ; save boot_info

    ; =========================
    ; Setup page table (identity)
    ; =========================
    mov ecx, 0
    mov edi, page_table_low
    sub edi, 0xC0000000

.fill_low:
    mov eax, ecx
    shl eax, 12     ; physical = idx * 4096
    or eax, 0x3     ; present + writable
    mov [edi], eax

    add edi, 4
    inc ecx
    cmp ecx, 1024
    jne .fill_low

    ; =========================
    ; Setup page table (Higher Half)
    ; =========================
    mov ecx, 0
    mov edi, page_table_high
    sub edi, 0xC0000000

.fill_high:
    mov eax, ecx
    shl eax, 12
    or eax, 0x3
    mov [edi], eax

    add edi, 4
    inc ecx
    cmp ecx, 1024
    jne .fill_high


    ; =========================
    ; Setup VGA on 0xC00B8000
    ; =========================
    mov eax, 0x000B8000
    or eax, 0x3

    mov edi, page_table_high
    sub edi, 0xC0000000
    add edi, 184 * 4   ; 0xB8000 / 4096

    mov [edi], eax

    
    ; =========================
    ; Setup page directory
    ; =========================
    mov ebx, page_directory
    sub ebx, 0xC0000000

    mov eax, page_table_low
    sub eax, 0xC0000000
    or eax, 0x3
    mov [ebx + 0], eax       ; IdentityMapping

    mov eax, page_table_high
    sub eax, 0xC0000000
    or eax, 0x3
    mov [ebx + 768*4], eax   ; 0xC0000000

    ; =========================
    ; Load CR3
    ; =========================
    mov eax, page_directory
    sub eax, 0xC0000000
    mov cr3, eax

    ; =========================
    ; Enable Paging
    ; =========================
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax


    lea eax, [higher_half]
    jmp eax


section .text
higher_half:
    mov dword [page_directory + 0], 0

    ;flush TLB
    mov eax, cr3
    mov cr3, eax

    ; set the stack in the top
    mov esp, stack_top

    add edi, 0xC0000000
    push edi    ; boot_info
    push esi    ; magic

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

; Pagination
align 4096
global page_directory
page_directory:
    resd 1024
global page_table_low
page_table_low:
    resd 1024
global page_table_high
page_table_high:
    resd 1024
