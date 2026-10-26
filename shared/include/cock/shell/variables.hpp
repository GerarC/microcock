/**
 * @file variables.hpp
 * @brief Shell variables: names, assignments and a small fixed table. No I/O, tested on the host.
 */
#ifndef COCK_SHELL_VARIABLES_HPP
#define COCK_SHELL_VARIABLES_HPP

#include <cock/shell/words.hpp>
#include <stddef.h>

namespace cock::shell {

/** Most variables the table holds. */
constexpr size_t MAX_VARIABLES = 16;
/** Longest variable name in bytes, the terminator included. */
constexpr size_t NAME_BYTES = 32;
/** Longest variable value in bytes, the terminator included. */
constexpr size_t VALUE_BYTES = 64;
/** Character that starts an expansion and separates a name from its value in an assignment. */
constexpr char DOLLAR = '$';
/** Separates the name from the value in an assignment. */
constexpr char ASSIGN = '=';
/** Name of the variable that holds the status of the last program. */
constexpr char STATUS_NAME[] = "?";
/** Name of the variable that holds the current directory. */
constexpr char DIRECTORY_NAME[] = "PWD";
/** Lowest lower case letter. */
constexpr char FIRST_LOWER = 'a';
/** Highest lower case letter. */
constexpr char LAST_LOWER = 'z';
/** Lowest upper case letter. */
constexpr char FIRST_UPPER = 'A';
/** Highest upper case letter. */
constexpr char LAST_UPPER = 'Z';
/** Underscore, allowed in names. */
constexpr char UNDERSCORE = '_';

/**
 * @brief Tells whether a character can start a variable name.
 * @param character Character to test.
 * @return True for letters and the underscore.
 */
inline bool isNameStart(char character) {
	return (character >= FIRST_LOWER && character <= LAST_LOWER) || (character >= FIRST_UPPER && character <= LAST_UPPER) ||
		   character == UNDERSCORE;
}

/**
 * @brief Tells whether a character can be part of a variable name after the first one.
 * @param character Character to test.
 * @return True for letters, digits and the underscore.
 */
inline bool isNameChar(char character) {
	return isNameStart(character) || (character >= FIRST_DIGIT && character <= LAST_DIGIT);
}

/**
 * @brief Measures the variable name at the start of a text.
 * @param text Null terminated text.
 * @return Length of the longest run of name characters that starts with a name start; 0 when the text does not begin
 *         with a name.
 */
inline size_t nameLength(const char *text) {
	if (!isNameStart(text[0])) return 0;

	size_t length = 1;
	while (isNameChar(text[length]))
		length++;
	return length;
}

/**
 * @brief Tells whether a word is an assignment, `NAME=value`.
 * @param word Null terminated word, quotes already removed.
 * @param length Receives the length of the name when the word is an assignment; untouched otherwise.
 * @return True when the word starts with a name followed by `=`; the value is what follows the `=`.
 */
inline bool isAssignment(const char *word, size_t &length) {
	size_t found = nameLength(word);
	if (found == 0 || word[found] != ASSIGN) return false;
	length = found;
	return true;
}

/** Outcome of `Variables::set`. */
enum class SetResult {
	/** The variable now has the value. */
	OK,
	/** The name or the value does not fit its buffer. */
	TOO_LONG,
	/** The table is full and the name is new. */
	TABLE_FULL,
};

/** Fixed table of shell variables: no allocation, names compared by bytes. */
class Variables {
public:
	/**
	 * @brief Sets a variable, creating it when it is new.
	 * @param name Name, `length` bytes, not null terminated.
	 * @param length Length of `name`, at most `NAME_BYTES - 1`.
	 * @param value Null terminated value, at most `VALUE_BYTES - 1` bytes.
	 * @return `OK`, `TOO_LONG` or `TABLE_FULL`; the table does not change on failure.
	 */
	SetResult set(const char *name, size_t length, const char *value) {
		size_t valueLength = 0;
		while (value[valueLength] != TERMINATOR)
			valueLength++;
		if (length >= NAME_BYTES || valueLength >= VALUE_BYTES) return SetResult::TOO_LONG;

		size_t slot = find(name, length);
		if (slot == MAX_VARIABLES) {
			for (slot = 0; slot < MAX_VARIABLES && used[slot]; slot++) {
			}
			if (slot == MAX_VARIABLES) return SetResult::TABLE_FULL;
			for (size_t index = 0; index < length; index++)
				names[slot][index] = name[index];
			names[slot][length] = TERMINATOR;
			used[slot] = true;
		}
		for (size_t index = 0; index <= valueLength; index++)
			values[slot][index] = value[index];
		return SetResult::OK;
	}

	/**
	 * @brief Looks a variable up.
	 * @param name Name, `length` bytes, not null terminated.
	 * @param length Length of `name`.
	 * @return The null terminated value, valid until the next `set` or `unset`; null when the variable is not set.
	 */
	const char *get(const char *name, size_t length) const {
		size_t slot = find(name, length);
		return slot == MAX_VARIABLES ? nullptr : values[slot];
	}

	/**
	 * @brief Removes a variable.
	 * @param name Name, `length` bytes, not null terminated.
	 * @param length Length of `name`.
	 * @return True when the variable existed.
	 */
	bool unset(const char *name, size_t length) {
		size_t slot = find(name, length);
		if (slot == MAX_VARIABLES) return false;
		used[slot] = false;
		return true;
	}

	/**
	 * @brief Reaches the variables in the order of the table, for listing them.
	 * @param index Position, from 0 up to but excluding `MAX_VARIABLES`.
	 * @param name Receives the null terminated name of the variable at that position.
	 * @param value Receives its null terminated value.
	 * @return True when the position holds a variable; false when it is empty or out of range.
	 */
	bool at(size_t index, const char *&name, const char *&value) const {
		if (index >= MAX_VARIABLES || !used[index]) return false;
		name = names[index];
		value = values[index];
		return true;
	}

private:
	size_t find(const char *name, size_t length) const {
		for (size_t slot = 0; slot < MAX_VARIABLES; slot++) {
			if (!used[slot]) continue;
			size_t index = 0;
			while (index < length && names[slot][index] == name[index])
				index++;
			if (index == length && names[slot][length] == TERMINATOR) return slot;
		}
		return MAX_VARIABLES;
	}

	char names[MAX_VARIABLES][NAME_BYTES] = {};
	char values[MAX_VARIABLES][VALUE_BYTES] = {};
	bool used[MAX_VARIABLES] = {};
};

} // namespace cock::shell

#endif // !COCK_SHELL_VARIABLES_HPP
