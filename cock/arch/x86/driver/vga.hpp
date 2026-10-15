#ifndef ARCH_X86_DRIVER_VGA_HPP
#define ARCH_X86_DRIVER_VGA_HPP

#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

typedef uint8_t vcolor;
typedef uint16_t vchar;

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

class VGA final {
  public:
	static constexpr size_t WIDTH = 80;
	static constexpr size_t HEIGHT = 25;
	static constexpr uintptr_t PHYSICAL_BUFFER = 0xB8000;
	static constexpr uintptr_t BUFFER =
		core::hal::KERNEL_SPACE_BASE + PHYSICAL_BUFFER;

	constexpr VGA()
		: row(0), column(0), color(entryColor(VGAColor::GREEN, VGAColor::BLACK)) {}

	void setColor(VGAColor fg, VGAColor bg);
	void putChar(char c);
	void write(const char *data, size_t size);
	void clear();

  private:
	static constexpr size_t FIRST_ROW = 0;
	static constexpr size_t FIRST_COLUMN = 0;
	static constexpr unsigned COLOR_BACKGROUND_SHIFT = 4;
	static constexpr unsigned CHARACTER_COLOR_SHIFT = 8;
	static constexpr char NEWLINE = '\n';
	static constexpr char EMPTY = ' ';

	size_t row;
	size_t column;
	vcolor color;

	static constexpr vcolor entryColor(VGAColor fg, VGAColor bg) {
		return static_cast<vcolor>(fg) |
			   (static_cast<vcolor>(bg) << COLOR_BACKGROUND_SHIFT);
	}

	static constexpr vchar entry(unsigned char uc, vcolor color) {
		return static_cast<vchar>(uc) |
			   static_cast<vchar>(static_cast<vchar>(color)
								  << CHARACTER_COLOR_SHIFT);
	}

	static volatile vchar *buffer() {
		return reinterpret_cast<volatile vchar *>(BUFFER);
	}

	void newline();
	void scroll();
};

} // namespace cock::arch::x86

#endif // !ARCH_X86_DRIVER_VGA_HPP
