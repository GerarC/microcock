#include <cock/utils/panic.hpp>
#include <stdio.h>

#define ETERNAL ;;

namespace cock::utils {
constexpr const char *KERNEL_PANIC_MESSAGE = "cock panic: %s\n";

__attribute__((noreturn)) void panic(const char *msg) {
	printf(KERNEL_PANIC_MESSAGE, msg);
	for (ETERNAL)
		__asm__ volatile("cli; hlt");
	__builtin_unreachable();
}
} // namespace cock::utils
