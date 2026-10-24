/**
 * @file names.hpp
 * @brief Wire protocol of the name server (`names`) and the packing helpers for its calls.
 *
 * A program that wants to talk to a server calls the name server (`NAMES_PID`) with synchronous calls of
 * four words. Word 0 is the header (see `pack`): the operation in bits 0 to 7, the number of the part in
 * bits 8 to 15, the `LAST` flag in bit 16 and a service label in bits 17 to 31. Words 1 to 3 carry 12 bytes
 * of a name, the first byte in the low bits of word 1 (little endian), or the numbers an operation needs.
 * A name longer than 12 bytes travels in several calls, one part each, in order; the name server answers
 * `MORE` to every part but the last and keeps the bytes meanwhile. The reply holds the `Status` in word 0,
 * the process id of the server in word 1, its service label in word 2 and, when the server has an endpoint,
 * the handle the caller now holds on it in word 3 (`NO_CAPABILITY_NODE` when it has none). The handle carries
 * the process id of the caller as its badge, so the server tells its clients apart without a process id in
 * the message.
 *
 * A name that starts with `/` is a path; any other name is short for `/run/<name>`. `CONNECT` is the only
 * operation a program uses. The other three are sent by the root server (`ROOT_PID`) while it starts the
 * system, and the name server refuses them from anyone else.
 */
#ifndef COCK_ABI_NAMES_HPP
#define COCK_ABI_NAMES_HPP

#include <cock/abi/call.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::names {

/** Operation in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Ask for the right to talk to the server with a name (words 1 to 3 of each part); the reply carries its process id. */
	CONNECT = 1,
	/** Root only: a service exists. Label in the header, name in the parts. */
	DEFINE = 2,
	/** Root only: the service with the label of the header may talk to the service with the label in word 1. */
	ALLOW = 3,
	/** Root only: the process with the id in word 1 now runs the service with the label of the header, and receives on the endpoint in word 2 (`NO_ENDPOINT` when the service has none). */
	BIND = 4,
};

/** Status in word 0 of a reply. */
enum class Status : uint32_t {
	/** Done. For `CONNECT` the process id and label of the server are valid. */
	OK = 0,
	/** No server has that name, or it is not running. A server that restarts is found again once it runs. */
	NOT_FOUND = 1,
	/** The policy of the manifest does not let the caller talk to that server. */
	DENIED = 2,
	/** The caller is not a process the root server started and registered (yet). A caller that was just started may retry. */
	UNKNOWN_CLIENT = 3,
	/** The right could not be given: the capability table of the caller or the derivation tree is full. */
	NO_ROOM = 4,
	/** Unknown operation, empty or too long name, parts out of order, a label out of range, a duplicate or a table that is full. */
	BAD_REQUEST = 5,
	/** A registration came from a process other than the root server. */
	NOT_ROOT = 6,
	/** The part was kept; send the next one. Only for a part that is not the last. */
	MORE = 7,
};

/** Number of words of a request or a reply (the synchronous call width). */
constexpr size_t CALL_WORDS = call::WORD_COUNT;
/** Index of the header word in a request, and of the status in a reply. */
constexpr size_t HEADER_WORD = 0;
/** Index of the first name word in a request. */
constexpr size_t FIRST_NAME_WORD = 1;
/** Number of name words in a request. */
constexpr size_t NAME_WORDS = CALL_WORDS - FIRST_NAME_WORD;
/** Index of the process id in a reply, and of the second number in a registration. */
constexpr size_t PID_WORD = 1;
/** Index of the service label in a reply. */
constexpr size_t LABEL_WORD = 2;
/** Index of the endpoint id in a `BIND` request. */
constexpr size_t ENDPOINT_WORD = 2;
/** Index of the handle in a reply to `CONNECT`. */
constexpr size_t HANDLE_WORD = 3;
/** Bytes in one word. */
constexpr size_t WORD_BYTES = sizeof(uint32_t);
/** Bytes of a name in one call. */
constexpr size_t PART_BYTES = NAME_WORDS * WORD_BYTES;
/** Most parts of one name. */
constexpr size_t MAX_PARTS = 5;
/** Longest name in bytes, before the `/run/` prefix is added to a short name. */
constexpr size_t MAX_NAME_BYTES = MAX_PARTS * PART_BYTES;
/** Prefix of a name that does not start with a slash. */
constexpr char RUN_PREFIX[] = "/run/";
/** Length of `RUN_PREFIX`. */
constexpr size_t RUN_PREFIX_BYTES = sizeof(RUN_PREFIX) - 1;
/** First character of a path. */
constexpr char PATH_SEPARATOR = '/';

/** Bits in a byte; shift of the part number in the header. */
constexpr uint32_t BYTE_BITS = 8;
/** Mask of one byte. */
constexpr uint32_t BYTE_MASK = 0xFF;
/** Shift of the `LAST` flag in the header. */
constexpr uint32_t LAST_SHIFT = 16;
/** Header mask of the `LAST` flag: this part ends the name, or the request has one part. */
constexpr uint32_t LAST_FLAG = uint32_t(1) << LAST_SHIFT;
/** Shift of the service label in the header. */
constexpr uint32_t LABEL_SHIFT = 17;
/** Largest service label that fits in the header (15 bits). */
constexpr uint32_t MAX_LABEL = 0x7FFF;

/**
 * @brief Builds the header word of a request.
 * @param op The operation.
 * @param label Service label, at most `MAX_LABEL`; larger values are cut.
 * @param part Number of the part of the name, from 0; only the low 8 bits are kept.
 * @param last True when this is the last part, which is the case of a request with a single part.
 * @return op | part << 8 | LAST flag | label << 17.
 */
inline uint32_t pack(Op op, uint32_t label = 0, size_t part = 0, bool last = true) {
	return (static_cast<uint32_t>(op) & BYTE_MASK) |
		   ((static_cast<uint32_t>(part) & BYTE_MASK) << BYTE_BITS) | (last ? LAST_FLAG : 0) |
		   ((label & MAX_LABEL) << LABEL_SHIFT);
}

/**
 * @brief Extracts the operation code of a header.
 * @param header Header word.
 * @return Bits 0 to 7, not validated: compare it with the `Op` values.
 */
inline uint32_t opOf(uint32_t header) {
	return header & BYTE_MASK;
}

/**
 * @brief Extracts the part number of a header.
 * @param header Header word.
 * @return Bits 8 to 15.
 */
inline size_t partOf(uint32_t header) {
	return (header >> BYTE_BITS) & BYTE_MASK;
}

/**
 * @brief Tells whether a header has the `LAST` flag.
 * @param header Header word.
 * @return true when the part is the last of its name.
 */
inline bool isLast(uint32_t header) {
	return (header & LAST_FLAG) != 0;
}

/**
 * @brief Extracts the service label of a header.
 * @param header Header word.
 * @return Bits 17 to 31.
 */
inline uint32_t labelOf(uint32_t header) {
	return header >> LABEL_SHIFT;
}

/**
 * @brief Tells how many calls a name takes.
 * @param length Length of the name in bytes.
 * @return The number of parts: 1 to `MAX_PARTS` for a length from 1 to `MAX_NAME_BYTES`; 0 for an empty
 *         name; a number above `MAX_PARTS` for a longer one, which `packPart` refuses.
 */
inline size_t partCount(size_t length) {
	return (length + PART_BYTES - 1) / PART_BYTES;
}

/**
 * @brief Writes one part of a name into words 1 to 3 of a request.
 * @param name The whole name.
 * @param length Its length in bytes, from 1 to `MAX_NAME_BYTES`.
 * @param part Number of the part, below `partCount(length)`.
 * @param words Request words; words 1 to 3 are cleared and filled, word 0 is not touched.
 * @return true when the part exists; false (and the words are cleared) for an empty or too long name or a
 *         part past the end.
 */
inline bool packPart(const char *name, size_t length, size_t part,
					 uint32_t (&words)[CALL_WORDS]) {
	for (size_t index = 0; index < NAME_WORDS; index++)
		words[FIRST_NAME_WORD + index] = 0;
	if (length == 0 || length > MAX_NAME_BYTES || part >= partCount(length)) return false;

	size_t first = part * PART_BYTES;
	for (size_t index = 0; index < PART_BYTES && first + index < length; index++) {
		uint32_t shift = static_cast<uint32_t>(index % WORD_BYTES) * BYTE_BITS;
		words[FIRST_NAME_WORD + index / WORD_BYTES] |=
			static_cast<uint32_t>(static_cast<uint8_t>(name[first + index])) << shift;
	}
	return true;
}

/**
 * @brief Reads the bytes of one part out of words 1 to 3 of a request.
 * @param words Request words; only words 1 to 3 are read.
 * @param bytes Receives the bytes, up to the first zero byte.
 * @return The number of bytes, from 0 to `PART_BYTES`.
 */
inline size_t unpackPart(const uint32_t (&words)[CALL_WORDS], char (&bytes)[PART_BYTES]) {
	size_t length = 0;
	while (length < PART_BYTES) {
		uint32_t shift = static_cast<uint32_t>(length % WORD_BYTES) * BYTE_BITS;
		char character = static_cast<char>(
			(words[FIRST_NAME_WORD + length / WORD_BYTES] >> shift) & BYTE_MASK);
		if (character == '\0') break;
		bytes[length++] = character;
	}
	return length;
}

} // namespace cock::abi::names

#endif // !COCK_ABI_NAMES_HPP
