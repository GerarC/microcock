ENTRY(_start)

SECTIONS {
    . = 0x00100000;

    .multiboot ALIGN(4K) : {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))

    }

    .boot ALIGN(4K) : {
        *(.boot*)
    }

    . += 0xC0000000;

    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
        *(.text*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000) {
        *(.rodata*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
        *(.data*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
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
}

