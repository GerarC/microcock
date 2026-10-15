#include <stdio.h>
#if defined(__is_libk)
#include <hal/console.hpp>
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char)ic;
	cock::core::hal::console_write(&c, sizeof(c));
#endif
	return ic;
}
