#include "word_copy.hpp"
#include <string.h>

LIBC_KEEP_SYMBOL void *memmove(void *destptr, const void *srcptr, size_t size) {
	using namespace libc_string;

	unsigned char *dest = static_cast<unsigned char *>(destptr);
	const unsigned char *src = static_cast<const unsigned char *>(srcptr);

	if (dest < src) {
		if (wordAligned(dest, src)) {
			while (size >= WORD_SIZE) {
				*reinterpret_cast<Word *>(dest) =
					*reinterpret_cast<const Word *>(src);
				dest += WORD_SIZE;
				src += WORD_SIZE;
				size -= WORD_SIZE;
			}
		}
		while (size-- > 0)
			*dest++ = *src++;
		return destptr;
	}

	dest += size;
	src += size;
	if (wordAligned(dest, src)) {
		while (size >= WORD_SIZE) {
			dest -= WORD_SIZE;
			src -= WORD_SIZE;
			*reinterpret_cast<Word *>(dest) = *reinterpret_cast<const Word *>(src);
			size -= WORD_SIZE;
		}
	}
	while (size-- > 0)
		*--dest = *--src;
	return destptr;
}
