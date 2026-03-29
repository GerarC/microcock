#include <cock/core/hal/utils.hpp>

namespace cock::core::hal {

void halt() { __asm__ volatile("hlt"); }

void block_interruptions() { __asm__ volatile("cli"); }

void unblock_interruptions() { __asm__ volatile("sti"); }

} // namespace cock::core::hal
