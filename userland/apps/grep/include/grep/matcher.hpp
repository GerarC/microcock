/** @file
 * @brief Cutting an input into lines and printing the ones that contain a text.
 */
#ifndef GREP_MATCHER_HPP
#define GREP_MATCHER_HPP

#include <cock/user/stream.hpp>
#include <stddef.h>

namespace cock::grep {

/** @brief Bytes of a line that are kept; the rest of a longer line is dropped. */
constexpr size_t LINE_BYTES = 255;

/** @brief Looks for a text in the lines of one input and prints those that have it. */
class LineMatcher {
public:
	/**
	 * @brief Prepares a matcher for one input.
	 *
	 * @param program Name of the program, for the message of a failed write.
	 * @param text Text to look for; empty matches every line.
	 * @param label Name put in front of each printed line as `<label>:`, or null for no prefix.
	 */
	LineMatcher(const char *program, const char *text, const char *label);

	/**
	 * @brief Takes the next chunk of the input; every line that ends in it is checked at once.
	 *
	 * @param chunk First byte.
	 * @param count Number of bytes.
	 * @return `Flow::MORE`, or `Flow::ERROR` after printing why when the output cannot be written.
	 */
	cock::user::tool::Flow add(const char *chunk, size_t count);

	/**
	 * @brief Checks the last line when the input ended without a newline.
	 *
	 * @return `Flow::MORE`, or `Flow::ERROR` as `add` does.
	 */
	cock::user::tool::Flow finish();

	/** @brief True once a line has matched. */
	bool matched() const { return found; }

private:
	cock::user::tool::Flow endLine();

	const char *program;
	const char *text;
	size_t textLength;
	const char *label;
	char line[LINE_BYTES] = {};
	size_t length = 0;
	bool found = false;
};

} // namespace cock::grep

#endif // !GREP_MATCHER_HPP
