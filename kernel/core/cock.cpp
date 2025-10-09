#include <cock/driver/vga.hpp>
#include <cock/core/gdt.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

#define VERSION 0

extern "C" void cock_main(void) {
    init_gdt();
	vga_instance.init();
    puts("GDT is initialized");;
    printf("Welcome to microcock V%d\n", VERSION);
    puts("Semillero de Linux");
    puts("SEIC");
}

// TODO: Create initialized header
