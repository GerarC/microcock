#include <cock/utils/panic.hpp>
#include <stdarg.h>
#include <stdio.h>

#define ETERNAL  ; ;

namespace cock::utils {
constexpr const char *KERNEL_PANIC_MESSAGE = "cock panic: ";

__attribute__((noreturn)) void panic(const char *msg, ...) {
	va_list args;
	va_start(args, msg);

	printf("cock panic: ");
	vprintf(msg, args);
	putchar('\n');
	va_end(args);

	for (ETERNAL)
		__asm__ volatile("cli; hlt");
	__builtin_unreachable();
}
} // namespace cock::utils
