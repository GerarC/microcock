#include <stdio.h>
#if defined(__is_libk)
#include <hal/console.hpp>
#else
#include <unistd.h>
#endif

int putchar(int ic) {
	char c = (char)ic;
#if defined(__is_libk)
	cock::core::hal::console_write(&c, sizeof(c));
#else
	write(1, &c, 1);
#endif
	return ic;
}
