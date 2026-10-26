/**
 * @file words.hpp
 * @brief Command line helpers of the shell: splitting into words, number parsing and joining. No I/O, tested on the host.
 */
#ifndef COCK_SHELL_WORDS_HPP
#define COCK_SHELL_WORDS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::shell {

/** Most words in one command line, as many as `exec` accepts arguments; a longer line is rejected by `split`. */
constexpr size_t MAX_WORDS = 16;
/** Longest command line in bytes, matching the console line capacity. */
constexpr size_t MAX_LINE_BYTES = 128;
/** Word separator. */
constexpr char SPACE = ' ';
/** Word separator. */
constexpr char TAB = '\t';
/** End of a string. */
constexpr char TERMINATOR = '\0';
/** Lowest decimal digit character. */
constexpr char FIRST_DIGIT = '0';
/** Highest decimal digit character. */
constexpr char LAST_DIGIT = '9';
/** Opens and closes a quoted part in which nothing is special; no escapes. */
constexpr char SINGLE_QUOTE = '\'';
/** Opens and closes a quoted part in which only `\"` and `\\` are escapes. */
constexpr char DOUBLE_QUOTE = '"';
/** Escape character: outside quotes it makes the next character literal. */
constexpr char BACKSLASH = '\\';
/** Value of the quote state while the parser is outside any quote. */
constexpr char NO_QUOTE = TERMINATOR;
/** Base of the numbers accepted by `parseNumber`. */
constexpr uint32_t DECIMAL_BASE = 10;
/** Largest value that `parseNumber` accepts. */
constexpr uint32_t MAX_UINT32 = 0xFFFFFFFF;
/** Unquoted character that ends a command and sends its output to the next one. */
constexpr char PIPE_CHARACTER = '|';
/** Unquoted character that reads the input of a command from a file. */
constexpr char INPUT_CHARACTER = '<';
/** Unquoted character that writes the output of a command to a file (twice: appends to it). */
constexpr char OUTPUT_CHARACTER = '>';
/** Text of the token `|`. */
inline constexpr char PIPE_TEXT[] = "|";
/** Text of the token `<`. */
inline constexpr char INPUT_TEXT[] = "<";
/** Text of the token `>`. */
inline constexpr char OUTPUT_TEXT[] = ">";
/** Text of the token `>>`. */
inline constexpr char APPEND_TEXT[] = ">>";

/** Outcome of `split`. */
enum class SplitResult {
	/** The line was split. */
	OK,
	/** The line has more than `MAX_WORDS` words. */
	TOO_MANY_WORDS,
	/** A single or double quote was opened and never closed. */
	UNTERMINATED_QUOTE,
};

/** What a token of a line is. */
enum class TokenKind : uint8_t {
	/** An ordinary word, quotes and escapes already removed. */
	WORD,
	/** `|`. */
	PIPE,
	/** `<`. */
	REDIRECT_INPUT,
	/** `>`. */
	REDIRECT_OUTPUT,
	/** `>>`. */
	REDIRECT_APPEND,
};

/** Result of `split`: pointers into the original line, or to the text of an operator. */
struct Words {
	/** Start of each token; each word is null terminated in place, an operator points to its own text. */
	const char *items[MAX_WORDS];
	/** What each token is; a quoted or escaped `|`, `<` or `>` is a `WORD`. */
	TokenKind kinds[MAX_WORDS];
	/** Number of tokens found, at most `MAX_WORDS`. */
	size_t count;
};

/**
 * @brief Tells whether a character separates words.
 * @param character Character to test.
 * @return True for space and tab.
 */
inline bool isSeparator(char character) {
	return character == SPACE || character == TAB;
}

/**
 * @brief Tells whether an unquoted character is an operator of the shell.
 * @param character Character to test.
 * @return True for `|`, `<` and `>`.
 */
inline bool isOperator(char character) {
	return character == PIPE_CHARACTER || character == INPUT_CHARACTER || character == OUTPUT_CHARACTER;
}

/**
 * @brief Adds the operator token that starts at `read` and moves `read` past it.
 *
 * The first character is passed because the cell it was in may already hold the terminator of the word before it.
 * @param first The operator character, already read.
 * @param line The line; the character after `read` is looked at to tell `>` from `>>`.
 * @param read Index of the operator character; moves past the operator.
 * @param words Receives the token; its count must be below `MAX_WORDS`.
 */
inline void addOperator(char first, const char *line, size_t &read, Words &words) {
	read++;
	const char *text = PIPE_TEXT;
	TokenKind kind = TokenKind::PIPE;
	if (first == INPUT_CHARACTER) {
		text = INPUT_TEXT;
		kind = TokenKind::REDIRECT_INPUT;
	} else if (first == OUTPUT_CHARACTER) {
		text = OUTPUT_TEXT;
		kind = TokenKind::REDIRECT_OUTPUT;
		if (line[read] == OUTPUT_CHARACTER) {
			read++;
			text = APPEND_TEXT;
			kind = TokenKind::REDIRECT_APPEND;
		}
	}
	words.items[words.count] = text;
	words.kinds[words.count++] = kind;
}

/**
 * @brief Splits a line into words and operators in place.
 *
 * Words are separated by spaces and tabs. Single quotes keep everything between them literally; double quotes keep
 * it too except that `\"` and `\\` stand for `"` and `\`; outside quotes a backslash makes the next character literal
 * (a backslash at the end of the line stays). Quotes join with the text next to them (`a"b c"d` is one word) and
 * `""` is an empty word. Quotes and escapes are removed in place: the result is never longer than the line, so the
 * words point into `line`, which must outlive them. An unquoted `|`, `<`, `>` or `>>` is a token of its own, with or
 * without spaces around it (`a>b` is `a`, `>`, `b`); quoted or escaped it is part of a word.
 * @param line Writable null terminated line.
 * @param words Receives the tokens; `count` is reset to 0 first.
 * @return `OK`, or `TOO_MANY_WORDS` when the line has more than `MAX_WORDS` tokens (the first ones are still
 *         filled in), or `UNTERMINATED_QUOTE` when a quote is never closed (`words` is then unusable).
 */
inline SplitResult split(char *line, Words &words) {
	words.count = 0;
	size_t read = 0;
	size_t write = 0;

	while (true) {
		while (isSeparator(line[read]))
			read++;
		if (line[read] == TERMINATOR) return SplitResult::OK;
		if (words.count == MAX_WORDS) return SplitResult::TOO_MANY_WORDS;
		if (isOperator(line[read])) {
			addOperator(line[read], line, read, words);
			continue;
		}
		words.kinds[words.count] = TokenKind::WORD;
		words.items[words.count++] = &line[write];

		char quote = NO_QUOTE;
		while (line[read] != TERMINATOR && (quote != NO_QUOTE || (!isSeparator(line[read]) && !isOperator(line[read])))) {
			char character = line[read++];
			if (quote == NO_QUOTE) {
				if (character == SINGLE_QUOTE || character == DOUBLE_QUOTE) {
					quote = character;
					continue;
				}
				if (character == BACKSLASH && line[read] != TERMINATOR) character = line[read++];
			} else if (character == quote) {
				quote = NO_QUOTE;
				continue;
			} else if (quote == DOUBLE_QUOTE && character == BACKSLASH &&
					   (line[read] == DOUBLE_QUOTE || line[read] == BACKSLASH)) {
				character = line[read++];
			}
			line[write++] = character;
		}
		if (quote != NO_QUOTE) return SplitResult::UNTERMINATED_QUOTE;

		char next = line[read];
		line[write++] = TERMINATOR;
		if (next == TERMINATOR) return SplitResult::OK;
		if (!isOperator(next)) {
			read++;
			continue;
		}
		if (words.count == MAX_WORDS) return SplitResult::TOO_MANY_WORDS;
		addOperator(next, line, read, words);
	}
}

/**
 * @brief Parses an unsigned decimal number.
 * @param text Null terminated digits only (no sign, no spaces, no prefix).
 * @param value Receives the number on success; untouched on failure.
 * @return False for an empty string, any non digit character or a value above 4294967295.
 */
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

/**
 * @brief Joins words with single spaces into a buffer.
 * @param words Words to join.
 * @param first Index of the first word to include; at or above `count` gives an empty string.
 * @param buffer Destination.
 * @param size Capacity of `buffer` in bytes, including the terminator.
 * @param length Receives the length of the text, without the terminator.
 * @return True on success; false when `size` is 0 or the text does not fit.
 */
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
