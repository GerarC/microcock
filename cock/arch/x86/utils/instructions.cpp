#include <cock/core/hal/utils.hpp>

namespace cock::core::hal {

void halt() { __asm__ volatile("hlt"); }

uintptr_t save_and_block_interruptions() {
	uintptr_t flags;
	__asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
	return flags;
}

void restore_interruptions(uintptr_t flags) {
	__asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

void manual_timer() { __asm__ volatile("int $0x20"); }

} // namespace cock::core::hal
