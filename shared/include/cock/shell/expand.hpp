/**
 * @file expand.hpp
 * @brief Variable expansion of a command line, done before `split`. No I/O, tested on the host.
 */
#ifndef COCK_SHELL_EXPAND_HPP
#define COCK_SHELL_EXPAND_HPP

#include <cock/shell/variables.hpp>
#include <cock/shell/words.hpp>
#include <stddef.h>

namespace cock::shell {

/** Outcome of `expand`. */
enum class ExpandResult {
	/** The line was expanded. */
	OK,
	/** The expanded line does not fit the output buffer. */
	TOO_LONG,
};

namespace detail {

/**
 * @brief Appends one character to the output of `expand`.
 * @param out Output buffer.
 * @param size Capacity of `out` in bytes, the terminator included.
 * @param length Bytes written so far; grows by one on success.
 * @param character Character to append.
 * @return False when there is no room for it and the terminator.
 */
inline bool put(char *out, size_t size, size_t &length, char character) {
	if (length + 1 >= size) return false;
	out[length++] = character;
	return true;
}

}

/**
 * @brief Replaces `$?` and `$NAME` references in a line with their values.
 *
 * A reference is `$` followed by `?` or by a name (letters, digits and underscore, not starting with a digit).
 * A `$` followed by anything else stays as it is. Nothing is expanded between single quotes, and `\$` is a literal
 * `$` (inside double quotes too). The value is inserted so that `split` reads it back as the same text: outside quotes
 * it is one piece of the word, with spaces, tabs, quotes and backslashes escaped (so it is never split nor re-read as a
 * quote), and inside double quotes only `"` and `\` are escaped. A variable that is not set gives an empty text, which
 * outside quotes makes the word vanish when it is alone. Quotes and escapes of the line itself are copied untouched
 * for `split` to remove.
 *
 * @tparam Lookup Callable `const char *(const char *name, size_t length)` that returns the null terminated value of the
 *         variable, valid until its next call, or null when it is not set.
 * @param line Null terminated line.
 * @param out Receives the expanded, null terminated line.
 * @param size Capacity of `out` in bytes, the terminator included.
 * @param lookup Gives the values.
 * @return `OK`, or `TOO_LONG` when the result needs `size` bytes or more (`out` is then unusable).
 */
template <typename Lookup>
inline ExpandResult expand(const char *line, char *out, size_t size, Lookup &&lookup) {
	size_t length = 0;
	char quote = NO_QUOTE;
	size_t read = 0;

	while (line[read] != TERMINATOR) {
		char character = line[read++];

		if (character == BACKSLASH && quote != SINGLE_QUOTE && line[read] != TERMINATOR) {
			char next = line[read];
			if (quote == DOUBLE_QUOTE && next == DOLLAR) {
				read++;
				if (!detail::put(out, size, length, DOLLAR)) return ExpandResult::TOO_LONG;
			} else if (quote == DOUBLE_QUOTE && next != DOUBLE_QUOTE && next != BACKSLASH) {
				if (!detail::put(out, size, length, character)) return ExpandResult::TOO_LONG;
			} else {
				read++;
				if (!detail::put(out, size, length, character) || !detail::put(out, size, length, next))
					return ExpandResult::TOO_LONG;
			}
			continue;
		}

		if (quote == NO_QUOTE && (character == SINGLE_QUOTE || character == DOUBLE_QUOTE)) {
			quote = character;
		} else if (quote == character) {
			quote = NO_QUOTE;
		} else if (character == DOLLAR && quote != SINGLE_QUOTE) {
			size_t found = 0;
			const char *name = &line[read];
			if (line[read] == STATUS_NAME[0]) found = sizeof(STATUS_NAME) - sizeof(char);
			else found = nameLength(name);
			if (found != 0) {
				read += found;
				const char *value = lookup(name, found);
				for (size_t index = 0; value != nullptr && value[index] != TERMINATOR; index++) {
					char byte = value[index];
					bool special = byte == DOUBLE_QUOTE || byte == BACKSLASH ||
								   (quote == NO_QUOTE && (byte == SINGLE_QUOTE || isSeparator(byte)));
					if (special && !detail::put(out, size, length, BACKSLASH)) return ExpandResult::TOO_LONG;
					if (!detail::put(out, size, length, byte)) return ExpandResult::TOO_LONG;
				}
				continue;
			}
		}
		if (!detail::put(out, size, length, character)) return ExpandResult::TOO_LONG;
	}
	out[length] = TERMINATOR;
	return ExpandResult::OK;
}

} // namespace cock::shell

#endif // !COCK_SHELL_EXPAND_HPP
