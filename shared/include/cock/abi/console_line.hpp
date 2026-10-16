#ifndef COCK_ABI_CONSOLE_LINE_HPP
#define COCK_ABI_CONSOLE_LINE_HPP

#include <cock/data_structure/ring_buffer.hpp>
#include <stddef.h>

namespace cock::abi::console {

constexpr size_t LINE_CAPACITY = 128;
constexpr size_t READY_CAPACITY = 512;
constexpr char CHAR_BACKSPACE = '\b';
constexpr char CHAR_NEWLINE = '\n';
constexpr char FIRST_PRINTABLE = ' ';
constexpr char LAST_PRINTABLE = '~';

enum class Echo {
	NONE,
	CHARACTER,
	ERASE,
	NEWLINE,
};

class LineDiscipline {
  public:
	Echo feed(char character) {
		if (character == CHAR_BACKSPACE) {
			if (length == 0) return Echo::NONE;
			length--;
			return Echo::ERASE;
		}

		if (character == CHAR_NEWLINE) {
			commit();
			return Echo::NEWLINE;
		}

		if (character < FIRST_PRINTABLE || character > LAST_PRINTABLE) return Echo::NONE;
		if (length >= LINE_CAPACITY) return Echo::NONE;
		line[length++] = character;
		return Echo::CHARACTER;
	}

	size_t take(char *out, size_t limit) {
		size_t taken = 0;
		while (taken < limit && ready.pop(out[taken]))
			taken++;
		return taken;
	}

	bool hasInput() const { return !ready.isEmpty(); }

  private:
	void commit() {
		for (size_t index = 0; index < length; index++) {
			if (!ready.push(line[index])) break;
		}
		ready.push(CHAR_NEWLINE);
		length = 0;
	}

	char line[LINE_CAPACITY] = {};
	size_t length = 0;
	data_structure::RingBuffer<char, READY_CAPACITY> ready;
};

} // namespace cock::abi::console

#endif // !COCK_ABI_CONSOLE_LINE_HPP
