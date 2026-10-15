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
    .bss  : { *(.bss*) *(COMMON) } :data
}
