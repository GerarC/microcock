#include <ps/line.hpp>
#include <string.h>
#include <unistd.h>

namespace cock::ps {

namespace {

constexpr int STANDARD_OUTPUT = 1;
constexpr char SEPARATOR = ' ';
constexpr char NEWLINE = '\n';
constexpr char ZERO_DIGIT = '0';
constexpr uint32_t DECIMAL_RADIX = 10;
constexpr size_t MAX_DIGITS = 10;

} // namespace

void Line::character(char value) {
	if (used_ < LINE_BYTES) buffer_[used_++] = value;
}

void Line::text(const char *value) {
	for (; *value != '\0'; value++) character(*value);
}

void Line::spaces(size_t count) {
	for (size_t index = 0; index < count; index++) character(SEPARATOR);
}

void Line::number(uint32_t value, size_t width) {
	char digits[MAX_DIGITS];
	size_t count = 0;
	do {
		digits[count++] = static_cast<char>(ZERO_DIGIT + value % DECIMAL_RADIX);
		value /= DECIMAL_RADIX;
	} while (value != 0);

	spaces(width > count ? width - count : 0);
	while (count > 0) character(digits[--count]);
}

void Line::rightAligned(const char *value, size_t width) {
	size_t length = strlen(value);
	spaces(width > length ? width - length : 0);
	text(value);
}

void Line::finish() {
	if (used_ == LINE_BYTES) used_--;
	buffer_[used_++] = NEWLINE;
	write(STANDARD_OUTPUT, buffer_, used_);
	used_ = 0;
}

} // namespace cock::ps
