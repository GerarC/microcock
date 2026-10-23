/**
 * @file manifest.hpp
 * @brief Parser of `manifest.conf`, the list of programs that `init` starts and the capabilities each one gets.
 *
 * The file is text, one entry per line: `name key=value ...`. Blank lines and lines that start with `#` are
 * ignored, and tokens are separated by spaces or tabs. Keys:
 * `path=<file>` (ELF in the initrd; no path means a boot data module), `peer=<name>` (process told about
 * by `CAPABILITY_GRANTED`), `repeat=<1..32>` (copies to start; 1 for boot modules), `restart=<1..16>` (times `init` starts the program
 * again after it ends, within one restart window; needs a path and `repeat` of 1), `arg=<number>` (one
 * numeric argument) and `caps=<grant>,<grant>...` with grants `io:<first>[+count]`, `irq:<line>[+count]` and
 * `mem:<first>+<bytes>` plus the bare `spawn` (the right to call `SYS_PROC_CREATE`), and `ipc=<name>,<name>...` (programs this one may exchange messages with, calls
 * and shared memory with; `init` grants one IPC capability for the service label of each name).
 * Numbers are decimal or `0x` hexadecimal and must fit in 32 bits. The parser has no
 * dependencies and is covered by host tests; init reads the file, the kernel never does.
 */
#ifndef SHARED_COCK_ABI_MANIFEST_HPP
#define SHARED_COCK_ABI_MANIFEST_HPP

#include <cock/abi/capability.hpp>
#include <cock/abi/service.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::manifest {

/** Path of the manifest inside the initrd. */
constexpr const char *FILE_PATH = "manifest.conf";
/** Largest manifest file `init` reads, in bytes. */
constexpr size_t MAX_FILE_BYTES = 4096;
/** Most entries in the manifest; one more is `TOO_MANY_ENTRIES`. */
constexpr size_t MAX_ENTRIES = 32;
/** Longest process or peer name including the terminator; longer names are rejected. */
constexpr size_t MAX_NAME_BYTES = 32;
/** Longest program path including the terminator. */
constexpr size_t MAX_PATH_BYTES = 64;
/** Most capability grants per entry. */
constexpr size_t MAX_GRANTS = 4;
/** Most names in the `ipc` list of one entry. */
constexpr size_t MAX_IPC_PEERS = service::MAX_SPAWN_IPC;
/** Number of copies when `repeat` is not given. */
constexpr uint32_t DEFAULT_REPEAT = 1;
/** Largest accepted value of `repeat`. */
constexpr uint32_t MAX_REPEAT = 32;
/** Value of `restarts` for a program that `init` does not start again. */
constexpr uint32_t NO_RESTARTS = 0;
/** Largest accepted value of `restart`. */
constexpr uint32_t MAX_RESTARTS = 16;

/** One capability granted to a process. */
struct Grant {
	/** Kind of resource. */
	CapabilityType type;
	/** First port, IRQ line or physical address. */
	uint32_t first;
	/** Number of ports or lines, or the size in bytes for memory. Never 0, and `first + count` never passes 2^32. */
	uint32_t count;
};

/** One parsed line of the manifest. */
struct Entry {
	/** Name of the process, unique in the file, null terminated. */
	char name[MAX_NAME_BYTES];
	/** Path of the ELF in the initrd; empty for a boot module. */
	char path[MAX_PATH_BYTES];
	/** Name of the peer process, or empty when there is none. */
	char peer[MAX_NAME_BYTES];
	/** Capabilities to grant; only the first `grantCount` are valid. */
	Grant grants[MAX_GRANTS];
	/** Number of valid grants, at most `MAX_GRANTS`. */
	size_t grantCount;
	/** Names of the programs this one may talk to (the `ipc` key); only the first `ipcCount` are valid. */
	char ipc[MAX_IPC_PEERS][MAX_NAME_BYTES];
	/** Number of valid names in `ipc`, at most `MAX_IPC_PEERS`. */
	size_t ipcCount;
	/** Copies to start, 1 to `MAX_REPEAT`. */
	uint32_t repeat;
	/** Restarts `init` may make within one restart window, or `NO_RESTARTS` when the program is not supervised. */
	uint32_t restarts;
	/** Numeric argument; valid only when `hasArgument` is true. */
	uint32_t argument;
	/** True when the line had `arg=`. */
	bool hasArgument;

	/**
	 * @brief Tells whether the entry names a data module instead of a program.
	 * @return True when the path is empty.
	 */
	bool isBootModule() const { return path[0] == '\0'; }
	/**
	 * @brief Tells whether a peer was given.
	 * @return True when the peer name is not empty.
	 */
	bool hasPeer() const { return peer[0] != '\0'; }
};

/** Reason why parsing stopped; see `Manifest::errorLine` for the line. */
enum class Error : uint32_t {
	/** No error. */
	NONE = 0,
	/** More than `MAX_ENTRIES` entries. */
	TOO_MANY_ENTRIES,
	/** Missing, too long, or containing `=`. */
	BAD_NAME,
	/** Another entry has the same name. */
	DUPLICATE_NAME,
	/** A token is not `key=value` or the key is not known. */
	UNKNOWN_KEY,
	/** A key appears twice on one line. */
	DUPLICATE_KEY,
	/** A value is empty, too long, not a number, out of range, or a boot module with `repeat` other than 1. */
	BAD_VALUE,
	/** More than `MAX_GRANTS` grants on one line. */
	TOO_MANY_GRANTS,
	/** More than `MAX_IPC_PEERS` names in the `ipc` list of one line. */
	TOO_MANY_PEERS,
};

namespace detail {

/** Line terminator. */
constexpr char NEWLINE = '\n';
/** Blank character. */
constexpr char SPACE = ' ';
/** Blank character. */
constexpr char TAB = '\t';
/** Blank character, so files with CRLF line ends parse. */
constexpr char CARRIAGE_RETURN = '\r';
/** First character of a comment line. */
constexpr char COMMENT_START = '#';
/** Separates a key from its value. */
constexpr char KEY_SEPARATOR = '=';
/** Separates grants in a `caps` list. */
constexpr char LIST_SEPARATOR = ',';
/** Separates the grant type from its range. */
constexpr char TYPE_SEPARATOR = ':';
/** Separates the first value of a range from its count. */
constexpr char COUNT_SEPARATOR = '+';
/** Second character of a hexadecimal prefix. */
constexpr char HEX_MARKER = 'x';
/** Uppercase form of the hexadecimal marker. */
constexpr char HEX_MARKER_UPPER = 'X';
/** Lowest decimal digit. */
constexpr char DIGIT_ZERO = '0';
/** Highest decimal digit. */
constexpr char DIGIT_NINE = '9';
/** Lowest lowercase hexadecimal digit. */
constexpr char LETTER_LOWER_A = 'a';
/** Highest lowercase hexadecimal digit. */
constexpr char LETTER_LOWER_F = 'f';
/** Lowest uppercase hexadecimal digit. */
constexpr char LETTER_UPPER_A = 'A';
/** Highest uppercase hexadecimal digit. */
constexpr char LETTER_UPPER_F = 'F';
/** Base of plain numbers; also the value of the digit `a`. */
constexpr uint32_t DECIMAL_BASE = 10;
/** Base of numbers with the `0x` prefix. */
constexpr uint32_t HEX_BASE = 16;
/** Length of the `0x` prefix. */
constexpr size_t HEX_PREFIX_BYTES = 2;
/** Value returned by `digitValue` for a character that is not a digit; above any base. */
constexpr uint32_t NOT_A_DIGIT = 0xFF;
/** Count of a grant that gives no `+count` (one port or one line). */
constexpr uint32_t SINGLE_UNIT = 1;
/** Largest value a number may have. */
constexpr uint64_t MAX_NUMBER = 0xFFFFFFFF;
/** End of the 32 bit address space; a grant range must end at or below it. */
constexpr uint64_t ADDRESS_LIMIT = uint64_t(1) << 32;

/** Key of the program path. */
constexpr const char *KEY_PATH = "path";
/** Key of the peer name. */
constexpr const char *KEY_PEER = "peer";
/** Key of the copy count. */
constexpr const char *KEY_REPEAT = "repeat";
/** Key of the restart limit. */
constexpr const char *KEY_RESTART = "restart";
/** Key of the capability list. */
constexpr const char *KEY_CAPS = "caps";
/** Key of the numeric argument. */
constexpr const char *KEY_ARGUMENT = "arg";
/** Key of the list of programs this one may talk to. */
constexpr const char *KEY_IPC = "ipc";
/** Grant type for `IO_PORTS`. */
constexpr const char *TYPE_IO = "io";
/** Grant type for `IRQ`. */
constexpr const char *TYPE_IRQ = "irq";
/** Grant type for `PHYSICAL_MEMORY`; a count in bytes is required. */
constexpr const char *TYPE_MEMORY = "mem";
/** Grant for `SPAWN`; it has no range, so it is written alone as `spawn`. */
constexpr const char *GRANT_SPAWN = "spawn";

/** Non owning view of a piece of the text, from `begin` up to but not including `end`. */
struct Slice {
	/** First character. */
	const char *begin;
	/** One past the last character. */
	const char *end;

	/**
	 * @brief Length of the view.
	 * @return Number of characters.
	 */
	size_t length() const { return static_cast<size_t>(end - begin); }
	/**
	 * @brief Tells whether the view has no characters.
	 * @return True when empty.
	 */
	bool empty() const { return begin == end; }

	/**
	 * @brief Compares the view with a null terminated string.
	 * @param text String to compare with; must not be null.
	 * @return True when both have exactly the same characters.
	 */
	bool equals(const char *text) const {
		size_t index = 0;
		for (; index < length(); index++)
			if (text[index] == '\0' || text[index] != begin[index]) return false;
		return text[index] == '\0';
	}
};

/**
 * @brief Tells whether a character is blank.
 * @param character Character to test.
 * @return True for space, tab and carriage return.
 */
inline bool isBlank(char character) {
	return character == SPACE || character == TAB || character == CARRIAGE_RETURN;
}

/**
 * @brief Removes blank characters from both ends.
 * @param slice View to trim.
 * @return The trimmed view.
 */
inline Slice trim(Slice slice) {
	while (slice.begin != slice.end && isBlank(*slice.begin))
		slice.begin++;
	while (slice.begin != slice.end && isBlank(*(slice.end - 1)))
		slice.end--;
	return slice;
}

/**
 * @brief Splits at the first occurrence of a separator.
 * @param whole View to split.
 * @param separator Character to look for.
 * @param before Receives the part before the separator, or the whole view when it is absent.
 * @param after Receives the part after the separator, or an empty view when it is absent.
 * @return True when the separator was found.
 */
inline bool splitAt(Slice whole, char separator, Slice &before, Slice &after) {
	for (const char *cursor = whole.begin; cursor != whole.end; cursor++) {
		if (*cursor != separator) continue;
		before = {whole.begin, cursor};
		after = {cursor + 1, whole.end};
		return true;
	}
	before = whole;
	after = {whole.end, whole.end};
	return false;
}

/**
 * @brief Takes the next blank separated token.
 * @param rest In: remaining text; out: text after the token (not trimmed).
 * @return The token, or an empty view when only blanks remain.
 */
inline Slice nextToken(Slice &rest) {
	rest = trim(rest);
	const char *cursor = rest.begin;
	while (cursor != rest.end && !isBlank(*cursor))
		cursor++;
	Slice token = {rest.begin, cursor};
	rest.begin = cursor;
	return token;
}

/**
 * @brief Value of a hexadecimal digit.
 * @param character Character `0-9`, `a-f` or `A-F`.
 * @return 0 to 15, or `NOT_A_DIGIT`.
 */
inline uint32_t digitValue(char character) {
	if (character >= DIGIT_ZERO && character <= DIGIT_NINE)
		return static_cast<uint32_t>(character - DIGIT_ZERO);
	if (character >= LETTER_LOWER_A && character <= LETTER_LOWER_F)
		return static_cast<uint32_t>(character - LETTER_LOWER_A) + DECIMAL_BASE;
	if (character >= LETTER_UPPER_A && character <= LETTER_UPPER_F)
		return static_cast<uint32_t>(character - LETTER_UPPER_A) + DECIMAL_BASE;
	return NOT_A_DIGIT;
}

/**
 * @brief Parses a decimal or `0x` hexadecimal number.
 * @param text Digits only; no sign or spaces.
 * @param value Receives the number on success.
 * @return False for an empty text, a bad digit, a lone `0x` prefix or a value above 32 bits.
 */
inline bool parseNumber(Slice text, uint32_t &value) {
	uint32_t base = DECIMAL_BASE;
	const char *cursor = text.begin;
	if (text.length() > HEX_PREFIX_BYTES && text.begin[0] == DIGIT_ZERO &&
		(text.begin[1] == HEX_MARKER || text.begin[1] == HEX_MARKER_UPPER)) {
		base = HEX_BASE;
		cursor += HEX_PREFIX_BYTES;
	}
	if (cursor == text.end) return false;

	uint64_t result = 0;
	for (; cursor != text.end; cursor++) {
		uint32_t digit = digitValue(*cursor);
		if (digit >= base) return false;
		result = result * base + digit;
		if (result > MAX_NUMBER) return false;
	}
	value = static_cast<uint32_t>(result);
	return true;
}

/**
 * @brief Copies a view into a null terminated buffer.
 * @param text View to copy.
 * @param destination Output buffer.
 * @param capacity Size of `destination` including the terminator.
 * @return False when the text is empty or does not fit.
 */
inline bool copyText(Slice text, char *destination, size_t capacity) {
	if (text.empty() || text.length() >= capacity) return false;
	for (size_t index = 0; index < text.length(); index++)
		destination[index] = text.begin[index];
	destination[text.length()] = '\0';
	return true;
}

/**
 * @brief Parses one grant such as `io:0x3F8+8`, `irq:1` or `mem:0xB8000+4000` or `spawn`.
 * @param text Grant text.
 * @param grant Receives the grant on success.
 * @return False for an unknown type, a bad number, a missing count for memory, a zero count or a range past 2^32.
 */
inline bool parseGrant(Slice text, Grant &grant) {
	if (text.equals(GRANT_SPAWN)) {
		grant.type = CapabilityType::SPAWN;
		grant.first = static_cast<uint32_t>(SPAWN_FIRST);
		grant.count = static_cast<uint32_t>(SPAWN_COUNT);
		return true;
	}
	Slice type;
	Slice range;
	if (!splitAt(text, TYPE_SEPARATOR, type, range)) return false;

	if (type.equals(TYPE_IO))
		grant.type = CapabilityType::IO_PORTS;
	else if (type.equals(TYPE_IRQ))
		grant.type = CapabilityType::IRQ;
	else if (type.equals(TYPE_MEMORY))
		grant.type = CapabilityType::PHYSICAL_MEMORY;
	else
		return false;

	Slice first;
	Slice count;
	bool hasCount = splitAt(range, COUNT_SEPARATOR, first, count);
	if (!parseNumber(first, grant.first)) return false;

	if (hasCount) {
		if (!parseNumber(count, grant.count)) return false;
	} else if (grant.type == CapabilityType::PHYSICAL_MEMORY) {
		return false;
	} else {
		grant.count = SINGLE_UNIT;
	}

	return grant.count != 0 &&
		   static_cast<uint64_t>(grant.first) + grant.count <= ADDRESS_LIMIT;
}

/**
 * @brief Parses a comma separated list of grants and appends them to an entry.
 * @param list Value of the `caps` key.
 * @param entry Entry whose grants are filled in.
 * @return `Error::NONE`, `TOO_MANY_GRANTS` or `BAD_VALUE`.
 */
inline Error parseGrants(Slice list, Entry &entry) {
	while (!list.empty()) {
		Slice item;
		Slice rest;
		splitAt(list, LIST_SEPARATOR, item, rest);
		list = rest;

		if (entry.grantCount == MAX_GRANTS) return Error::TOO_MANY_GRANTS;
		if (!parseGrant(item, entry.grants[entry.grantCount])) return Error::BAD_VALUE;
		entry.grantCount++;
	}
	return Error::NONE;
}

/**
 * @brief Parses a comma separated list of program names and appends them to the `ipc` list of an entry.
 * @param list Value of the `ipc` key.
 * @param entry Entry whose `ipc` names are filled in.
 * @return `Error::NONE`, `TOO_MANY_PEERS`, or `BAD_VALUE` for an empty or too long name or a repeated one.
 */
inline Error parseIpcNames(Slice list, Entry &entry) {
	if (list.empty()) return Error::BAD_VALUE;
	while (!list.empty()) {
		Slice item;
		Slice rest;
		bool more = splitAt(list, LIST_SEPARATOR, item, rest);
		list = rest;
		if (more && rest.empty()) return Error::BAD_VALUE;

		if (entry.ipcCount == MAX_IPC_PEERS) return Error::TOO_MANY_PEERS;
		if (!copyText(item, entry.ipc[entry.ipcCount], MAX_NAME_BYTES)) return Error::BAD_VALUE;
		for (size_t index = 0; index < entry.ipcCount; index++) {
			if (item.equals(entry.ipc[index])) return Error::BAD_VALUE;
		}
		entry.ipcCount++;
	}
	return Error::NONE;
}

/**
 * @brief Parses one non blank line into an entry.
 * @param line Trimmed line.
 * @param entry Entry to fill; `repeat` must already hold the default.
 * @return `Error::NONE` or the reason the line is wrong.
 */
inline Error parseLine(Slice line, Entry &entry) {
	Slice rest = line;
	Slice name = nextToken(rest);
	Slice nameBefore;
	Slice nameAfter;
	if (splitAt(name, KEY_SEPARATOR, nameBefore, nameAfter) ||
		!copyText(name, entry.name, MAX_NAME_BYTES))
		return Error::BAD_NAME;

	bool seenPath = false;
	bool seenPeer = false;
	bool seenRepeat = false;
	bool seenRestart = false;
	bool seenCaps = false;
	bool seenArgument = false;
	bool seenIpc = false;

	for (Slice token = nextToken(rest); !token.empty(); token = nextToken(rest)) {
		Slice key;
		Slice value;
		if (!splitAt(token, KEY_SEPARATOR, key, value)) return Error::UNKNOWN_KEY;

		if (key.equals(KEY_PATH)) {
			if (seenPath) return Error::DUPLICATE_KEY;
			seenPath = true;
			if (!copyText(value, entry.path, MAX_PATH_BYTES)) return Error::BAD_VALUE;
		} else if (key.equals(KEY_PEER)) {
			if (seenPeer) return Error::DUPLICATE_KEY;
			seenPeer = true;
			if (!copyText(value, entry.peer, MAX_NAME_BYTES)) return Error::BAD_VALUE;
		} else if (key.equals(KEY_REPEAT)) {
			if (seenRepeat) return Error::DUPLICATE_KEY;
			seenRepeat = true;
			if (!parseNumber(value, entry.repeat) || entry.repeat == 0 ||
				entry.repeat > MAX_REPEAT)
				return Error::BAD_VALUE;
		} else if (key.equals(KEY_RESTART)) {
			if (seenRestart) return Error::DUPLICATE_KEY;
			seenRestart = true;
			if (!parseNumber(value, entry.restarts) || entry.restarts == NO_RESTARTS ||
				entry.restarts > MAX_RESTARTS)
				return Error::BAD_VALUE;
		} else if (key.equals(KEY_ARGUMENT)) {
			if (seenArgument) return Error::DUPLICATE_KEY;
			seenArgument = true;
			if (!parseNumber(value, entry.argument)) return Error::BAD_VALUE;
			entry.hasArgument = true;
		} else if (key.equals(KEY_IPC)) {
			if (seenIpc) return Error::DUPLICATE_KEY;
			seenIpc = true;
			Error error = parseIpcNames(value, entry);
			if (error != Error::NONE) return error;
		} else if (key.equals(KEY_CAPS)) {
			if (seenCaps) return Error::DUPLICATE_KEY;
			seenCaps = true;
			Error error = parseGrants(value, entry);
			if (error != Error::NONE) return error;
		} else {
			return Error::UNKNOWN_KEY;
		}
	}

	if (entry.isBootModule() && entry.repeat != DEFAULT_REPEAT) return Error::BAD_VALUE;
	if (entry.restarts != NO_RESTARTS && (entry.isBootModule() || entry.repeat != DEFAULT_REPEAT))
		return Error::BAD_VALUE;
	return Error::NONE;
}

} // namespace detail

/** Parsed manifest: the entries in file order, or the line and reason of the first error. */
struct Manifest {
	/** Parsed entries; only the first `count` are valid. */
	Entry entries[MAX_ENTRIES];
	/** Number of entries. */
	size_t count;
	/** One based number of the line where parsing failed; 0 when there was no error. */
	size_t errorLine;
	/** Result of the last `parse`. */
	Error error;

	/**
	 * @brief Parses a whole file, replacing the previous content.
	 *
	 * Parsing stops at the first wrong line.
	 * @param text File content; not required to be null terminated.
	 * @param length Size of the content in bytes.
	 * @return True when the whole file is valid; false with `error` and `errorLine` set otherwise.
	 */
	bool parse(const char *text, size_t length) {
		count = 0;
		errorLine = 0;
		error = Error::NONE;

		const char *cursor = text;
		const char *end = text + length;
		size_t lineNumber = 0;
		while (cursor != end) {
			const char *lineEnd = cursor;
			while (lineEnd != end && *lineEnd != detail::NEWLINE)
				lineEnd++;
			lineNumber++;

			detail::Slice line = detail::trim({cursor, lineEnd});
			cursor = lineEnd == end ? end : lineEnd + 1;
			if (line.empty() || *line.begin == detail::COMMENT_START) continue;

			Error result = addEntry(line);
			if (result != Error::NONE) {
				error = result;
				errorLine = lineNumber;
				return false;
			}
		}
		return true;
	}

	/**
	 * @brief Looks an entry up by name.
	 * @param name Null terminated name.
	 * @return The entry, or null when there is none.
	 */
	const Entry *find(const char *name) const {
		for (size_t index = 0; index < count; index++) {
			detail::Slice known = {entries[index].name, entries[index].name};
			while (*known.end != '\0')
				known.end++;
			if (known.equals(name)) return &entries[index];
		}
		return nullptr;
	}

  private:
	/**
	 * @brief Parses a line and appends the entry.
	 * @param line Trimmed non blank line.
	 * @return `Error::NONE`, or the reason the line was refused (table full, bad line, duplicate name).
	 */
	Error addEntry(detail::Slice line) {
		if (count == MAX_ENTRIES) return Error::TOO_MANY_ENTRIES;

		Entry entry = {};
		entry.repeat = DEFAULT_REPEAT;
		Error result = detail::parseLine(line, entry);
		if (result != Error::NONE) return result;
		if (find(entry.name) != nullptr) return Error::DUPLICATE_NAME;

		entries[count++] = entry;
		return Error::NONE;
	}
};

} // namespace cock::abi::manifest

#endif // !SHARED_COCK_ABI_MANIFEST_HPP
