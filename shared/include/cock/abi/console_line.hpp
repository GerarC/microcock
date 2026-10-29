/**
 * @file console_line.hpp
 * @brief Line editor of the console server. It does no I/O, so it is tested on the host.
 *
 * Typed characters are collected in an editing line. Backspace erases the last one and Enter moves the whole
 * line plus a newline to a queue of finished input that `take` drains.
 */
#ifndef COCK_ABI_CONSOLE_LINE_HPP
#define COCK_ABI_CONSOLE_LINE_HPP

#include <cock/abi/console.hpp>
#include <cock/abi/console_history.hpp>
#include <cock/data_structure/ring_buffer.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::console {

/** Most characters of one line being edited; more are ignored. */
constexpr size_t LINE_CAPACITY = 128;
static_assert(LINE_CAPACITY == HISTORY_LINE_CAPACITY, "a line of the history must fit the editing line");
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
	/** The editing line was replaced by a line of the history: erase `replacedLength` characters and print the line. */
	REPLACE,
};

/** Line editing state: the line being typed and the queue of committed input. Not thread safe. */
class LineDiscipline {
  public:
	/**
	 * @brief Processes one typed character.
	 * @param key Key from the keyboard driver: a character, or a special key (`SPECIAL_KEY_BASE` and above). `KEY_UP`
	 * and `KEY_DOWN` replace the editing line with an older or newer line of the history (the line typed before the first
	 * recall comes back at the end), and give `Echo::REPLACE`, or `Echo::NONE` when there is no such line. Any other
	 * special key is ignored.
	 * @return The echo to show. Non printable characters, characters on a full line and backspace on an empty line give `Echo::NONE`.
	 */
	Echo feed(uint32_t key) {
		if (key == KEY_UP) return recallPrevious();
		if (key == KEY_DOWN) return recallNext();
		if (key > LAST_CHARACTER_KEY) return Echo::NONE;
		char character = static_cast<char>(key);

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

	/**
	 * @brief Gives the line being edited.
	 * @return The characters, not null terminated; `currentLength` tells how many.
	 */
	const char *currentLine() const { return line; }

	/**
	 * @brief Gives the length of the line being edited.
	 * @return Number of characters.
	 */
	size_t currentLength() const { return length; }

	/**
	 * @brief Gives how many characters the screen shows of the line that the last `Echo::REPLACE` replaced.
	 * @return Number of characters to erase before printing `currentLine`; meaningful only after `Echo::REPLACE`.
	 */
	size_t replacedLength() const { return replaced; }

  private:
	void commit() {
		for (size_t index = 0; index < length; index++) {
			if (!ready.push(line[index])) break;
		}
		ready.push(CHAR_NEWLINE);
		history.add(line, length);
		browsing = history.count();
		length = 0;
	}

	Echo recallPrevious() {
		if (browsing == 0) return Echo::NONE;
		if (browsing == history.count()) saveDraft();
		browsing--;
		return load(history.line(browsing), history.length(browsing));
	}

	Echo recallNext() {
		if (browsing >= history.count()) return Echo::NONE;
		browsing++;
		if (browsing == history.count()) return load(draft, draftLength);
		return load(history.line(browsing), history.length(browsing));
	}

	void saveDraft() {
		for (size_t index = 0; index < length; index++)
			draft[index] = line[index];
		draftLength = length;
	}

	Echo load(const char *text, size_t count) {
		replaced = length;
		for (size_t index = 0; index < count; index++)
			line[index] = text[index];
		length = count;
		return Echo::REPLACE;
	}

	char line[LINE_CAPACITY] = {};
	size_t length = 0;
	size_t replaced = 0;
	char draft[LINE_CAPACITY] = {};
	size_t draftLength = 0;
	size_t browsing = 0;
	History history;
	data_structure::RingBuffer<char, READY_CAPACITY> ready;
};

} // namespace cock::abi::console

#endif // !COCK_ABI_CONSOLE_LINE_HPP
