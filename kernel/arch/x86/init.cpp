#include <cock/core/arch.hpp>
#include <cock/driver/vga.hpp>
#include "./gdt/gdt.hpp"
#include "./interrupt/idt/idt.hpp"
#include "timer.hpp"

namespace cock::arch::x86 {

using cock::driver::vga_instance;

GDT gdt;
IDT idt;
Timer timer;

extern "C" void arch_video_init(){
    static driver::VGA vga;
    vga_instance = & vga;
    vga_instance->init();
    vga_instance->clear();
}

extern "C" void arch_core_init(){
    gdt.init();
    idt.init();
    timer.init();
}
    
}

