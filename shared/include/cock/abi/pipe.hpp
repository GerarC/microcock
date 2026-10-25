/**
 * @file pipe.hpp
 * @brief Wire protocol of the pipe server and the packing helpers for its messages.
 *
 * A pipe is an object of the server named `SERVER_NAME`, not of the kernel. A client calls the server through the
 * handle on its endpoint that the name server gave it, with a synchronous call of four words, and the server tells
 * its clients apart by the badge of that handle, which is the process id. Word 0 of a request is the header (see
 * `pack`): operation in bits 0 to 7, byte count in bits 8 to 15 and the number of an end in bits 16 to 31. Words 1 to
 * 3 carry up to `CHUNK_BYTES` bytes, the first byte in the low bits of word 1 (little endian), the same layout as the
 * console. The reply has the status in bits 0 to 7 of word 0 and a count in bits 8 to 15.
 *
 * An end is a read end or a write end of one pipe and belongs to one process; its number is local to that process, so
 * a copy of a process (`fork`) can keep the numbers it stored and the server gives the copy its own ends.
 */
#ifndef COCK_ABI_PIPE_HPP
#define COCK_ABI_PIPE_HPP

#include <cock/abi/call.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::pipe {

/** Name of the pipe server for the name server. */
constexpr char SERVER_NAME[] = "pipe";

/** Operation code in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Make a pipe and give the caller its two ends; the reply carries the read end in word 1 and the write end in word 2. */
	CREATE = 1,
	/** Take 1 to `CHUNK_BYTES` bytes from the pipe of the read end in the header; the reply is held while the pipe is empty and a write end is open, and has a count of zero when it is empty and none is. */
	READ = 2,
	/** Put 1 to `CHUNK_BYTES` bytes in the pipe of the write end in the header; the reply is held while the pipe is full, and the count may be less than the length sent. */
	WRITE = 3,
	/** Close the end in the header. */
	CLOSE = 4,
};

/** Status in the low byte of word 0 of a reply. */
enum class Status : uint32_t {
	/** Done; the count is valid. */
	OK = 0,
	/** Unknown operation, or a length of 0 or above `CHUNK_BYTES` for `READ` and `WRITE`. */
	BAD_REQUEST = 1,
	/** The caller has no such end, or it is the other kind (a `READ` on a write end). */
	BAD_HANDLE = 2,
	/** The table of pipes, of ends or of the waiting callers is full. */
	NO_ROOM = 3,
	/** A `WRITE` found no read end open: nobody can ever read what is written. */
	BROKEN = 4,
	/** A `READ` or `WRITE` could not be held because the table of waiting callers is full; the client sleeps and asks again. */
	BUSY = 5,
	/** Never sent by the server: the value a client keeps when its call did not go through (no server, or it died). */
	NOT_CONNECTED = 6,
};

/** Number of words of a request or a reply (the synchronous call width). */
constexpr size_t CALL_WORDS = call::WORD_COUNT;
/** Index of the header word. */
constexpr size_t HEADER_WORD = 0;
/** Index of the first data word, and of the read end in the reply to `CREATE`. */
constexpr size_t FIRST_DATA_WORD = 1;
/** Index of the write end in the reply to `CREATE`. */
constexpr size_t SECOND_WORD = 2;
/** Number of data words per message. */
constexpr size_t DATA_WORDS = 3;
/** Bytes in one payload word. */
constexpr size_t WORD_BYTES = sizeof(uint32_t);
/** Most data bytes in one message. */
constexpr size_t CHUNK_BYTES = DATA_WORDS * WORD_BYTES;

/** Bits in a byte; shift of the length field in the header. */
constexpr uint32_t BYTE_BITS = 8;
/** Mask of one byte field of the header and of a byte inside a data word. */
constexpr uint32_t BYTE_MASK = 0xFF;
/** Shift of the end number in the header. */
constexpr uint32_t END_SHIFT = 16;
/** Mask of the end number once shifted down. */
constexpr uint32_t END_MASK = 0xFFFF;
/** Value a client keeps for an end it does not have. */
constexpr uint32_t NO_END = 0xFFFFFFFF;

/**
 * @brief Builds the header word of a message.
 * @param code Operation (request) or status (reply); only the low 8 bits are kept.
 * @param length Byte count; only the low 8 bits are kept.
 * @param end Number of the end the request is about; only the low 16 bits are kept.
 * @return The header: code, length shifted by `BYTE_BITS` and end shifted by `END_SHIFT`.
 */
inline uint32_t pack(uint32_t code, size_t length, uint32_t end = 0) {
	return (code & BYTE_MASK) | ((static_cast<uint32_t>(length) & BYTE_MASK) << BYTE_BITS) |
		   ((end & END_MASK) << END_SHIFT);
}

/**
 * @brief Extracts the operation or the status.
 * @param header Header word.
 * @return Bits 0 to 7 of the header.
 */
inline uint32_t codeOf(uint32_t header) {
	return header & BYTE_MASK;
}

/**
 * @brief Extracts the byte count.
 * @param header Header word.
 * @return Bits 8 to 15 of the header. Not validated: the receiver checks it against `CHUNK_BYTES`.
 */
inline size_t lengthOf(uint32_t header) {
	return (header >> BYTE_BITS) & BYTE_MASK;
}

/**
 * @brief Extracts the number of the end.
 * @param header Header word.
 * @return Bits 16 to 31 of the header. Not validated: the server looks it up among the ends of the caller.
 */
inline uint32_t endOf(uint32_t header) {
	return (header >> END_SHIFT) & END_MASK;
}

/**
 * @brief Packs bytes into the data words of a message.
 * @param words Message words; words 1 to 3 are cleared and filled, word 0 is not touched.
 * @param bytes Source bytes, at least `length` bytes.
 * @param length Number of bytes; bytes beyond `CHUNK_BYTES` are ignored.
 */
inline void packBytes(uint32_t (&words)[FIRST_DATA_WORD + DATA_WORDS], const uint8_t *bytes, size_t length) {
	for (size_t index = 0; index < DATA_WORDS; index++)
		words[FIRST_DATA_WORD + index] = 0;

	for (size_t index = 0; index < length && index < CHUNK_BYTES; index++) {
		uint32_t shift = static_cast<uint32_t>(index % WORD_BYTES) * BYTE_BITS;
		words[FIRST_DATA_WORD + index / WORD_BYTES] |= static_cast<uint32_t>(bytes[index]) << shift;
	}
}

/**
 * @brief Unpacks the data words of a message into bytes.
 * @param words Message words; only words 1 to 3 are read.
 * @param bytes Destination, at least `length` bytes (at most `CHUNK_BYTES` are written).
 * @param length Number of bytes wanted.
 */
inline void unpackBytes(const uint32_t (&words)[FIRST_DATA_WORD + DATA_WORDS], uint8_t *bytes, size_t length) {
	for (size_t index = 0; index < length && index < CHUNK_BYTES; index++) {
		uint32_t shift = static_cast<uint32_t>(index % WORD_BYTES) * BYTE_BITS;
		bytes[index] = static_cast<uint8_t>((words[FIRST_DATA_WORD + index / WORD_BYTES] >> shift) & BYTE_MASK);
	}
}

static_assert(CALL_WORDS == FIRST_DATA_WORD + DATA_WORDS, "the header and the data words fill the call");

} // namespace cock::abi::pipe

#endif // !COCK_ABI_PIPE_HPP
