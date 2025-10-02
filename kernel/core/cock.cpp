#include <stddef.h>
#include <stdint.h>
#include <kernel/driver/vga.hpp>

#if defined (__linux__)
#error "You are not using a  cross-compiler"
#endif

extern "C" void cock_main(void){
    VGA vga;
    vga.writeString("Welcome to microcock\n");
}
