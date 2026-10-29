/**
 * @file console_history.hpp
 * @brief The lines a terminal already ran, kept for the Up and Down keys. It does no I/O, so it is tested on the host.
 */
#ifndef COCK_ABI_CONSOLE_HISTORY_HPP
#define COCK_ABI_CONSOLE_HISTORY_HPP

#include <stddef.h>

namespace cock::abi::console {

/** Most characters of one line kept in the history. */
constexpr size_t HISTORY_LINE_CAPACITY = 128;
/** Most lines kept; adding one more drops the oldest. */
constexpr size_t HISTORY_CAPACITY = 16;

/** The last lines that were run, oldest first. Not thread safe. */
class History {
  public:
	/**
	 * @brief Adds a line as the newest one.
	 *
	 * An empty line, and a line equal to the newest one, are not added. A line longer than `HISTORY_LINE_CAPACITY` is
	 * cut. When the history is full the oldest line is dropped.
	 * @param text Characters of the line, without the newline.
	 * @param length Number of characters.
	 */
	void add(const char *text, size_t length) {
		if (length == 0) return;
		if (length > HISTORY_LINE_CAPACITY) length = HISTORY_LINE_CAPACITY;
		if (used > 0 && sameAsNewest(text, length)) return;

		if (used == HISTORY_CAPACITY) {
			for (size_t index = 1; index < HISTORY_CAPACITY; index++)
				copy(index - 1, lines[index], lengths[index]);
			used--;
		}
		copy(used, text, length);
		used++;
	}

	/**
	 * @brief Gives how many lines are kept.
	 * @return A number from 0 to `HISTORY_CAPACITY`.
	 */
	size_t count() const { return used; }

	/**
	 * @brief Gives the characters of one line.
	 * @param index Position from the oldest (0) to the newest (`count() - 1`).
	 * @return The characters, not null terminated; null when `index` is not below `count()`.
	 */
	const char *line(size_t index) const { return index < used ? lines[index] : nullptr; }

	/**
	 * @brief Gives the length of one line.
	 * @param index Position from the oldest (0) to the newest (`count() - 1`).
	 * @return The number of characters; 0 when `index` is not below `count()`.
	 */
	size_t length(size_t index) const { return index < used ? lengths[index] : 0; }

  private:
	bool sameAsNewest(const char *text, size_t length) const {
		const size_t newest = used - 1;
		if (lengths[newest] != length) return false;
		for (size_t index = 0; index < length; index++) {
			if (lines[newest][index] != text[index]) return false;
		}
		return true;
	}

	void copy(size_t slot, const char *text, size_t length) {
		for (size_t index = 0; index < length; index++)
			lines[slot][index] = text[index];
		lengths[slot] = length;
	}

	char lines[HISTORY_CAPACITY][HISTORY_LINE_CAPACITY] = {};
	size_t lengths[HISTORY_CAPACITY] = {};
	size_t used = 0;
};

} // namespace cock::abi::console

#endif // !COCK_ABI_CONSOLE_HISTORY_HPP
