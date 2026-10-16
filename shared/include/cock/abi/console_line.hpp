/**
 * @file console_line.hpp
 * @brief Line editor of the console server. It does no I/O, so it is tested on the host.
 *
 * Typed characters are collected in an editing line. Backspace erases the last one and Enter moves the whole
 * line plus a newline to a queue of finished input that `take` drains.
 */
#ifndef COCK_ABI_CONSOLE_LINE_HPP
#define COCK_ABI_CONSOLE_LINE_HPP

#include <cock/data_structure/ring_buffer.hpp>
#include <stddef.h>

namespace cock::abi::console {

/** Most characters of one line being edited; more are ignored. */
constexpr size_t LINE_CAPACITY = 128;
/** Capacity of the queue of finished lines in characters; overflow drops characters. */
constexpr size_t READY_CAPACITY = 512;
/** Character that erases the previous character of the line. */
constexpr char CHAR_BACKSPACE = '\b';
/** Character that ends a line and commits it. */
constexpr char CHAR_NEWLINE = '\n';
/** Lowest accepted character (space). */
constexpr char FIRST_PRINTABLE = ' ';
/** Highest accepted character (tilde). */
constexpr char LAST_PRINTABLE = '~';

/** What the caller must echo to the screen after `feed`. */
enum class Echo {
	/** Nothing: the character was ignored. */
	NONE,
	/** Print the character that was fed. */
	CHARACTER,
	/** Erase the last character on screen (backspace, space, backspace). */
	ERASE,
	/** Print a newline; the line was committed. */
	NEWLINE,
};

/** Line editing state: the line being typed and the queue of committed input. Not thread safe. */
class LineDiscipline {
  public:
	/**
	 * @brief Processes one typed character.
	 * @param character Character from the keyboard driver.
	 * @return The echo to show. Non printable characters, characters on a full line and backspace on an empty line give `Echo::NONE`.
	 */
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

	/**
	 * @brief Removes committed input from the queue.
	 * @param out Destination buffer, at least `limit` bytes.
	 * @param limit Most characters to take.
	 * @return Number of characters copied; 0 when nothing is ready.
	 */
	size_t take(char *out, size_t limit) {
		size_t taken = 0;
		while (taken < limit && ready.pop(out[taken]))
			taken++;
		return taken;
	}

	/**
	 * @brief Tells whether finished input is waiting.
	 * @return True when `take` would return at least one character.
	 */
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
