#ifndef VGA_SERVER_HPP
#define VGA_SERVER_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::vga {

static constexpr uintptr_t VGA_PHYSICAL_ADDRESS = 0xB8000;
static constexpr uintptr_t VGA_VIRTUAL_TARGET   = 0xA0000000; 
static constexpr size_t    VGA_WIDTH            = 80; 
static constexpr size_t    VGA_HEIGHT           = 25;
static constexpr size_t    PAGE_SIZE            = 4096;

static constexpr char      CHAR_EMPTY           = ' ';
static constexpr char      CHAR_NEWLINE         = '\n';
static constexpr uint8_t   DEFAULT_FG           = 9; 
static constexpr uint8_t   DEFAULT_BG           = 0; 

enum class VGAColor : uint8_t {
	BLACK = 0,
	BLUE = 1,
	GREEN = 2,
	CYAN = 3,
	RED = 4,
	MAGENTA = 5,
	BROWN = 6,
	LIGHT_GRAY = 7,
	DARK_GRAY = 8,
	LIGHT_BLUE = 9,
	LIGHT_GREEN = 10,
	LIGHT_CYAN = 11,
	LIGHT_RED = 12,
	LIGHT_MAGENTA = 13,
	LIGHT_BROWN = 14,
	WHITE = 15
};

class VgaServer {
public:
    static void init();
    static void putChar(char c);
    static void write(const char* data, size_t size);
    static void clear();
    static void setColor(VGAColor fg, VGAColor bg);

private:
    static void newline();
    static void scroll();
    static uint16_t vga_entry(unsigned char uc, uint8_t color);

    static size_t row;
    static size_t column;
    static uint8_t current_color;
    static volatile uint16_t* buffer;
};

} // namespace cock::vga

#endif
