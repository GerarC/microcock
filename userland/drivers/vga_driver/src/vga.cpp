#include <cock/user/api.hpp>
#include <vga_driver/vga.hpp>
#include <sys/mman.h>
#include <string.h>

namespace cock::vga {

Screen VgaServer::screens[TERMINAL_COUNT];
size_t VgaServer::target = 0;
size_t VgaServer::active = 0;
volatile uint16_t* VgaServer::buffer = nullptr;
bool VgaServer::cursorReady = false;

bool VgaServer::init() {
    void* mapped = mmap((void*)VGA_VIRTUAL_TARGET, PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PHYS | MAP_FIXED, -1,
                        VGA_PHYSICAL_ADDRESS);
    if (mapped == MAP_FAILED) {
        buffer = nullptr;
        return false;
    }
    buffer = static_cast<volatile uint16_t*>(mapped);
    for (size_t index = 0; index < CELL_COUNT; index++) screens[0].cells[index] = buffer[index];
    for (size_t terminal = 1; terminal < TERMINAL_COUNT; terminal++)
        for (size_t index = 0; index < CELL_COUNT; index++)
            screens[terminal].cells[index] = vga_entry(CHAR_EMPTY, DEFAULT_ATTRIBUTE);
    return true;
}

bool VgaServer::initCursor() {
    if (!cursorReady) cursorReady = cock::user::request_ports(CRTC_INDEX_PORT, CRTC_PORT_COUNT);
    if (cursorReady) moveCursor();
    return cursorReady;
}

void VgaServer::moveCursor() {
    if (!cursorReady) return;
    const Screen& screen = screens[active];
    uint16_t position = static_cast<uint16_t>(screen.row * VGA_WIDTH + screen.column);
    cock::user::port_out8(CRTC_INDEX_PORT, CURSOR_LOCATION_HIGH);
    cock::user::port_out8(CRTC_DATA_PORT, static_cast<uint8_t>((position >> BYTE_BITS) & BYTE_MASK));
    cock::user::port_out8(CRTC_INDEX_PORT, CURSOR_LOCATION_LOW);
    cock::user::port_out8(CRTC_DATA_PORT, static_cast<uint8_t>(position & BYTE_MASK));
}

void VgaServer::selectTarget(size_t terminal) {
    if (terminal < TERMINAL_COUNT) target = terminal;
}

void VgaServer::show(size_t terminal) {
    if (terminal >= TERMINAL_COUNT) return;
    active = terminal;
    redraw();
    moveCursor();
}

void VgaServer::store(size_t index, uint16_t entry) {
    screens[target].cells[index] = entry;
    if (target == active) buffer[index] = entry;
}

void VgaServer::redraw() {
    for (size_t index = 0; index < CELL_COUNT; index++) buffer[index] = screens[active].cells[index];
}

void VgaServer::setColor(VGAColor fg, VGAColor bg) {
    screens[target].color = static_cast<uint8_t>((static_cast<uint8_t>(bg) << BACKGROUND_SHIFT) | (static_cast<uint8_t>(fg) & FOREGROUND_MASK));
}

void VgaServer::putChar(char c) {
    place(c);
    moveCursor();
}

void VgaServer::place(char c) {
    Screen& screen = screens[target];
    switch (screen.sgr.feed(c)) {
    case cock::abi::ansi::Result::CONSUMED:
        return;
    case cock::abi::ansi::Result::STYLE_CHANGED:
        screen.color = attributeOf(screen.sgr.style());
        return;
    case cock::abi::ansi::Result::PRINT:
        break;
    }
    if (c == CHAR_NEWLINE) {
        newline();
        return;
    }
    if (c == CHAR_BACKSPACE) {
        if (screen.column == 0) return;
        screen.column--;
        store(screen.row * VGA_WIDTH + screen.column, vga_entry(CHAR_EMPTY, screens[target].color));
        return;
    }
    store(screen.row * VGA_WIDTH + screen.column, vga_entry(c, screens[target].color));

    if (++screen.column == VGA_WIDTH) {
        screen.column = 0;
        newline();
    }
}

void VgaServer::write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) place(data[i]);
    moveCursor();
}

void VgaServer::clear() {
    Screen& screen = screens[target];
    for (size_t i = 0; i < CELL_COUNT; i++) store(i, vga_entry(CHAR_EMPTY, screens[target].color));
    screen.row = 0;
    screen.column = 0;
    moveCursor();
}

void VgaServer::newline() {
    Screen& screen = screens[target];
    screen.column = 0;
    if (++screen.row == VGA_HEIGHT) {
        scroll();
        screen.row = VGA_HEIGHT - 1;
    }
}

void VgaServer::scroll() {
    Screen& screen = screens[target];
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        memcpy(&screen.cells[(y - 1) * VGA_WIDTH], &screen.cells[y * VGA_WIDTH], VGA_WIDTH * 2);
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        screen.cells[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(CHAR_EMPTY, screens[target].color);
    }
    if (target == active) redraw();
}

uint8_t VgaServer::attributeOf(const cock::abi::ansi::Style& style) {
    using cock::abi::ansi::DEFAULT_COLOR;
    using cock::abi::ansi::NORMAL_COLOR_COUNT;

    uint8_t foreground = DEFAULT_FG;
    if (style.foreground != DEFAULT_COLOR) {
        bool bright = style.foreground >= NORMAL_COLOR_COUNT || style.bold;
        uint8_t normal = style.foreground % NORMAL_COLOR_COUNT;
        foreground = static_cast<uint8_t>(ANSI_TO_VGA[normal] | (bright ? BRIGHT_BIT : 0));
    }
    uint8_t background = DEFAULT_BG;
    if (style.background != DEFAULT_COLOR) background = ANSI_TO_VGA[style.background % NORMAL_COLOR_COUNT] & BACKGROUND_MASK;
    return static_cast<uint8_t>((background << BACKGROUND_SHIFT) | foreground);
}

uint16_t VgaServer::vga_entry(unsigned char uc, uint8_t color) {
    return static_cast<uint16_t>(uc) | static_cast<uint16_t>(color) << 8;
}

} // namespace cock::vga
