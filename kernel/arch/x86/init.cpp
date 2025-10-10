#include <cock/core/arch.hpp>
#include <cock/driver/vga.hpp>
#include "./gdt/gdt.hpp"
#include "./interrupt/idt/idt.hpp"

using namespace arch::x86;

GDT gdt;
IDT idt;

void init_video(){
    driver::vga_instance.init();
    driver::vga_instance.clear();
}

extern "C" void arch_init(){
    init_video();
    gdt.init();
    idt.init();
}
