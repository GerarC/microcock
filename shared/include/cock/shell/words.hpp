#ifndef COCK_SHELL_WORDS_HPP
#define COCK_SHELL_WORDS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::shell {

constexpr size_t MAX_WORDS = 8;
constexpr size_t MAX_LINE_BYTES = 128;
constexpr char SPACE = ' ';
constexpr char TAB = '\t';
constexpr char TERMINATOR = '\0';
constexpr char FIRST_DIGIT = '0';
constexpr char LAST_DIGIT = '9';
constexpr uint32_t DECIMAL_BASE = 10;
constexpr uint32_t MAX_UINT32 = 0xFFFFFFFF;

struct Words {
	char *items[MAX_WORDS];
	size_t count;
};

inline bool isSeparator(char character) {
	return character == SPACE || character == TAB;
}

inline bool split(char *line, Words &words) {
	words.count = 0;
	size_t index = 0;

	while (line[index] != TERMINATOR) {
		while (isSeparator(line[index]))
			line[index++] = TERMINATOR;
		if (line[index] == TERMINATOR) break;

		if (words.count == MAX_WORDS) return false;
		words.items[words.count++] = &line[index];
		while (line[index] != TERMINATOR && !isSeparator(line[index]))
			index++;
	}
	return true;
}

inline bool parseNumber(const char *text, uint32_t &value) {
	if (text[0] == TERMINATOR) return false;

	uint32_t result = 0;
	for (size_t index = 0; text[index] != TERMINATOR; index++) {
		if (text[index] < FIRST_DIGIT || text[index] > LAST_DIGIT) return false;

		uint32_t digit = static_cast<uint32_t>(text[index] - FIRST_DIGIT);
		if (result > (MAX_UINT32 - digit) / DECIMAL_BASE) return false;
		result = result * DECIMAL_BASE + digit;
	}
	value = result;
	return true;
}

inline bool join(const Words &words, size_t first, char *buffer, size_t size, size_t &length) {
	length = 0;
	for (size_t word = first; word < words.count; word++) {
		if (word > first) {
			if (length + 1 >= size) return false;
			buffer[length++] = SPACE;
		}
		for (size_t index = 0; words.items[word][index] != TERMINATOR; index++) {
			if (length + 1 >= size) return false;
			buffer[length++] = words.items[word][index];
		}
	}
	if (size == 0) return false;
	buffer[length] = TERMINATOR;
	return true;
}

} // namespace cock::shell

#endif // !COCK_SHELL_WORDS_HPP
