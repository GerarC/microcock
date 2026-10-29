#ifndef ARCH_X86_HAL_PAGE_HPP
#define ARCH_X86_HAL_PAGE_HPP

/**
 * @file
 * @brief HAL: translation granule (page size) of the architecture.
 *
 * Port requirement: define PAGE_SHIFT, PAGE_SIZE and PAGE_MASK. The core
 * allocates, maps and rounds memory in units of PAGE_SIZE.
 */

#include <stddef.h>

namespace cock::core::hal {

/** @brief log2 of the page size (12 for 4 KiB pages). */
constexpr size_t PAGE_SHIFT = 12;
/** @brief Page size in bytes. */
constexpr size_t PAGE_SIZE = size_t{1} << PAGE_SHIFT;
/** @brief Mask of the offset inside a page (PAGE_SIZE minus one). */
constexpr size_t PAGE_MASK = PAGE_SIZE - 1;

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_PAGE_HPP
