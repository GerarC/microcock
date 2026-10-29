ENTRY(_start)

PF_X = 1;
PF_W = 2;
PF_R = 4;

PHDRS {
    code PT_LOAD FLAGS(PF_R | PF_X);
    data PT_LOAD FLAGS(PF_R | PF_W);
}

SECTIONS {
    . = 0x400000;
    .text : { *(.text*) } :code
    .rodata : { *(.rodata*) } :code
    . = ALIGN(4K);
    .data : { *(.data*) } :data
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array))
        __init_array_end = .;
    } :data
    .ctors : {
        __ctors_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.ctors.*) .ctors))
        __ctors_end = .;
    } :data
    .bss  : { *(.bss*) *(COMMON) } :data
}
