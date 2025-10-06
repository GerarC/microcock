#include <cock/driver/vga.hpp>
#include <string.h>

VGA vga_instance;

VGA::VGA()
	: row(0), column(0),
	  color(vga_entry_color(VGAColor::GREEN, VGAColor::BLACK)),
	  buffer(reinterpret_cast<vchar *>(VGA_ADDRESS)) {};

void VGA::init(){
    row = column = 0;
    color = vga_entry_color(VGAColor::GREEN, VGAColor::BLACK);
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
	if (c == '\n') {
		newline();
		return;
	}
	putEntryAt(c, color, column, row);
	if (++column == VGA_WIDTH) {
		column = 0;
		newline();
	}
}

void VGA::write(const char *data, size_t size) {
	for (size_t i = 0; i < size; i++)
		putChar(data[i]);
}

void VGA::writeString(const char *data) { write(data, strlen(data)); }

void VGA::clear() {
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		for (size_t y = 0; y < VGA_HEIGHT; y++) {
			const size_t index = y * VGA_WIDTH + x;
			buffer[index] = vga_entry(' ', color);
		}
	}
	row = 0;
	column = 0;
}

void VGA::newline() {
	column = 0;
	if (++row == VGA_HEIGHT) {
		scroll();
		row = VGA_HEIGHT - 1;
	}
}

void VGA::scroll() {
	for (size_t y = 1; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			buffer[(y - 1) * VGA_WIDTH + x] = buffer[y * VGA_WIDTH + x];
		}
	}
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
	}
}

vcolor VGA::vga_entry_color(VGAColor fg, VGAColor bg) {
	return static_cast<vcolor>(fg) | (static_cast<vcolor>(bg) << 4);
}

vchar VGA::vga_entry(unsigned char uc, vcolor color) {
	return static_cast<vchar>(uc) | static_cast<vchar>(color) << 8;
}
