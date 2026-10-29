/**
 * @file resolve.hpp
 * @brief Turns a path that may be relative into an absolute, normalized one. No I/O and no allocation; tested on the host.
 *
 * A normalized path starts with `/`, has no empty component, no `.` and no `..`, and does not end with `/` unless it
 * is the root. `..` of the root is the root. Links are not looked at: the file servers resolve those.
 */
#ifndef SHARED_COCK_PATH_RESOLVE_HPP
#define SHARED_COCK_PATH_RESOLVE_HPP

#include <stddef.h>

namespace cock::path {

/** Separator of path components, and the whole root path. */
constexpr char SEPARATOR = '/';
/** Character of the components `.` and `..`. */
constexpr char DOT = '.';
/** End of a string. */
constexpr char TERMINATOR = '\0';
/** Length of the root path `/`. */
constexpr size_t ROOT_LENGTH = 1;
/** Length of a component that is a single character. */
constexpr size_t ONE_CHARACTER = 1;
/** Length of the component `..`. */
constexpr size_t PARENT_LENGTH = 2;
/** Offset of the second character of a component. */
constexpr size_t SECOND_CHARACTER = 1;
/** Smallest buffer `resolve` accepts: the root and its terminator. */
constexpr size_t MIN_CAPACITY = 2;

/**
 * @brief Applies the components of a path to a normalized path being built.
 *
 * @param out Holds the path so far, without a terminator; it grows in place.
 * @param capacity Size of `out`; one byte is kept for the terminator.
 * @param used Length of the path in `out`, at least `ROOT_LENGTH`; updated.
 * @param text Components separated by `/`, null terminated; a leading `/` is not looked at.
 * @return False when the result does not fit; `out` and `used` are then unspecified.
 */
inline bool apply(char *out, size_t capacity, size_t &used, const char *text) {
	size_t position = 0;
	while (text[position] != TERMINATOR) {
		while (text[position] == SEPARATOR) position++;
		size_t start = position;
		while (text[position] != TERMINATOR && text[position] != SEPARATOR) position++;
		size_t length = position - start;

		if (length == 0 || (length == ONE_CHARACTER && text[start] == DOT)) continue;
		if (length == PARENT_LENGTH && text[start] == DOT && text[start + SECOND_CHARACTER] == DOT) {
			while (used > ROOT_LENGTH && out[used - 1] != SEPARATOR) used--;
			if (used > ROOT_LENGTH) used--;
			continue;
		}

		size_t separator = used > ROOT_LENGTH ? ONE_CHARACTER : 0;
		if (used + separator + length + ONE_CHARACTER > capacity) return false;
		if (separator != 0) out[used++] = SEPARATOR;
		for (size_t index = 0; index < length; index++) out[used++] = text[start + index];
	}
	return true;
}

/**
 * @brief Makes an absolute, normalized path from a path and a working directory.
 *
 * @param base The working directory, absolute; used only when `path` is relative. Null counts as the root. It is
 * normalized too, so a `base` with `..` in it is harmless.
 * @param path The path to resolve, null terminated; may be absolute or relative, and may hold `.` and `..`.
 * @param out Receives the result with its terminator.
 * @param capacity Size of `out`, at least `MIN_CAPACITY`.
 * @return False when an argument is null, `capacity` is too small or the result does not fit; `out` then holds an
 * empty string (when `capacity` is not zero).
 */
inline bool resolve(const char *base, const char *path, char *out, size_t capacity) {
	if (out == nullptr || capacity == 0) return false;
	out[0] = TERMINATOR;
	if (path == nullptr || capacity < MIN_CAPACITY) return false;

	out[0] = SEPARATOR;
	size_t used = ROOT_LENGTH;
	bool fits = true;
	if (path[0] != SEPARATOR && base != nullptr) fits = apply(out, capacity, used, base);
	fits = fits && apply(out, capacity, used, path);
	if (!fits) {
		out[0] = TERMINATOR;
		return false;
	}
	out[used] = TERMINATOR;
	return true;
}

} // namespace cock::path

#endif // !SHARED_COCK_PATH_RESOLVE_HPP
