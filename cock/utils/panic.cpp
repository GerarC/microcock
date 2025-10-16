#include <cock/utils/panic.hpp>
#include <stdarg.h>
#include <stdio.h>

#define FOR_ETERNAL for (;;)

namespace cock::utils {
constexpr const char *KERNEL_PANIC_MESSAGE = "cock panic: ";

__attribute__((noreturn)) void panic(const char *msg, ...) {
	va_list args;
	va_start(args, msg);

	printf(KERNEL_PANIC_MESSAGE);
	vprintf(msg, args);
	putchar('\n');
	va_end(args);

	FOR_ETERNAL
	__asm__ volatile("cli; hlt");
	__builtin_unreachable();
}
} // namespace cock::utils
