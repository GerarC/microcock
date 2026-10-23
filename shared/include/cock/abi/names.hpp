/**
 * @file names.hpp
 * @brief Wire protocol of the name server (`names`) and the packing helpers for its calls.
 *
 * A program that wants to talk to a server calls the name server (`NAMES_PID`) with a synchronous call of
 * four words. Word 0 is the header (see `pack`): the operation in bits 0 to 7 and a service label in bits
 * 8 to 31. Words 1 to 3 carry a name of up to `MAX_NAME_BYTES` bytes, the first byte in the low bits of word 1
 * (little endian), or the numbers the operation needs. The reply holds the `Status` in word 0, the process
 * id of the server in word 1 and its service label in word 2.
 *
 * `CONNECT` is the only operation a program uses. The other three are sent by the root server (`ROOT_PID`)
 * while it starts the system, and the name server refuses them from anyone else.
 */
#ifndef COCK_ABI_NAMES_HPP
#define COCK_ABI_NAMES_HPP

#include <cock/abi/call.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::names {

/** Operation in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Ask for the right to talk to the server with a name (words 1 to 3); the reply carries its process id. */
	CONNECT = 1,
	/** Root only: a service exists. Label in the header, name in words 1 to 3. */
	DEFINE = 2,
	/** Root only: the service with the label of the header may talk to the service with the label in word 1. */
	ALLOW = 3,
	/** Root only: the process with the id in word 1 now runs the service with the label of the header. */
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
	/** Unknown operation, empty name, a label out of range, a duplicate or a table that is full. */
	BAD_REQUEST = 5,
	/** A registration came from a process other than the root server. */
	NOT_ROOT = 6,
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
/** Bytes in one word. */
constexpr size_t WORD_BYTES = sizeof(uint32_t);
/** Longest name in bytes; a name of exactly this length has no terminator on the wire. */
constexpr size_t MAX_NAME_BYTES = NAME_WORDS * WORD_BYTES;

/** Bits in a byte; shift of the label in the header. */
constexpr uint32_t BYTE_BITS = 8;
/** Mask of one byte. */
constexpr uint32_t BYTE_MASK = 0xFF;
/** Largest service label that fits in the header (24 bits). */
constexpr uint32_t MAX_LABEL = 0xFFFFFF;

/**
 * @brief Builds the header word of a request.
 * @param op The operation.
 * @param label Service label, at most `MAX_LABEL`; larger values are cut.
 * @return op | label << 8.
 */
inline uint32_t pack(Op op, uint32_t label = 0) {
	return (static_cast<uint32_t>(op) & BYTE_MASK) | ((label & MAX_LABEL) << BYTE_BITS);
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
 * @brief Extracts the service label of a header.
 * @param header Header word.
 * @return Bits 8 to 31.
 */
inline uint32_t labelOf(uint32_t header) {
	return header >> BYTE_BITS;
}

/**
 * @brief Writes a name into words 1 to 3 of a request.
 * @param name NUL terminated name.
 * @param words Request words; words 1 to 3 are cleared and filled, word 0 is not touched.
 * @return true when the name has 1 to `MAX_NAME_BYTES` bytes; false (and the words are cleared) for an
 *         empty or longer name.
 */
inline bool packName(const char *name, uint32_t (&words)[CALL_WORDS]) {
	for (size_t index = 0; index < NAME_WORDS; index++)
		words[FIRST_NAME_WORD + index] = 0;

	size_t length = 0;
	while (name[length] != '\0') {
		if (length == MAX_NAME_BYTES) {
			for (size_t index = 0; index < NAME_WORDS; index++)
				words[FIRST_NAME_WORD + index] = 0;
			return false;
		}
		uint32_t shift = static_cast<uint32_t>(length % WORD_BYTES) * BYTE_BITS;
		words[FIRST_NAME_WORD + length / WORD_BYTES] |=
			static_cast<uint32_t>(static_cast<uint8_t>(name[length])) << shift;
		length++;
	}
	return length != 0;
}

/**
 * @brief Reads the name out of words 1 to 3 of a request.
 * @param words Request words; only words 1 to 3 are read.
 * @param name Receives the name and a terminator.
 * @return The length of the name, from 0 (an empty name) to `MAX_NAME_BYTES`.
 */
inline size_t unpackName(const uint32_t (&words)[CALL_WORDS], char (&name)[MAX_NAME_BYTES + 1]) {
	size_t length = 0;
	while (length < MAX_NAME_BYTES) {
		uint32_t shift = static_cast<uint32_t>(length % WORD_BYTES) * BYTE_BITS;
		char character = static_cast<char>(
			(words[FIRST_NAME_WORD + length / WORD_BYTES] >> shift) & BYTE_MASK);
		if (character == '\0') break;
		name[length++] = character;
	}
	name[length] = '\0';
	return length;
}

} // namespace cock::abi::names

#endif // !COCK_ABI_NAMES_HPP
