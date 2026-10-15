#ifndef ARCH_X86_HAL_PAGE_HPP
#define ARCH_X86_HAL_PAGE_HPP

#include <stddef.h>

namespace cock::core::hal {

constexpr size_t PAGE_SHIFT = 12;
constexpr size_t PAGE_SIZE = size_t{1} << PAGE_SHIFT;
constexpr size_t PAGE_MASK = PAGE_SIZE - 1;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_PAGE_HPP
