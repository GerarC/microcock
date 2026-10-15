#include "word_copy.hpp"
#include <string.h>

LIBC_KEEP_SYMBOL void *memcpy(void *__restrict dest, const void *__restrict src, size_t n) {
	using namespace libc_string;

	unsigned char *to = static_cast<unsigned char *>(dest);
	const unsigned char *from = static_cast<const unsigned char *>(src);

	if (wordAligned(to, from)) {
		while (n >= WORD_SIZE) {
			*reinterpret_cast<Word *>(to) = *reinterpret_cast<const Word *>(from);
			to += WORD_SIZE;
			from += WORD_SIZE;
			n -= WORD_SIZE;
		}
	}
	while (n-- > 0)
		*to++ = *from++;
	return dest;
}
