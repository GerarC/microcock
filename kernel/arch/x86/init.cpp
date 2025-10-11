#include <cock/core/arch.hpp>
#include <cock/driver/vga.hpp>
#include "./gdt/gdt.hpp"
#include "./interrupt/idt/idt.hpp"

namespace cock::arch::x86 {

using cock::driver::vga_instance;

GDT gdt;
IDT idt;

void init_video(){
    vga_instance.init();
    vga_instance.clear();
}

extern "C" void arch_init(){
    init_video();
    gdt.init();
    idt.init();
}
    
}

