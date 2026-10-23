#ifndef LIBC_STRING_WORD_COPY_HPP
#define LIBC_STRING_WORD_COPY_HPP

#include <stddef.h>
#include <stdint.h>

/**
 * @file word_copy.hpp
 * @brief Word sized access helpers shared by the string functions.
 */
namespace libc_string {

/** Machine word that may alias any object, so the string functions can read memory as words. */
typedef uintptr_t __attribute__((may_alias)) Word;

/** Size of a `Word` in bytes (4 on i386). */
constexpr size_t WORD_SIZE = sizeof(Word);
/** Mask of the address bits that must be zero for word alignment. */
constexpr uintptr_t WORD_ALIGNMENT_MASK = WORD_SIZE - 1;
/** Largest value of one byte. */
constexpr unsigned MAX_BYTE_VALUE = 0xFF;
/** A word with 0x01 in every byte; multiplied by a byte it repeats that byte in all lanes. */
constexpr Word REPEATED_BYTE_UNIT = ~Word{0} / MAX_BYTE_VALUE;

/**
 * @brief Tells whether two pointers can be accessed as whole words.
 * @param first First address.
 * @param second Second address.
 * @return True when both addresses are word aligned.
 */
inline bool wordAligned(const void *first, const void *second) {
	return ((reinterpret_cast<uintptr_t>(first) |
			 reinterpret_cast<uintptr_t>(second)) &
			WORD_ALIGNMENT_MASK) == 0;
}

} // namespace libc_string

#endif // !LIBC_STRING_WORD_COPY_HPP
