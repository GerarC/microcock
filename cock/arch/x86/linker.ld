ENTRY(_start)

KERNEL_OFFSET = 0xC0000000;
BOOT_MAPPED_SIZE = 0x400000;

SECTIONS {
    . = 0x00100000;

    _kernel_start = .;
    .multiboot ALIGN(4K) : {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))

    }

    .boot ALIGN(4K) : {
        *(.boot*)
    }

    . += KERNEL_OFFSET;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_OFFSET) {
        _text_start = .;
        *(.text*)
        _text_end = .;
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_OFFSET) {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_OFFSET) {
        *(.data*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_OFFSET) {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    .init_array ALIGN(8) : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    .ctors ALIGN(8) : {
        __ctors_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.ctors))
        __ctors_end = .;
    }

    .dtors ALIGN(8) : {
        __dtors_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.dtors))
        __dtors_end = .;
    }


    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
    _kernel_end = .;

    ASSERT(_kernel_end - KERNEL_OFFSET <= BOOT_MAPPED_SIZE,
           "the kernel image does not fit in the memory mapped at boot")
}

