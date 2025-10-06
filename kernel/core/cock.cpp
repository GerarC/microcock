#include <cock/driver/vga.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

extern "C" void cock_main(void) {
	vga_instance.init();
	puts("Welcome to microcock");
    puts("SEIC");
    puts("Semillero de Linux");
    while (1) {
	    puts("Welcome to microcock");
        for(int i = 0; i < 1000000000; i++);
    }
}
