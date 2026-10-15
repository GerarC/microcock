#ifndef CORE_HAL_UTILS_HPP
#define CORE_HAL_UTILS_HPP

#include <stdint.h>

namespace cock::core::hal {

void halt();
uintptr_t save_and_block_interruptions();
void restore_interruptions(uintptr_t flags);
void manual_timer();
}

#endif // !CORE_HAL_UTILS_HPP
