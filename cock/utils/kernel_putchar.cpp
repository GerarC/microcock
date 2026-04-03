#include <cock/core/cock.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/ipc/message.hpp>
#include <stdio.h>
#include <cock/driver/vga.hpp>
#include <cock/driver/serial.hpp>

extern "C" int putchar(int ic) {
    char c = (char)ic;
    
    cock::driver::Serial::putChar(c);
    
    if (cock::driver::vga_instance && cock::driver::vga_instance->isEnabled()) {
        cock::driver::vga_instance->putChar(c);
    }
    
    return ic;
}
