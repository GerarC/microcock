/** @file
 * @brief One line of output built in memory and written to standard output with a single `write`.
 *
 * The escape sequences of a colored line then reach the terminal together, so no other process can print between them.
 */
#ifndef PS_LINE_HPP
#define PS_LINE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::ps {

/** Capacity of a `Line` in bytes, enough for a row with every color and the longest process name. */
constexpr size_t LINE_BYTES = 160;

/**
 * @brief A line of text with a fixed capacity.
 *
 * Whatever does not fit in `LINE_BYTES` is dropped; the line never overflows.
 */
class Line {
public:
	/**
	 * @brief Appends a string.
	 *
	 * @param value Zero terminated text.
	 */
	void text(const char *value);

	/**
	 * @brief Appends one character.
	 *
	 * @param value The character.
	 */
	void character(char value);

	/**
	 * @brief Appends a run of spaces.
	 *
	 * @param count How many.
	 */
	void spaces(size_t count);

	/**
	 * @brief Appends a number in decimal, right aligned to a width.
	 *
	 * @param value The number.
	 * @param width Minimum number of columns; shorter numbers are padded with spaces on the left.
	 */
	void number(uint32_t value, size_t width);

	/**
	 * @brief Appends a string right aligned to a width.
	 *
	 * @param value Zero terminated text.
	 * @param width Minimum number of columns; shorter text is padded with spaces on the left.
	 */
	void rightAligned(const char *value, size_t width);

	/**
	 * @brief Appends a newline, writes the line to standard output with one `write` and empties it.
	 *
	 * A failed or short write is ignored: `ps` has nowhere else to report it.
	 */
	void finish();

private:
	char buffer_[LINE_BYTES];
	size_t used_ = 0;
};

} // namespace cock::ps

#endif // !PS_LINE_HPP
