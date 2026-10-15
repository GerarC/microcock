#ifndef SHARED_COCK_ABI_MANIFEST_HPP
#define SHARED_COCK_ABI_MANIFEST_HPP

#include <cock/abi/capability.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::manifest {

constexpr const char *FILE_PATH = "manifest.conf";
constexpr size_t MAX_FILE_BYTES = 4096;
constexpr size_t MAX_ENTRIES = 32;
constexpr size_t MAX_NAME_BYTES = 32;
constexpr size_t MAX_PATH_BYTES = 64;
constexpr size_t MAX_GRANTS = 4;
constexpr uint32_t DEFAULT_REPEAT = 1;
constexpr uint32_t MAX_REPEAT = 32;

struct Grant {
	CapabilityType type;
	uint32_t first;
	uint32_t count;
};

struct Entry {
	char name[MAX_NAME_BYTES];
	char path[MAX_PATH_BYTES];
	char peer[MAX_NAME_BYTES];
	Grant grants[MAX_GRANTS];
	size_t grantCount;
	uint32_t repeat;
	uint32_t argument;
	bool hasArgument;

	bool isBootModule() const { return path[0] == '\0'; }
	bool hasPeer() const { return peer[0] != '\0'; }
};

enum class Error : uint32_t {
	NONE = 0,
	TOO_MANY_ENTRIES,
	BAD_NAME,
	DUPLICATE_NAME,
	UNKNOWN_KEY,
	DUPLICATE_KEY,
	BAD_VALUE,
	TOO_MANY_GRANTS,
};

namespace detail {

constexpr char NEWLINE = '\n';
constexpr char SPACE = ' ';
constexpr char TAB = '\t';
constexpr char CARRIAGE_RETURN = '\r';
constexpr char COMMENT_START = '#';
constexpr char KEY_SEPARATOR = '=';
constexpr char LIST_SEPARATOR = ',';
constexpr char TYPE_SEPARATOR = ':';
constexpr char COUNT_SEPARATOR = '+';
constexpr char HEX_MARKER = 'x';
constexpr char HEX_MARKER_UPPER = 'X';
constexpr char DIGIT_ZERO = '0';
constexpr char DIGIT_NINE = '9';
constexpr char LETTER_LOWER_A = 'a';
constexpr char LETTER_LOWER_F = 'f';
constexpr char LETTER_UPPER_A = 'A';
constexpr char LETTER_UPPER_F = 'F';
constexpr uint32_t DECIMAL_BASE = 10;
constexpr uint32_t HEX_BASE = 16;
constexpr size_t HEX_PREFIX_BYTES = 2;
constexpr uint32_t NOT_A_DIGIT = 0xFF;
constexpr uint32_t SINGLE_UNIT = 1;
constexpr uint64_t MAX_NUMBER = 0xFFFFFFFF;
constexpr uint64_t ADDRESS_LIMIT = uint64_t(1) << 32;

constexpr const char *KEY_PATH = "path";
constexpr const char *KEY_PEER = "peer";
constexpr const char *KEY_REPEAT = "repeat";
constexpr const char *KEY_CAPS = "caps";
constexpr const char *KEY_ARGUMENT = "arg";
constexpr const char *TYPE_IO = "io";
constexpr const char *TYPE_IRQ = "irq";
constexpr const char *TYPE_MEMORY = "mem";

struct Slice {
	const char *begin;
	const char *end;

	size_t length() const { return static_cast<size_t>(end - begin); }
	bool empty() const { return begin == end; }

	bool equals(const char *text) const {
		size_t index = 0;
		for (; index < length(); index++)
			if (text[index] == '\0' || text[index] != begin[index]) return false;
		return text[index] == '\0';
	}
};

inline bool isBlank(char character) {
	return character == SPACE || character == TAB || character == CARRIAGE_RETURN;
}

inline Slice trim(Slice slice) {
	while (slice.begin != slice.end && isBlank(*slice.begin))
		slice.begin++;
	while (slice.begin != slice.end && isBlank(*(slice.end - 1)))
		slice.end--;
	return slice;
}

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

inline Slice nextToken(Slice &rest) {
	rest = trim(rest);
	const char *cursor = rest.begin;
	while (cursor != rest.end && !isBlank(*cursor))
		cursor++;
	Slice token = {rest.begin, cursor};
	rest.begin = cursor;
	return token;
}

inline uint32_t digitValue(char character) {
	if (character >= DIGIT_ZERO && character <= DIGIT_NINE)
		return static_cast<uint32_t>(character - DIGIT_ZERO);
	if (character >= LETTER_LOWER_A && character <= LETTER_LOWER_F)
		return static_cast<uint32_t>(character - LETTER_LOWER_A) + DECIMAL_BASE;
	if (character >= LETTER_UPPER_A && character <= LETTER_UPPER_F)
		return static_cast<uint32_t>(character - LETTER_UPPER_A) + DECIMAL_BASE;
	return NOT_A_DIGIT;
}

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

inline bool copyText(Slice text, char *destination, size_t capacity) {
	if (text.empty() || text.length() >= capacity) return false;
	for (size_t index = 0; index < text.length(); index++)
		destination[index] = text.begin[index];
	destination[text.length()] = '\0';
	return true;
}

inline bool parseGrant(Slice text, Grant &grant) {
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
	bool seenCaps = false;
	bool seenArgument = false;

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
		} else if (key.equals(KEY_ARGUMENT)) {
			if (seenArgument) return Error::DUPLICATE_KEY;
			seenArgument = true;
			if (!parseNumber(value, entry.argument)) return Error::BAD_VALUE;
			entry.hasArgument = true;
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
	return Error::NONE;
}

} // namespace detail

struct Manifest {
	Entry entries[MAX_ENTRIES];
	size_t count;
	size_t errorLine;
	Error error;

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
