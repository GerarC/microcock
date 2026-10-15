#include "word_copy.hpp"
#include <string.h>

LIBC_KEEP_SYMBOL void *memset(void *dest, int value, size_t size) {
	using namespace libc_string;

	unsigned char *buffer = static_cast<unsigned char *>(dest);
	unsigned char byte = static_cast<unsigned char>(value);

	if (wordAligned(buffer, buffer)) {
		Word pattern = static_cast<Word>(byte) * REPEATED_BYTE_UNIT;
		while (size >= WORD_SIZE) {
			*reinterpret_cast<Word *>(buffer) = pattern;
			buffer += WORD_SIZE;
			size -= WORD_SIZE;
		}
	}
	while (size-- > 0)
		*buffer++ = byte;
	return dest;
}
