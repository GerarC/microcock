#include <cock/utils/panic.hpp>
#include <stdio.h>

__attribute__((noreturn)) void panic(const char *msg) {
	printf("cock panic: %s\n", msg);
	for (;;)
		__asm__ volatile("cli; hlt");
	__builtin_unreachable();
}
