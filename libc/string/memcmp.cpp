#include "word_copy.hpp"
#include <string.h>

LIBC_KEEP_SYMBOL int memcmp(const void *aptr, const void *bptr, size_t size) {
	using namespace libc_string;

	const unsigned char *a = static_cast<const unsigned char *>(aptr);
	const unsigned char *b = static_cast<const unsigned char *>(bptr);

	if (wordAligned(a, b)) {
		while (size >= WORD_SIZE && *reinterpret_cast<const Word *>(a) ==
										*reinterpret_cast<const Word *>(b)) {
			a += WORD_SIZE;
			b += WORD_SIZE;
			size -= WORD_SIZE;
		}
	}
	for (; size > 0; size--, a++, b++) {
		if (*a < *b) return -1;
		if (*b < *a) return 1;
	}
	return 0;
}
