#ifndef LIBC_STRING_WORD_COPY_HPP
#define LIBC_STRING_WORD_COPY_HPP

#include <stddef.h>
#include <stdint.h>

namespace libc_string {

typedef uintptr_t __attribute__((may_alias)) Word;

constexpr size_t WORD_SIZE = sizeof(Word);
constexpr uintptr_t WORD_ALIGNMENT_MASK = WORD_SIZE - 1;
constexpr unsigned MAX_BYTE_VALUE = 0xFF;
constexpr Word REPEATED_BYTE_UNIT = ~Word{0} / MAX_BYTE_VALUE;

inline bool wordAligned(const void *first, const void *second) {
	return ((reinterpret_cast<uintptr_t>(first) |
			 reinterpret_cast<uintptr_t>(second)) &
			WORD_ALIGNMENT_MASK) == 0;
}

} // namespace libc_string

#endif // !LIBC_STRING_WORD_COPY_HPP
