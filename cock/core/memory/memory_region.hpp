#ifndef CORE_MEMORY_REGION_HPP
#define CORE_MEMORY_REGION_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/**
 * @brief Tracks dynamically allocated virtual memory pages for a process.
 * * Used by the Thread class for garbage collection and memory leak prevention.
 */
typedef struct memory_region_t {
	uintptr_t virt_addr;        /**< Starting virtual address of the allocated region. */
	size_t page_count;          /**< Number of contiguous 4KB pages allocated. */

    /**
     * @brief Equality operator used by LinkedList for searching and removal operations.
     */
	bool operator==(const memory_region_t &other) const {
		return virt_addr == other.virt_addr;
	}
} MemoryRegion;

} // namespace cock::core::memory

#endif // !CORE_MEMORY_REGION_HPP
