; Declaration of constants for multiboot header
MBALIGN     equ 1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ 1 << 1              ; provide memory map
MBGFX       equ 0                   ;  Use graphics
MBFLAGS     equ MBALIGN | MEMINFO | MBGFX   ; this is Multiboot 'flag' field
MAGIC       equ 0x1BADB002          ; 'magic number' lets to the bootloader find the header
CHECKSUM    equ -(MAGIC + MBFLAGS)  ; this is the checksum of above

KERNEL_VIRTUAL_BASE     equ 0xC0000000
PAGE_SHIFT              equ 12
PAGE_SIZE               equ 1 << PAGE_SHIFT
TABLE_ENTRIES           equ 1024
ENTRY_SIZE              equ 4
PAGE_PRESENT_WRITABLE   equ 0x3
CR0_PAGING              equ 0x80000000
VGA_PHYSICAL_BASE       equ 0x000B8000
VGA_PAGE_INDEX          equ VGA_PHYSICAL_BASE >> PAGE_SHIFT
DIRECTORY_SHIFT         equ 22
KERNEL_DIRECTORY_INDEX  equ KERNEL_VIRTUAL_BASE >> DIRECTORY_SHIFT
RECURSIVE_DIRECTORY_INDEX equ TABLE_ENTRIES - 1
IDENTITY_DIRECTORY_INDEX  equ 0
STACK_SIZE              equ 16384
STACK_ALIGNMENT         equ 16
MULTIBOOT_ALIGNMENT     equ 4
MULTIBOOT_UNUSED_FIELDS equ 0
MULTIBOOT_LINEAR_MODE   equ 0x0000
MULTIBOOT_WIDTH         equ 0x0320
MULTIBOOT_HEIGHT        equ 0x0258
MULTIBOOT_DEPTH         equ 0x0020
NO_ENTRY                equ 0
FIRST_INDEX             equ 0

; Section of the kernel.
section .boot
global _start:function
_start:
    mov esi, eax    ; save magic
    mov ebp, ebx    ; save boot_info

    ; =========================
    ; Setup page table (identity)
    ; =========================
    mov ecx, FIRST_INDEX
    mov edi, page_table_low
    sub edi, KERNEL_VIRTUAL_BASE

.fill_low:
    mov eax, ecx
    shl eax, PAGE_SHIFT     ; physical = idx * 4096
    or eax, PAGE_PRESENT_WRITABLE     ; present + writable
    mov [edi], eax

    add edi, ENTRY_SIZE
    inc ecx
    cmp ecx, TABLE_ENTRIES
    jne .fill_low

    ; =========================
    ; Setup page table (Higher Half)
    ; =========================
    mov ecx, FIRST_INDEX
    mov edi, page_table_high
    sub edi, KERNEL_VIRTUAL_BASE

.fill_high:
    mov eax, ecx
    shl eax, PAGE_SHIFT
    or eax, PAGE_PRESENT_WRITABLE
    mov [edi], eax

    add edi, ENTRY_SIZE
    inc ecx
    cmp ecx, TABLE_ENTRIES
    jne .fill_high


    ; =========================
    ; Setup VGA on 0xC00B8000
    ; =========================
    mov eax, VGA_PHYSICAL_BASE
    or eax, PAGE_PRESENT_WRITABLE

    mov edi, page_table_high
    sub edi, KERNEL_VIRTUAL_BASE
    add edi, VGA_PAGE_INDEX * ENTRY_SIZE   ; 0xB8000 / 4096

    mov [edi], eax


    ; =========================
    ; Setup page directory
    ; =========================
    mov ebx, page_directory
    sub ebx, KERNEL_VIRTUAL_BASE

    mov eax, page_table_low
    sub eax, KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT_WRITABLE
    mov [ebx + IDENTITY_DIRECTORY_INDEX * ENTRY_SIZE], eax       ; IdentityMapping

    mov eax, page_table_high
    sub eax, KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT_WRITABLE
    mov [ebx + KERNEL_DIRECTORY_INDEX * ENTRY_SIZE], eax   ; 0xC0000000


    mov eax, page_directory
    sub eax, KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT_WRITABLE
    mov [ebx + RECURSIVE_DIRECTORY_INDEX * ENTRY_SIZE], eax

    ; =========================
    ; Load CR3
    ; =========================
    mov eax, page_directory
    sub eax, KERNEL_VIRTUAL_BASE
    mov cr3, eax

    ; =========================
    ; Enable Paging
    ; =========================
    mov eax, cr0
    or eax, CR0_PAGING
    mov cr0, eax


    lea eax, [higher_half]
    jmp eax


section .text
higher_half:
    mov dword [page_directory + IDENTITY_DIRECTORY_INDEX * ENTRY_SIZE], NO_ENTRY

    ;flush TLB
    mov eax, cr3
    mov cr3, eax

    ; set the stack in the top
    mov esp, stack_top

    push ebp
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
align MULTIBOOT_ALIGNMENT
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM
    dd MULTIBOOT_UNUSED_FIELDS, MULTIBOOT_UNUSED_FIELDS, MULTIBOOT_UNUSED_FIELDS, MULTIBOOT_UNUSED_FIELDS, MULTIBOOT_UNUSED_FIELDS

    ;; Graphic vals
    dd MULTIBOOT_LINEAR_MODE
    dd MULTIBOOT_WIDTH   ; 800 width
    dd MULTIBOOT_HEIGHT   ; 600 height
    dd MULTIBOOT_DEPTH   ; 32 depth

; As multiboot standard has no  definition of the stack pointer. The next section is to allocate a small stack
section .bss
align STACK_ALIGNMENT
stack_bottom:
    resb STACK_SIZE ; 16 KiB is reserved for stac KiB is reserved for stack
stack_top:

; Pagination
align PAGE_SIZE
global page_directory
page_directory:
    resd TABLE_ENTRIES
global page_table_low
page_table_low:
    resd TABLE_ENTRIES
global page_table_high
page_table_high:
    resd TABLE_ENTRIES
