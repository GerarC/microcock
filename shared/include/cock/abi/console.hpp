#ifndef COCK_ABI_CONSOLE_HPP
#define COCK_ABI_CONSOLE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::console {

constexpr char SERVER_NAME[] = "console";
constexpr char KEYBOARD_NAME[] = "kbd";

enum class Op : uint32_t {
	WRITE = 1,
	READ = 2,
};

enum class Status : uint32_t {
	OK = 0,
	BAD_REQUEST = 1,
};

constexpr size_t HEADER_WORD = 0;
constexpr size_t FIRST_DATA_WORD = 1;
constexpr size_t DATA_WORDS = 3;
constexpr size_t WORD_BYTES = sizeof(uint32_t);
constexpr size_t CHUNK_BYTES = DATA_WORDS * WORD_BYTES;

constexpr uint32_t BYTE_BITS = 8;
constexpr uint32_t BYTE_MASK = 0xFF;

inline uint32_t pack(uint32_t code, size_t length) {
	return (code & BYTE_MASK) | ((static_cast<uint32_t>(length) & BYTE_MASK) << BYTE_BITS);
}

inline uint32_t codeOf(uint32_t header) {
	return header & BYTE_MASK;
}

inline size_t lengthOf(uint32_t header) {
	return (header >> BYTE_BITS) & BYTE_MASK;
}

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
