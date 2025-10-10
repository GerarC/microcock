#include <cock/core/arch.hpp>
#include <cock/driver/vga.hpp>
#include "./gdt/gdt.hpp"


void init_video(){
    vga_instance.init();
    vga_instance.clear();
}

extern "C" void arch_init(){
    init_video();
    init_gdt();
}
