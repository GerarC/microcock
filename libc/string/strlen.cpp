#include <string.h>

LIBC_KEEP_SYMBOL size_t strlen(const char *str) {
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
