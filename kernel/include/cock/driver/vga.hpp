#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t vcolor;
typedef uint16_t vchar;

namespace driver {

enum class VGAColor : vcolor {
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
	WHITE = 15,
};

class VGA {
  public:
	static constexpr size_t VGA_WIDTH = 80;
	static constexpr size_t VGA_HEIGHT = 25;
	static constexpr uintptr_t VGA_ADDRESS = 0xB8000;
	VGA();

	void setColor(VGAColor fg, VGAColor bg);
	void putEntryAt(unsigned char c, vcolor color, size_t x, size_t y);
	void putChar(char c);
	void write(const char *data, size_t size);
	void writeString(const char *data);
	void clear();
	void init();

  private:
	size_t row;
	size_t column;
	vcolor color;
	vchar volatile *buffer;

	void newline();
	void scroll();

	vcolor vga_entry_color(VGAColor fg, VGAColor bg);
	vchar vga_entry(unsigned char uc, vcolor color);
};

extern VGA vga_instance;

} // namespace driver

#endif // !VGA_H
