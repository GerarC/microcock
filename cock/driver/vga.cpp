#include <cock/driver/vga.hpp>
#include <string.h>

namespace cock::driver {

constexpr VGAColor DEFAULT_BG_COLOR = VGAColor::BLACK;
constexpr VGAColor DEFAULT_FG_COLOR = VGAColor::GREEN;
constexpr char INITIAL_ROW = 0;
constexpr char INITIAL_COL = 0;
constexpr char NEWLINE = '\n';
constexpr char EMPTY = ' ';
constexpr size_t STEP = 1;

VGA* vga_instance = nullptr;

VGA::VGA()
	: row(INITIAL_ROW), column(INITIAL_COL),
	  color(vga_entry_color(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR)),
	  buffer(reinterpret_cast<vchar *>(VGA_ADDRESS)) {}

void VGA::init() {
	row = INITIAL_ROW;
	column = INITIAL_COL;
	color = vga_entry_color(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR);
	buffer = reinterpret_cast<vchar *>(VGA_ADDRESS);
}

void VGA::setColor(VGAColor fg, VGAColor bg) {
	color = vga_entry_color(fg, bg);
}

void VGA::putEntryAt(unsigned char c, vcolor color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	buffer[index] = vga_entry(c, color);
}

void VGA::putChar(char c) {
	if (c == NEWLINE) {
		newline();
		return;
	}
	putEntryAt(c, color, column, row);
	if (++column == VGA_WIDTH) {
		column = INITIAL_COL;
		newline();
	}
}

void VGA::write(const char *data, size_t size) {
	for (size_t i = 0; i < size; i++)
		putChar(data[i]);
}

void VGA::writeString(const char *data) { write(data, strlen(data)); }

void VGA::clear() {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			buffer[index] = vga_entry(EMPTY, color);
		}
	}
	row = INITIAL_ROW;
	column = INITIAL_COL;
}

void VGA::newline() {
	column = INITIAL_COL;
	if (++row == VGA_HEIGHT) {
		scroll();
		row = VGA_HEIGHT - STEP;
	}
}

void VGA::scroll() {
	for (size_t y = 1; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			buffer[(y - STEP) * VGA_WIDTH + x] = buffer[y * VGA_WIDTH + x];
		}
	}
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		buffer[(VGA_HEIGHT - STEP) * VGA_WIDTH + x] = vga_entry(EMPTY, color);
	}
}

constexpr vcolor VGA::vga_entry_color(VGAColor fg, VGAColor bg) {
	return static_cast<vcolor>(fg) | (static_cast<vcolor>(bg) << 4);
}

constexpr vchar VGA::vga_entry(unsigned char uc, vcolor color) {
	return static_cast<vchar>(uc) | static_cast<vchar>(color) << 8;
}

} // namespace cock::driver
