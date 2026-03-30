#include <cock/core/hal/utils.hpp>

namespace cock::core::hal {

void halt() { __asm__ volatile("hlt"); }

void block_interruptions() { __asm__ volatile("cli"); }

void unblock_interruptions() { __asm__ volatile("sti"); }

void manual_timer() { __asm__ volatile("int $0x20"); }

} // namespace cock::core::hal
