/**
 * @file console.hpp
 * @brief Wire protocol of the console driver and of the `tty` server, and the packing helpers for their messages.
 *
 * The console driver (`SERVER_NAME`) writes to the serial port and the screen; the `tty` server (`TTY_NAME`) holds the
 * typed line and answers `READ`. Clients call either server with a synchronous call of four words. Word 0 is the header
 * (see `pack`): operation or status in bits 0 to 7, byte count in bits 8 to 15 and the `LAST` flag in bit 16.
 * Words 1 to 3 carry up to `CHUNK_BYTES` bytes, the first byte in the low bits of word 1 (little endian).
 * The reply uses the same layout with the `Status` in place of the operation and a count in place of the
 * length. The keyboard driver (`KEYBOARD_NAME`) feeds the `tty` server with `CONSOLE_KEY` messages, and the `tty`
 * server draws the echo of the typed line through `ECHO` calls on the driver.
 */
#ifndef COCK_ABI_CONSOLE_HPP
#define COCK_ABI_CONSOLE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::console {

/** Name of the console driver (output) for the name server. */
constexpr char SERVER_NAME[] = "console";
/** Name of the `tty` server (line discipline and input) for the name server. */
constexpr char TTY_NAME[] = "tty";
/** Name of the display server the console draws on, for the name server. */
constexpr char DISPLAY_NAME[] = "vga_driver";
/** Name of the only process allowed to send `CONSOLE_KEY` to the `tty` server. */
constexpr char KEYBOARD_NAME[] = "kbd";

/** Operation code in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Driver: send 1 to 12 bytes to the serial port and the display; `LAST` marks the final chunk of one `write` call. */
	WRITE = 1,
	/** `tty`: ask for at most 1 to 12 bytes of input; the reply is held until a full line is ready. */
	READ = 2,
	/** Driver: clear the screen and put the cursor at the top; no data. */
	CLEAR = 3,
	/** Driver, only from the `tty` server: draw 1 to 12 bytes of the typed line on the screen, not on the serial port; a backspace erases. */
	ECHO = 4,
};

/** Status in the low byte of word 0 of a reply. */
enum class Status : uint32_t {
	/** The operation succeeded; the count is valid. */
	OK = 0,
	/** Unknown operation, an operation the server does not offer, or a length of 0 or above `CHUNK_BYTES`. */
	BAD_REQUEST = 1,
	/** A READ found the table of waiting readers full; the client sleeps a tick and retries. */
	BUSY = 2,
	/** The caller may not use the operation (`ECHO` from anyone but the `tty` server). */
	DENIED = 3,
};

/** Index of the header word in the four word payload. */
constexpr size_t HEADER_WORD = 0;
/** Index of the first data word. */
constexpr size_t FIRST_DATA_WORD = 1;
/** Number of data words per message. */
constexpr size_t DATA_WORDS = 3;
/** Bytes in one payload word (4). */
constexpr size_t WORD_BYTES = sizeof(uint32_t);
/** Most data bytes in one message (12). */
constexpr size_t CHUNK_BYTES = DATA_WORDS * WORD_BYTES;

/** Bits in a byte; shift of the length field in the header. */
constexpr uint32_t BYTE_BITS = 8;
/** Mask of one byte field of the header and of a byte inside a data word. */
constexpr uint32_t BYTE_MASK = 0xFF;

/** A single set bit used to build flag masks. */
constexpr uint32_t FLAG_BIT = 1;
/** Bit position of the `LAST` flag in the header. */
constexpr uint32_t LAST_BIT_SHIFT = 16;
/** Header mask of the `LAST` flag: the chunk ends one `write` call. */
constexpr uint32_t LAST_FLAG = FLAG_BIT << LAST_BIT_SHIFT;

/**
 * @brief Builds the header word of a message.
 * @param code Operation (request) or status (reply); only the low 8 bits are kept.
 * @param length Byte count or reply count; only the low 8 bits are kept.
 * @param last True to set the `LAST` flag.
 * @return The header: code | length << 8 | LAST flag.
 */
inline uint32_t pack(uint32_t code, size_t length, bool last = false) {
	return (code & BYTE_MASK) | ((static_cast<uint32_t>(length) & BYTE_MASK) << BYTE_BITS) |
		   (last ? LAST_FLAG : 0);
}

/**
 * @brief Tells whether the header has the `LAST` flag.
 * @param header Header word.
 * @return True when the chunk is the final one of a write.
 */
inline bool isLast(uint32_t header) {
	return (header & LAST_FLAG) != 0;
}

/**
 * @brief Extracts the operation or status.
 * @param header Header word.
 * @return Bits 0 to 7 of the header.
 */
inline uint32_t codeOf(uint32_t header) {
	return header & BYTE_MASK;
}

/**
 * @brief Extracts the byte count.
 * @param header Header word.
 * @return Bits 8 to 15 of the header. Not validated: the receiver must check it against `CHUNK_BYTES`.
 */
inline size_t lengthOf(uint32_t header) {
	return (header >> BYTE_BITS) & BYTE_MASK;
}

/**
 * @brief Packs bytes into the data words of a message.
 * @param words Message words; words 1 to 3 are cleared and filled, word 0 is not touched.
 * @param bytes Source bytes, at least `length` bytes.
 * @param length Number of bytes; bytes beyond `CHUNK_BYTES` are ignored.
 */
inline void packBytes(uint32_t (&words)[FIRST_DATA_WORD + DATA_WORDS], const char *bytes,
					  size_t length) {
	for (size_t index = 0; index < DATA_WORDS; index++)
		words[FIRST_DATA_WORD + index] = 0;

	for (size_t index = 0; index < length && index < CHUNK_BYTES; index++) {
		uint32_t shift = static_cast<uint32_t>(index % WORD_BYTES) * BYTE_BITS;
		words[FIRST_DATA_WORD + index / WORD_BYTES] |=
			static_cast<uint32_t>(static_cast<uint8_t>(bytes[index])) << shift;
	}
}

/**
 * @brief Unpacks the data words of a message into bytes.
 * @param words Message words; only words 1 to 3 are read.
 * @param bytes Destination, at least `length` bytes (at most `CHUNK_BYTES` are written).
 * @param length Number of bytes wanted.
 */
inline void unpackBytes(const uint32_t (&words)[FIRST_DATA_WORD + DATA_WORDS], char *bytes,
						size_t length) {
	for (size_t index = 0; index < length && index < CHUNK_BYTES; index++) {
		uint32_t shift = static_cast<uint32_t>(index % WORD_BYTES) * BYTE_BITS;
		bytes[index] = static_cast<char>((words[FIRST_DATA_WORD + index / WORD_BYTES] >> shift) &
										 BYTE_MASK);
	}
}

} // namespace cock::abi::console

#endif // !COCK_ABI_CONSOLE_HPP
