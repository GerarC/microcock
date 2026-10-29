#include <cock/arch/x86/driver/vga.hpp>

namespace cock::arch::x86 {

void VGA::setColor(VGAColor fg, VGAColor bg) { color = entryColor(fg, bg); }

void VGA::putChar(char c) {
	if (c == NEWLINE) {
		newline();
		return;
	}
	buffer()[row * WIDTH + column] = entry(static_cast<unsigned char>(c), color);
	if (++column == WIDTH) newline();
}

void VGA::write(const char *data, size_t size) {
	for (size_t i = 0; i < size; i++)
		putChar(data[i]);
}

void VGA::clear() {
	for (size_t y = 0; y < HEIGHT; y++) {
		for (size_t x = 0; x < WIDTH; x++)
			buffer()[y * WIDTH + x] = entry(EMPTY, color);
	}
	row = FIRST_ROW;
	column = FIRST_COLUMN;
}

void VGA::newline() {
	column = FIRST_COLUMN;
	if (++row == HEIGHT) {
		scroll();
		row = HEIGHT - 1;
	}
}

void VGA::scroll() {
	for (size_t y = 1; y < HEIGHT; y++) {
		for (size_t x = 0; x < WIDTH; x++)
			buffer()[(y - 1) * WIDTH + x] = buffer()[y * WIDTH + x];
	}
	for (size_t x = 0; x < WIDTH; x++)
		buffer()[(HEIGHT - 1) * WIDTH + x] = entry(EMPTY, color);
}

} // namespace cock::arch::x86
