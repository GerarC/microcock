#ifndef CORE_HAL_UTILS_HPP
#define CORE_HAL_UTILS_HPP

namespace cock::core::hal {

void halt();
void block_interruptions();
void unblock_interruptions();
}

#endif // !CORE_HAL_UTILS_HPP
