#include <string.h>

void *memset(void *dest, int value, size_t size) {
	unsigned char *buf = (unsigned char *)dest;
	for (size_t i = 0; i < size; i++)
		buf[i] = (unsigned char)value;
	return dest;
}
