#include <stdio.h>
#if defined(__is_libk)
#include <cock/driver/vga.hpp>
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char)ic;
    driver::vga_instance.write(&c, sizeof(c));
#else
// TODO: implement stdio and write a systemcall
#endif
	return ic;
}
