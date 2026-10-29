#include <cock/user/stream.hpp>
#include <hexdump/row.hpp>

using namespace cock::user::tool;

namespace cock::hexdump {

namespace {

constexpr char DIGITS[] = "0123456789abcdef";
constexpr uint32_t NIBBLE_BITS = 4;
constexpr uint32_t NIBBLE_MASK = 0xF;
constexpr size_t BYTE_DIGITS = 2;
constexpr char SPACE = ' ';
constexpr char BAR = '|';
constexpr char NEWLINE = '\n';
constexpr char UNPRINTABLE = '.';
constexpr uint8_t PRINTABLE_FIRST = 0x20;
constexpr uint8_t PRINTABLE_LAST = 0x7E;
constexpr size_t HEX_GAP = 2;
constexpr size_t HEX_CELL = BYTE_DIGITS + 1;
constexpr size_t BAR_COUNT = 2;
constexpr size_t LINE_BYTES = OFFSET_DIGITS + HEX_GAP + ROW_BYTES * HEX_CELL + 1 + BAR_COUNT + ROW_BYTES + 1;

size_t putOffset(char *line, uint32_t offset) {
	for (size_t index = 0; index < OFFSET_DIGITS; index++) {
		uint32_t shift = static_cast<uint32_t>(OFFSET_DIGITS - 1 - index) * NIBBLE_BITS;
		line[index] = DIGITS[(offset >> shift) & NIBBLE_MASK];
	}
	return OFFSET_DIGITS;
}

} // namespace

bool RowWriter::flush() {
	char line[LINE_BYTES] = {};
	size_t at = putOffset(line, offset);
	for (size_t index = 0; index < HEX_GAP; index++) line[at++] = SPACE;

	for (size_t index = 0; index < ROW_BYTES; index++) {
		bool present = index < filled;
		line[at++] = present ? DIGITS[pending[index] >> NIBBLE_BITS] : SPACE;
		line[at++] = present ? DIGITS[pending[index] & NIBBLE_MASK] : SPACE;
		line[at++] = SPACE;
	}

	line[at++] = SPACE;
	line[at++] = BAR;
	for (size_t index = 0; index < filled; index++) {
		bool printable = pending[index] >= PRINTABLE_FIRST && pending[index] <= PRINTABLE_LAST;
		line[at++] = printable ? static_cast<char>(pending[index]) : UNPRINTABLE;
	}
	line[at++] = BAR;
	line[at++] = NEWLINE;

	offset += static_cast<uint32_t>(filled);
	filled = 0;
	return writeAll(STDOUT_DESCRIPTOR, line, at);
}

bool RowWriter::add(uint8_t byte) {
	pending[filled++] = byte;
	return filled < ROW_BYTES || flush();
}

bool RowWriter::finish() {
	if (offset == 0 && filled == 0) return true;
	if (filled != 0 && !flush()) return false;

	char line[OFFSET_DIGITS + 1] = {};
	size_t at = putOffset(line, offset);
	line[at++] = NEWLINE;
	return writeAll(STDOUT_DESCRIPTOR, line, at);
}

} // namespace cock::hexdump
