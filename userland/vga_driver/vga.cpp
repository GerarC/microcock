#include "vga.hpp"
#include <sys/mman.h>
#include <string.h>

namespace cock::vga {

size_t VgaServer::row = 0;
size_t VgaServer::column = 0;
uint8_t VgaServer::current_color = (DEFAULT_BG << 4) | (DEFAULT_FG & 0x0F);
volatile uint16_t* VgaServer::buffer = nullptr;

void VgaServer::init() {
    buffer = (uint16_t*)mmap((void*)VGA_VIRTUAL_TARGET, PAGE_SIZE, 
                             PROT_READ | PROT_WRITE, 
                             MAP_PHYS | MAP_FIXED, -1, 
                             VGA_PHYSICAL_ADDRESS);
    // clear();
}

void VgaServer::setColor(VGAColor fg, VGAColor bg) {
    current_color = (static_cast<uint8_t>(bg) << 4) | (static_cast<uint8_t>(fg) & 0x0F);
}

void VgaServer::putChar(char c) {
    if (c == CHAR_NEWLINE) {
        newline();
        return;
    }
    const size_t index = row * VGA_WIDTH + column;
    buffer[index] = vga_entry(c, current_color);

    if (++column == VGA_WIDTH) {
        column = 0;
        newline();
    }
}

void VgaServer::write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) putChar(data[i]);
}

void VgaServer::clear() {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        buffer[i] = vga_entry(CHAR_EMPTY, current_color);
    }
    row = 0;
    column = 0;
}

void VgaServer::newline() {
    column = 0;
    if (++row == VGA_HEIGHT) {
        scroll();
        row = VGA_HEIGHT - 1;
    }
}

void VgaServer::scroll() {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        memcpy((void*)&buffer[(y - 1) * VGA_WIDTH], 
               (void*)&buffer[y * VGA_WIDTH], VGA_WIDTH * 2);
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(CHAR_EMPTY, current_color);
    }
}

uint16_t VgaServer::vga_entry(unsigned char uc, uint8_t color) {
    return static_cast<uint16_t>(uc) | static_cast<uint16_t>(color) << 8;
}

} // namespace cock::vga
