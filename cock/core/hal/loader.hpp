#ifndef CORE_HAL_LOADER_HPP
#define CORE_HAL_LOADER_HPP

#include <stdint.h>

namespace cock::core::hal {
uintptr_t load_executable(void *file_in_mem, uintptr_t address_space);
}

#endif
