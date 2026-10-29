#include <wc/counts.hpp>

namespace cock::wc {

namespace {

constexpr char NEWLINE = '\n';
constexpr char SPACE = ' ';
constexpr char TAB = '\t';
constexpr char RETURN = '\r';
constexpr char VERTICAL_TAB = '\v';
constexpr char FORM_FEED = '\f';

bool isWhiteSpace(char byte) {
	return byte == SPACE || byte == NEWLINE || byte == TAB || byte == RETURN || byte == VERTICAL_TAB ||
		   byte == FORM_FEED;
}

} // namespace

void Counts::add(const char *chunk, size_t count) {
	for (size_t index = 0; index < count; index++) {
		if (chunk[index] == NEWLINE) lines++;
		if (isWhiteSpace(chunk[index])) {
			inWord = false;
		} else if (!inWord) {
			inWord = true;
			words++;
		}
	}
	bytes += static_cast<uint32_t>(count);
}

void Counts::merge(const Counts &other) {
	lines += other.lines;
	words += other.words;
	bytes += other.bytes;
}

} // namespace cock::wc
